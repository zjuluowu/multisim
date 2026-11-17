#include "capacity_probe.h"
#include "internal/common/capability.h"
#include "parameter_mock.h"
#include "sim_test.h"
namespace SimGuard
{
extern "C" unsigned int SimHostParameterReads();
struct SimTest::Environment
{
    testing::StrictMock<SimHost::MockParameterService> parameters_;
    std::unique_ptr<SimHost::ScopedParameterService> scope_;
    Environment()
    {
        // Exact counts/order are intentionally unconstrained: queries may repeat.
        EXPECT_CALL(parameters_, Read(testing::StrEq("const.telephony.slotCount"), testing::_, testing::_, testing::_))
            .Times(testing::AnyNumber())
            .WillRepeatedly([](const char *, const char *, char *value, unsigned int length)
                            { return SimHost::CopyParameterValue("1", value, length); });
        EXPECT_CALL(parameters_,
                    Read(testing::StrEq("const.booster.virtual_modem_switch"), testing::_, testing::_, testing::_))
            .Times(testing::AnyNumber())
            .WillRepeatedly([](const char *, const char *, char *value, unsigned int length)
                            { return SimHost::CopyParameterValue("false", value, length); });
        EXPECT_CALL(parameters_, Read(testing::StrEq("const.product.devicetype"), testing::_, testing::_, testing::_))
            .Times(testing::AnyNumber())
            .WillRepeatedly([](const char *, const char *, char *value, unsigned int length)
                            { return SimHost::CopyParameterValue("phone", value, length); });
        scope_ = std::make_unique<SimHost::ScopedParameterService>(parameters_);
    }
};
SimTest::SimTest() = default;
SimTest::~SimTest() = default;
void SimTest::SetUp() { environment_ = std::make_unique<Environment>(); }
void SimTest::TearDown() { environment_.reset(); }
int SimTest::ReadCapacity() { return SimHost::ObserveCapacity(); }
bool SimTest::ParameterBoundaryObserved() { return SimHostParameterReads() > 0; }
void SimTest::StartPrimary(const Card &)
{
    SkipCapability("UNSUPPORTED: Primary requires the real Core/Manager/Controller material");
}
int SimTest::SelectPrimary(const Card &)
{
    SkipCapability("UNSUPPORTED: Primary requires the real Core/Manager/Controller material");
}
void SimTest::ExpectPrimary(const Card &, const std::string &)
{
    SkipCapability("UNSUPPORTED: no Primary observation in this material");
}
void SimTest::ExpectPrimaryChanged(const Card &)
{
    SkipCapability("UNSUPPORTED: no Primary notification boundary in this material");
}
void SimTest::ExpectPrimaryResult(bool) { SkipCapability("UNSUPPORTED: no Primary request in this material"); }
void SimTest::ExpectPrimaryIdle() { SkipCapability("UNSUPPORTED: no Primary request in this material"); }

int SimTest::CardCapacity() { return ReadCapacity(); }
Card SimTest::AddCard(std::string, CardKind, State, Lock)
{
    SkipCapability("UNSUPPORTED: initialization-free count material has no card lifecycle");
}
void SimTest::Start() { SkipCapability("UNSUPPORTED: this count material deliberately does not initialize SIM"); }
Card SimTest::ExistingCard(int)
{
    SkipCapability("UNSUPPORTED: this count material deliberately does not initialize SIM");
}
State SimTest::ReadCardState(const Card &)
{
    SkipCapability("UNSUPPORTED: this count material deliberately does not initialize SIM");
}
void SimTest::ExpectReadableCard(const Card &)
{
    SkipCapability("UNSUPPORTED: this count material deliberately does not initialize SIM");
}
void SimTest::ExpectCardState(const Card &, State)
{
    SkipCapability("UNSUPPORTED: this count material deliberately does not initialize SIM");
}
void SimTest::ExpectStateNotification(const Card &, State, Lock)
{
    SkipCapability("UNSUPPORTED: this count material has no notification subscription");
}
void SimTest::ChangeCard(const Card &, State, Lock)
{
    SkipCapability("UNSUPPORTED: this count material has no mutable card resource");
}
UnlockResult SimTest::UnlockPin(const Card &, const std::string &)
{
    SkipCapability("UNSUPPORTED: this count material has no controlled PIN resource");
}
UnlockResult SimTest::UnlockPuk(const Card &, const std::string &, const std::string &)
{
    SkipCapability("UNSUPPORTED: this count material has no controlled PUK resource");
}

} // namespace SimGuard
