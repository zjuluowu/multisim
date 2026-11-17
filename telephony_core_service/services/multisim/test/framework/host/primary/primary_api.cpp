// SPDX-License-Identifier: Apache-2.0
#include "sim_protocol_test.h"
#include "primary_environment.h"
#include <future>
#include <optional>
namespace SimGuard {
using namespace OHOS::Telephony;
using namespace std::chrono_literals;
using Rig=Sim028Host::PrimaryEnvironment;
struct SimTest::Environment {
    std::unique_ptr<Rig> rig;
    std::optional<PrimaryPath> path;
    unsigned expected=6;
    struct Captured { Rig::Pending transport; RadioProtocol protocol{}; };
    std::vector<Captured> requests;
    std::future<int> caller;
    int result=0;
    void Mapping(bool flipped) {
        auto &s=*rig;
        EXPECT_CALL(*s.ril,GetRadioProtocol(testing::_,testing::_)).Times(2).WillRepeatedly([flipped](int slot,const auto &event){
            auto reply=std::make_shared<RadioProtocol>();reply->slotId=slot;reply->modemId=flipped?1-slot:slot;
            reply->technology=flipped?(slot==1?0x11:0x22):(slot==0?0x11:0x22);
            Rig::Reply({slot,event->GetOwner(),event->GetInnerEventId()},reply);return 0;
        });
        s.manager->GetRadioProtocol(0);s.manager->GetRadioProtocol(1);
        const auto deadline=std::chrono::steady_clock::now()+3s;
        while((s.manager->GetRadioProtocolSlotIdByModemId(flipped?0:1)!=1 ||
            (flipped && (s.manager->GetRadioProtocolTech(1)!=0x11 || s.manager->GetRadioProtocolTech(0)!=0x22))) && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        ASSERT_EQ(s.manager->GetRadioProtocolSlotIdByModemId(flipped?0:1),1);
        if(flipped){EXPECT_EQ(s.manager->GetRadioProtocolTech(1),0x11);EXPECT_EQ(s.manager->GetRadioProtocolTech(0),0x22);}
    }
};
SimTest::SimTest()=default;
SimTest::~SimTest()=default;
void SimTest::SetUp(){environment_=std::make_unique<Environment>();}
void SimTest::TearDown(){
    RecordProperty("business_assertions_before_cleanup",HasFailure()?"FAIL":"PASS");
    environment_.reset();
}
Card SimTest::AddCard(std::string name, CardKind kind, State state, Lock lock) {
    if(kind!=CardKind::Usim || state!=State::Locked || lock!=Lock::Pin)throw std::invalid_argument("Legacy Primary bootstrap requires controlled PIN-locked USIM");
    return {std::move(name),1,kind,"8900000000000000001"};
}
void SimTest::StartPrimary(const Card &card){
    auto &e=*environment_;
    // Legacy closure has two modem routes and one controlled PIN-locked USIM.
    if(card.physicalSlot!=1 || card.kind!=CardKind::Usim || card.iccid!="8900000000000000001")throw std::invalid_argument("Legacy Primary resource supports the declared USIM endpoint only");
    e.rig=std::make_unique<Rig>(e.path==PrimaryPath::Direct);auto &s=*e.rig;
    ASSERT_TRUE(s.StartLockedCard());bool present=false;
    EXPECT_EQ(s.manager->HasSimCard(card.physicalSlot,present),TELEPHONY_ERR_SUCCESS);
    ASSERT_TRUE(present);ASSERT_TRUE(s.manager->IsSimActive(card.physicalSlot));
    if(e.path!=PrimaryPath::Direct)e.Mapping(!e.path.has_value());
    s.ClearObservations();s.Permit();
    EXPECT_CALL(*s.ril,SetPrimarySlot(testing::_,testing::_)).Times(e.path==PrimaryPath::Direct?1:0)
        .WillRepeatedly([&e](int slot,const auto &event){
            EXPECT_EQ(slot,1);EXPECT_TRUE(std::dynamic_pointer_cast<MultiSimController>(event->GetOwner()));
            auto &s=*e.rig;std::lock_guard<std::mutex> lock(s.mutex);
            s.observation.Add("ril_primary",{{"slot",std::to_string(slot)}});
            e.requests.push_back({{slot,event->GetOwner(),event->GetInnerEventId()}, {}});s.changed.notify_all();return 0;
        });
    EXPECT_CALL(*s.ril,SetRadioProtocol(testing::_,testing::_,testing::_)).Times(e.path==PrimaryPath::ThreePhase?e.expected:0)
        .WillRepeatedly([&e](int slot,RadioProtocol protocol,const auto &event){
            EXPECT_EQ(protocol.slotId,slot);EXPECT_TRUE(std::dynamic_pointer_cast<RadioProtocolController>(event->GetOwner()));
            if(e.expected==24 && static_cast<int>(protocol.phase)==4)EXPECT_EQ(protocol.status,RadioProtocolStatus::RADIO_PROTOCOL_STATUS_FAIL);
            auto &s=*e.rig;std::lock_guard<std::mutex> lock(s.mutex);
            s.observation.Add("ril_protocol",{{"slot",std::to_string(slot)},{"session",std::to_string(protocol.sessionId)},{"phase",std::to_string(static_cast<int>(protocol.phase))},{"modem",std::to_string(protocol.modemId)},{"technology",std::to_string(protocol.technology)}});
            e.requests.push_back({{slot,event->GetOwner(),event->GetInnerEventId()},protocol});s.changed.notify_all();return 0;
        });
}
int SimTest::SelectPrimary(const Card &card){auto &e=*environment_;return e.result=e.rig->SetPrimarySlotId(card.physicalSlot);}
void SimTest::ExpectPrimaryResult(bool success){EXPECT_EQ(environment_->result,success?TELEPHONY_ERR_SUCCESS:TELEPHONY_ERR_LOCAL_PTR_NULL);}
void SimTest::ExpectPrimaryIdle(){
    auto &s=*environment_->rig;const auto deadline=std::chrono::steady_clock::now()+3s;
    while(s.manager->IsSetPrimarySlotIdInProgress() && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    EXPECT_FALSE(s.manager->IsSetPrimarySlotIdInProgress());
}
void SimTest::ExpectPrimary(const Card &card,const std::string &identity){
    auto &s=*environment_->rig;std::lock_guard<std::mutex> lock(s.mutex);
    EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"],std::to_string(card.physicalSlot))<<"PRIMARY_COMMIT_TARGET";
    EXPECT_EQ(s.parametersDb["persist.telephony.MainCellularDataSlotId"],std::to_string(card.physicalSlot));
    if(!identity.empty())EXPECT_EQ(s.parametersDb["persist.telephony.MainCard.Iccid"],identity);
}
void SimTest::ExpectPrimaryUnchanged(int previous,const std::string &identity){
    auto &s=*environment_->rig;std::lock_guard<std::mutex> lock(s.mutex);
    EXPECT_EQ(s.parametersDb["persist.telephony.MainSlotId"],std::to_string(previous));
    EXPECT_NE(s.parametersDb["persist.telephony.MainCellularDataSlotId"],"1");
    if(!identity.empty())EXPECT_EQ(s.parametersDb["persist.telephony.MainCard.Iccid"],identity)<<"ROLLBACK_ICCID";
}
void SimTest::ExpectPrimaryNotifications(bool committed,unsigned requests,bool direct){
    auto &s=*environment_->rig;std::lock_guard<std::mutex> lock(s.mutex);s.ExpectNotifications(committed,requests,direct);
}
void PrimaryProtocol::Configure(PrimaryPath path,unsigned expected){auto &e=*test_.environment_;if(e.rig)throw std::logic_error("Configure before initialization");e.path=path;e.expected=expected;}
void PrimaryProtocol::SetPreviousPrimary(int slot,const std::string &identity){auto &s=*test_.environment_->rig;std::lock_guard<std::mutex> lock(s.mutex);s.parametersDb["persist.telephony.MainSlotId"]=std::to_string(slot);if(!identity.empty())s.parametersDb["persist.telephony.MainCard.Iccid"]=identity;}
void PrimaryProtocol::ExpectUncommitted(const Card &card){auto &s=*test_.environment_->rig;std::lock_guard<std::mutex> lock(s.mutex);EXPECT_NE(s.parametersDb["persist.telephony.MainSlotId"],std::to_string(card.physicalSlot))<<"NO_EARLY_COMMIT";}
void PrimaryProtocol::Begin(const Card &card){auto &e=*test_.environment_;e.caller=std::async(std::launch::async,[&e,card]{return e.rig->SetPrimarySlotId(card.physicalSlot);});}
std::vector<PrimaryRequest> PrimaryProtocol::WaitRequests(size_t count,std::chrono::milliseconds timeout){
    auto &e=*test_.environment_;auto &s=*e.rig;std::unique_lock<std::mutex> lock(s.mutex);
    if(!s.changed.wait_for(lock,timeout,[&]{return e.requests.size()>=count;}))throw std::runtime_error("Primary protocol request deadline exceeded");
    std::vector<PrimaryRequest> out;for(size_t i=0;i<e.requests.size();++i){const auto &r=e.requests[i];out.push_back({i,r.transport.slot,r.protocol.sessionId,static_cast<int>(r.protocol.phase),r.protocol.modemId,r.protocol.technology});}return out;
}
void PrimaryProtocol::ExpectPending(std::chrono::milliseconds duration){auto &e=*test_.environment_;EXPECT_EQ(e.caller.wait_for(duration),std::future_status::timeout);EXPECT_TRUE(e.rig->manager->IsSetPrimarySlotIdInProgress());}
void PrimaryProtocol::Reply(const PrimaryRequest &request,int delta){
    auto &e=*test_.environment_;SimTest::Environment::Captured r;{std::lock_guard<std::mutex> lock(e.rig->mutex);r=e.requests.at(request.token);}
    if(e.path==PrimaryPath::Direct){auto reply=std::make_shared<RadioResponseInfo>();reply->error=ErrType::NONE;e.rig->observation.Add("ril_response",{{"slot",std::to_string(r.transport.slot)},{"error",std::to_string(static_cast<int>(reply->error))}});Rig::Reply(r.transport,reply);}
    else{auto reply=std::make_shared<RadioProtocol>(r.protocol);reply->sessionId+=delta;e.rig->observation.Add("ril_response",{{"slot",std::to_string(r.transport.slot)},{"session",std::to_string(reply->sessionId)},{"phase",std::to_string(static_cast<int>(reply->phase))}});Rig::Reply(r.transport,reply);}
}
void PrimaryProtocol::Notify(const PrimaryRequest &request){
    auto &e=*test_.environment_;SimTest::Environment::Captured r;Rig::Pending pending;
    {std::lock_guard<std::mutex> lock(e.rig->mutex);r=e.requests.at(request.token);pending={r.transport.slot,e.rig->notifications.at({r.transport.slot,RADIO_SIM_RADIO_PROTOCOL_NOTIFY}),RADIO_SIM_RADIO_PROTOCOL_NOTIFY};}
    auto reply=std::make_shared<RadioProtocol>(r.protocol);reply->phase=RadioProtocolPhase::RADIO_PROTOCOL_PHASE_NOTIFY;reply->status=RadioProtocolStatus::RADIO_PROTOCOL_STATUS_SUCCESS;
    e.rig->observation.Add("ril_response",{{"slot",std::to_string(r.transport.slot)},{"session",std::to_string(reply->sessionId)},{"phase",std::to_string(static_cast<int>(reply->phase))}});Rig::Reply(pending,reply);
}
void PrimaryProtocol::Reject(const PrimaryRequest &request){
    auto &e=*test_.environment_;SimTest::Environment::Captured r;{std::lock_guard<std::mutex> lock(e.rig->mutex);r=e.requests.at(request.token);}
    auto reply=std::make_shared<RadioResponseInfo>();reply->error=ErrType::ERR_GENERIC_FAILURE;
    SimGuard::Primary::Fields fields{{"slot",std::to_string(r.transport.slot)},{"error",std::to_string(static_cast<int>(reply->error))}};
    if(e.path==PrimaryPath::ThreePhase){fields["session"]=std::to_string(r.protocol.sessionId);fields["phase"]=std::to_string(static_cast<int>(r.protocol.phase));}
    e.rig->observation.Add("ril_response",fields);Rig::Reply(r.transport,reply);
}
void PrimaryProtocol::Finish(std::chrono::milliseconds timeout){auto &e=*test_.environment_;if(e.caller.valid()){ASSERT_EQ(e.caller.wait_for(timeout),std::future_status::ready);e.result=e.caller.get();}}
void PrimaryProtocol::ExpectRoute(const Card &card){EXPECT_EQ(test_.environment_->rig->manager->GetRadioProtocolSlotIdByModemId(0),card.physicalSlot);}
}
