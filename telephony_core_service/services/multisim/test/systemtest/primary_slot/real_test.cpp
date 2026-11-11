#include <gmock/gmock.h>
#include "sim_manager.h"
#include "core_service_sim.h"
#include "parameter_mock.h"
#include "security_boundary.h"
#include "external_services_mock.h"
#include "ffrt_runtime.h"
#include "real_scenario.h"
#include <future>
#include <optional>
namespace {
using namespace OHOS::Telephony;
using namespace std::chrono_literals;
TEST(RealSimPrimary, InitializesRealManagerAndControllerWithoutRil)
{
    testing::StrictMock<SimHost::MockParameterService> parameters;
    EXPECT_CALL(parameters, Read(testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly([](const char *key, const char *fallback, char *out, unsigned size) {
            return SimHost::CopyParameterValue(std::string(key) == "const.telephony.slotCount" ? "2" : fallback, out, size);
        });
    SimHost::ScopedParameterService installed(parameters);
    testing::StrictMock<PrimarySlotHost::ExternalIdentity> identity;
    PrimarySlotHost::IdentityScope security(identity);
    testing::StrictMock<SimHost::MockExternalServices> services;
    EXPECT_CALL(services, Publish(testing::_, testing::_)).WillRepeatedly(testing::Return(true));
    EXPECT_CALL(services, CreateDataShare(testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::Return(nullptr)); // explicit unavailable Provider; never fabricated success.
    SimHost::ScopedExternalServices external(services);
    auto manager = std::make_shared<SimManager>(nullptr);
    ASSERT_TRUE(manager->OnInit(SIM_SLOT_COUNT_MD + 1));
    EXPECT_EQ(manager->GetMaxSimCount(), 2);
    EXPECT_EQ(manager->SetPrimarySlotId(1, true), TELEPHONY_ERR_NO_SIM_CARD);
    EXPECT_FALSE(manager->IsSetPrimarySlotIdInProgress());
    SimHost::StopCallingThreadTasks();
    manager.reset();
    EXPECT_TRUE(SimHost::AwaitSchedulerStopped(3s));
}
TEST(RealSimPrimary, C0CoreToRealManagerAndController)
{
    Sim028Host::RealScenario scenario;
    ASSERT_TRUE(scenario.StartLockedCard());
    bool present = false;
    EXPECT_EQ(scenario.manager->HasSimCard(1, present), TELEPHONY_ERR_SUCCESS);
    ASSERT_TRUE(present);
    ASSERT_TRUE(scenario.manager->IsSimActive(1));
    // C0 is backed by an actual device-query reply, rather than assuming the
    // controller's initial modemId value represents the current mapping.
    EXPECT_CALL(*scenario.ril, GetRadioProtocol(testing::_, testing::_)).Times(2)
        .WillRepeatedly([](int slot, const auto &event) {
            auto reported = std::make_shared<RadioProtocol>();
            reported->slotId = slot; reported->modemId = 1 - slot;
            reported->technology = slot == 1 ? 0x11 : 0x22;
            Sim028Host::RealScenario::Reply({slot, event->GetOwner(), event->GetInnerEventId()}, reported);
            return 0;
        });
    scenario.manager->GetRadioProtocol(0); scenario.manager->GetRadioProtocol(1);
    const auto mappingDeadline = std::chrono::steady_clock::now() + 3s;
    while ((scenario.manager->GetRadioProtocolSlotIdByModemId(0) != 1 ||
        scenario.manager->GetRadioProtocolTech(1) != 0x11 || scenario.manager->GetRadioProtocolTech(0) != 0x22) &&
        std::chrono::steady_clock::now() < mappingDeadline) std::this_thread::sleep_for(1ms);
    ASSERT_EQ(scenario.manager->GetRadioProtocolSlotIdByModemId(0), 1);
    ASSERT_EQ(scenario.manager->GetRadioProtocolTech(1), 0x11);
    ASSERT_EQ(scenario.manager->GetRadioProtocolTech(0), 0x22);
    scenario.ClearObservations();
    scenario.Permit();
    EXPECT_CALL(*scenario.ril, SetPrimarySlot(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*scenario.ril, SetRadioProtocol(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_EQ(scenario.core.SetPrimarySlotId(1), TELEPHONY_ERR_SUCCESS);
    EXPECT_FALSE(scenario.manager->IsSetPrimarySlotIdInProgress());
    std::lock_guard<std::mutex> lock(scenario.mutex);
    EXPECT_EQ(scenario.parametersDb["persist.telephony.MainSlotId"], "1") << "PRIMARY_COMMIT_TARGET";
    EXPECT_EQ(scenario.parametersDb["persist.telephony.MainCellularDataSlotId"], "1");
    EXPECT_EQ(scenario.parametersDb["persist.telephony.MainCard.Iccid"], "AB2FA72FFEABDAB4E56CB9230C9A119AB236ECE269144D4E335A36E2D26558F2");
    scenario.ExpectNotifications(true, 0, false);
    testing::Test::RecordProperty("business_assertions_before_cleanup", testing::Test::HasFailure() ? "FAIL" : "PASS");
}
TEST(RealSimPrimary, BDirectRilSuccessCommitsAfterReply)
{
    Sim028Host::RealScenario s(true);
    ASSERT_TRUE(s.StartLockedCard());
    s.ClearObservations(); s.Permit();
    using Pending = Sim028Host::RealScenario::Pending;
    std::optional<Pending> request;
    EXPECT_CALL(*s.ril, SetPrimarySlot(1, testing::_)).Times(1).WillOnce([&](int slot, const auto &event) {
        EXPECT_TRUE(std::dynamic_pointer_cast<MultiSimController>(event->GetOwner()));
        std::lock_guard<std::mutex> lock(s.mutex);
        request = Pending{slot, event->GetOwner(), event->GetInnerEventId()}; s.changed.notify_all(); return 0;
    });
    EXPECT_CALL(*s.ril, SetRadioProtocol(testing::_, testing::_, testing::_)).Times(0);
    auto caller = std::async(std::launch::async, [&] { return s.core.SetPrimarySlotId(1); });
    {
        std::unique_lock<std::mutex> lock(s.mutex);
        ASSERT_TRUE(s.changed.wait_for(lock, 3s, [&] { return request.has_value(); }));
        EXPECT_NE(s.parametersDb["persist.telephony.MainSlotId"], "1") << "NO_EARLY_COMMIT";
    }
    EXPECT_EQ(caller.wait_for(50ms), std::future_status::timeout);
    EXPECT_TRUE(s.manager->IsSetPrimarySlotIdInProgress());
    auto reply = std::make_shared<RadioResponseInfo>(); reply->error = ErrType::NONE;
    Sim028Host::RealScenario::Reply(*request, reply);
    ASSERT_EQ(caller.wait_for(3s), std::future_status::ready);
    EXPECT_EQ(caller.get(), TELEPHONY_ERR_SUCCESS);
    EXPECT_FALSE(s.manager->IsSetPrimarySlotIdInProgress());
    std::lock_guard<std::mutex> lock(s.mutex);
    EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"], "1") << "PRIMARY_COMMIT_TARGET";
    EXPECT_EQ(s.parametersDb["persist.telephony.MainCellularDataSlotId"], "1");
    s.ExpectNotifications(true, 1, true);
    testing::Test::RecordProperty("business_assertions_before_cleanup", testing::Test::HasFailure() ? "FAIL" : "PASS");
}
TEST(RealSimPrimary, BDirectRilFailureKeepsPreviousPrimary)
{
    Sim028Host::RealScenario s(true);
    ASSERT_TRUE(s.StartLockedCard()); s.ClearObservations(); s.Permit();
    { std::lock_guard<std::mutex> lock(s.mutex); s.parametersDb["persist.telephony.MainSlotId"] = "0"; }
    EXPECT_CALL(*s.ril, SetPrimarySlot(1, testing::_)).Times(1).WillOnce([](int slot, const auto &event) {
        auto reply = std::make_shared<RadioResponseInfo>(); reply->error = ErrType::ERR_GENERIC_FAILURE;
        Sim028Host::RealScenario::Reply({slot, event->GetOwner(), event->GetInnerEventId()}, reply); return 0;
    });
    EXPECT_CALL(*s.ril, SetRadioProtocol(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_EQ(s.core.SetPrimarySlotId(1), TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_FALSE(s.manager->IsSetPrimarySlotIdInProgress());
    std::lock_guard<std::mutex> lock(s.mutex);
    EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"], "0") << "FAILURE_PRESERVES_PRIMARY";
    EXPECT_NE(s.parametersDb["persist.telephony.MainCellularDataSlotId"], "1");
    s.ExpectNotifications(false, 1, true);
    testing::Test::RecordProperty("business_assertions_before_cleanup", testing::Test::HasFailure() ? "FAIL" : "PASS");
}
TEST(RealSimPrimary, C1ThreePhasesWithStaleCheckResponseRejected)
{
    using namespace Sim028Host;
    RealScenario s;
    ASSERT_TRUE(s.StartLockedCard()); s.ClearObservations(); s.Permit();
    EXPECT_CALL(*s.ril, SetPrimarySlot(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*s.ril, GetRadioProtocol(testing::_, testing::_)).Times(2).WillRepeatedly([](int slot, const auto &event) {
        auto response = std::make_shared<RadioProtocol>();
        response->slotId = slot; response->modemId = slot; response->technology = slot == 0 ? 0x11 : 0x22;
        RealScenario::Reply({slot, event->GetOwner(), event->GetInnerEventId()}, response); return 0;
    });
    s.manager->GetRadioProtocol(0); s.manager->GetRadioProtocol(1);
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (s.manager->GetRadioProtocolSlotIdByModemId(1) != 1 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    ASSERT_EQ(s.manager->GetRadioProtocolSlotIdByModemId(1), 1);
    struct ProtocolRequest { RealScenario::Pending transport; RadioProtocol protocol; };
    std::vector<ProtocolRequest> requests;
    EXPECT_CALL(*s.ril, SetRadioProtocol(testing::_, testing::_, testing::_)).Times(6)
        .WillRepeatedly([&](int slot, RadioProtocol protocol, const auto &event) {
            std::cout << "TRACE radio slot=" << slot << " session=" << protocol.sessionId
                << " phase=" << static_cast<int>(protocol.phase) << " modem=" << protocol.modemId << "\n";
            EXPECT_EQ(protocol.slotId, slot);
            EXPECT_TRUE(std::dynamic_pointer_cast<RadioProtocolController>(event->GetOwner()));
            std::lock_guard<std::mutex> lock(s.mutex);
            requests.push_back({{slot, event->GetOwner(), event->GetInnerEventId()}, protocol});
            s.changed.notify_all(); return 0;
        });
    auto caller = std::async(std::launch::async, [&] { return s.core.SetPrimarySlotId(1); });
    int session = -1;
    for (unsigned stage = 0; stage < 3; ++stage) {
        std::vector<ProtocolRequest> pair;
        {
            std::unique_lock<std::mutex> lock(s.mutex);
            ASSERT_TRUE(s.changed.wait_for(lock, 3s, [&] { return requests.size() >= 2 * (stage + 1); }));
            pair.assign(requests.begin() + stage * 2, requests.begin() + stage * 2 + 2);
        }
        EXPECT_EQ(caller.wait_for(10ms), std::future_status::timeout);
        for (const auto &request : pair) {
            auto reply = std::make_shared<RadioProtocol>(request.protocol);
            if (session == -1) session = reply->sessionId;
            EXPECT_EQ(reply->sessionId, session);
            EXPECT_EQ(static_cast<int>(reply->phase), stage == 0 ? 1 : stage == 1 ? 2 : 4);
            EXPECT_EQ(reply->modemId, stage == 0 ? request.transport.slot : 1 - request.transport.slot);
            EXPECT_EQ(reply->technology, stage == 0 ? (request.transport.slot == 0 ? 0x11 : 0x22) :
                (request.transport.slot == 0 ? 0x22 : 0x11));
            if (stage == 0 && request.transport.slot == 0) {
                auto stale = std::make_shared<RadioProtocol>(*reply); stale->sessionId += 99;
                RealScenario::Reply(request.transport, stale);
                EXPECT_EQ(caller.wait_for(20ms), std::future_status::timeout);
                std::lock_guard<std::mutex> lock(s.mutex); EXPECT_EQ(requests.size(), 2u);
            }
            RealScenario::Reply(request.transport, reply);
            if (stage == 1) {
                reply = std::make_shared<RadioProtocol>(*reply);
                reply->phase = RadioProtocolPhase::RADIO_PROTOCOL_PHASE_NOTIFY;
                reply->status = RadioProtocolStatus::RADIO_PROTOCOL_STATUS_SUCCESS;
                std::weak_ptr<AppExecFwk::EventHandler> owner;
                { std::lock_guard<std::mutex> lock(s.mutex); owner = s.notifications.at({request.transport.slot, RADIO_SIM_RADIO_PROTOCOL_NOTIFY}); }
                RealScenario::Reply({request.transport.slot, owner, RADIO_SIM_RADIO_PROTOCOL_NOTIFY}, reply);
            }
        }
    }
    ASSERT_EQ(caller.wait_for(3s), std::future_status::ready);
    EXPECT_EQ(caller.get(), TELEPHONY_ERR_SUCCESS);
    EXPECT_EQ(s.manager->GetRadioProtocolSlotIdByModemId(0), 1);
    EXPECT_FALSE(s.manager->IsSetPrimarySlotIdInProgress());
    std::lock_guard<std::mutex> lock(s.mutex);
    EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"], "1") << "PRIMARY_COMMIT_TARGET";
    EXPECT_EQ(s.parametersDb["persist.telephony.MainCellularDataSlotId"], "1");
    s.ExpectNotifications(true, 1, false);
    testing::Test::RecordProperty("business_assertions_before_cleanup", testing::Test::HasFailure() ? "FAIL" : "PASS");
}
TEST(RealSimPrimary, BRealTimeoutKeepsPreviousPrimary)
{
    Sim028Host::RealScenario s(true);
    ASSERT_TRUE(s.StartLockedCard()); s.ClearObservations(); s.Permit();
    { std::lock_guard<std::mutex> lock(s.mutex); s.parametersDb["persist.telephony.MainSlotId"] = "0"; }
    EXPECT_CALL(*s.ril, SetPrimarySlot(1, testing::_)).Times(1).WillOnce([](int, const auto &) {
        return 0; // device accepted the request but supplies no reply; real 45-second timer must end it
    });
    EXPECT_CALL(*s.ril, SetRadioProtocol(testing::_, testing::_, testing::_)).Times(0);
    const auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(s.core.SetPrimarySlotId(1), TELEPHONY_ERR_LOCAL_PTR_NULL);
    EXPECT_GE(std::chrono::steady_clock::now() - start, 44s);
    EXPECT_FALSE(s.manager->IsSetPrimarySlotIdInProgress());
    std::lock_guard<std::mutex> lock(s.mutex);
    EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"], "0") << "TIMEOUT_PRESERVES_PRIMARY";
    EXPECT_NE(s.parametersDb["persist.telephony.MainCellularDataSlotId"], "1");
    s.ExpectNotifications(false, 1, true);
    testing::Test::RecordProperty("business_assertions_before_cleanup", testing::Test::HasFailure() ? "FAIL" : "PASS");
}
TEST(RealSimPrimary, C1CheckFailureRestoresParametersAndExhaustsFiveRetries)
{
    using namespace Sim028Host;
    RealScenario s;
    ASSERT_TRUE(s.StartLockedCard()); s.ClearObservations(); s.Permit();
    { std::lock_guard<std::mutex> lock(s.mutex);
      s.parametersDb["persist.telephony.MainSlotId"] = "0";
      s.parametersDb["persist.telephony.MainCard.Iccid"] = "previous-parameter-value"; }
    EXPECT_CALL(*s.ril, SetPrimarySlot(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*s.ril, GetRadioProtocol(testing::_, testing::_)).Times(2).WillRepeatedly([](int slot, const auto &event) {
        auto response = std::make_shared<RadioProtocol>(); response->slotId = slot;
        response->modemId = slot; response->technology = slot == 0 ? 0x11 : 0x22;
        RealScenario::Reply({slot, event->GetOwner(), event->GetInnerEventId()}, response); return 0;
    });
    s.manager->GetRadioProtocol(0); s.manager->GetRadioProtocol(1);
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (s.manager->GetRadioProtocolSlotIdByModemId(1) != 1 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    ASSERT_EQ(s.manager->GetRadioProtocolSlotIdByModemId(1), 1);
    std::map<int, std::vector<std::pair<int, RadioProtocolPhase>>> phases;
    unsigned requests = 0;
    EXPECT_CALL(*s.ril, SetRadioProtocol(testing::_, testing::_, testing::_)).Times(24)
        .WillRepeatedly([&](int slot, RadioProtocol protocol, const auto &event) {
            std::cout << "TRACE radio slot=" << slot << " session=" << protocol.sessionId
                << " phase=" << static_cast<int>(protocol.phase) << " modem=" << protocol.modemId << "\n";
            EXPECT_EQ(protocol.slotId, slot); EXPECT_EQ(protocol.modemId, slot);
            EXPECT_THAT(protocol.phase, testing::AnyOf(RadioProtocolPhase::RADIO_PROTOCOL_PHASE_CHECK,
                RadioProtocolPhase::RADIO_PROTOCOL_PHASE_COMPLETE)); // no UPDATE after failed CHECK
            {
                std::lock_guard<std::mutex> lock(s.mutex);
                phases[protocol.sessionId].emplace_back(slot, protocol.phase); ++requests; s.changed.notify_all();
            }
            if (protocol.phase == RadioProtocolPhase::RADIO_PROTOCOL_PHASE_CHECK) {
                auto response = std::make_shared<RadioResponseInfo>(); response->error = ErrType::ERR_GENERIC_FAILURE;
                RealScenario::Reply({slot, event->GetOwner(), event->GetInnerEventId()}, response);
            } else {
                EXPECT_EQ(protocol.status, RadioProtocolStatus::RADIO_PROTOCOL_STATUS_FAIL);
                RealScenario::Reply({slot, event->GetOwner(), event->GetInnerEventId()}, std::make_shared<RadioProtocol>(protocol));
            }
            return 0;
        });
    EXPECT_EQ(s.core.SetPrimarySlotId(1), TELEPHONY_ERR_LOCAL_PTR_NULL);
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"], "0") << "ROLLBACK_SLOT";
        EXPECT_EQ(s.parametersDb["persist.telephony.MainCard.Iccid"], "previous-parameter-value") << "ROLLBACK_ICCID";
        EXPECT_NE(s.parametersDb["persist.telephony.MainCellularDataSlotId"], "1");
    }
    {
        std::unique_lock<std::mutex> lock(s.mutex);
        ASSERT_TRUE(s.changed.wait_for(lock, 12s, [&] { return requests == 24; }));
        EXPECT_EQ(phases.size(), 6u); // original plus the five retries, each with a fresh session
        for (const auto &[session, sequence] : phases) {
            EXPECT_THAT(sequence, testing::ElementsAre(std::make_pair(0, RadioProtocolPhase::RADIO_PROTOCOL_PHASE_CHECK),
                std::make_pair(1, RadioProtocolPhase::RADIO_PROTOCOL_PHASE_CHECK),
                std::make_pair(0, RadioProtocolPhase::RADIO_PROTOCOL_PHASE_COMPLETE),
                std::make_pair(1, RadioProtocolPhase::RADIO_PROTOCOL_PHASE_COMPLETE)));
        }
    }
    const auto done = std::chrono::steady_clock::now() + 3s;
    while (s.manager->IsSetPrimarySlotIdInProgress() && std::chrono::steady_clock::now() < done)
        std::this_thread::sleep_for(1ms);
    EXPECT_FALSE(s.manager->IsSetPrimarySlotIdInProgress());
    std::lock_guard<std::mutex> lock(s.mutex);
    EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"], "0") << "ROLLBACK_SLOT";
    EXPECT_EQ(s.parametersDb["persist.telephony.MainCard.Iccid"], "previous-parameter-value") << "ROLLBACK_ICCID";
    s.ExpectNotifications(false, 6, false);
    testing::Test::RecordProperty("business_assertions_before_cleanup", testing::Test::HasFailure() ? "FAIL" : "PASS");
}
}
int main(int argc, char **argv)
{
    testing::InitGoogleMock(&argc, argv);
    // Full initialization currently retains production subscription cycles.
    // Never reuse those live callbacks with another scenario's mock lifetime.
    const auto filter = testing::GTEST_FLAG(filter);
    if (!testing::GTEST_FLAG(list_tests) && (filter.find('*') != std::string::npos ||
        filter.find(':') != std::string::npos || filter.find('?') != std::string::npos)) {
        std::cerr << "Use run_primary_real.py or host.py: each real scenario requires its own process.\n";
        return 2;
    }
    return RUN_ALL_TESTS();
}
