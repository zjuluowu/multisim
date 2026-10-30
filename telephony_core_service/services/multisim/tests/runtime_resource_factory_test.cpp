/*
 * Copyright (c) 2026 multisim contributors.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// 主机协议测试：无生产设备/服务成功桩；opaque句柄只检查控制块及发布顺序，不调用平台方法。
#include <cassert>
#include <iostream>
#include "assembly/manager_publication.h"
#include "runtime/resource/resource_barrier.h"
#include "runtime/session/session_registry.h"
#include "runtime/result_sinks.h"

using namespace OHOS::Telephony;
using namespace OHOS::Telephony::MultiSim;

namespace {
StepRef Step(uint64_t request, uint32_t attempt = 1) { return {{request}, 1, attempt}; }

void ResourceScenarios()
{
    const ResourceKey modem {"shared-modem"};
    const ResourceKey euicc {"same-physical-euicc-for-both-file-views"};
    ResourceBarrier barrier;
    assert(!barrier.Acquire({modem}, Step(1), ResourceUse::Mutation));
    const auto modemCheck = barrier.TrackResource(modem);
    const auto euiccCheck = barrier.TrackResource(euicc);
    assert(!barrier.AcceptInitialEvidence(modemCheck, {false, true, true}));
    assert(barrier.AcceptInitialEvidence(modemCheck, {true, true, true}));
    // 全资源预留原子：第二个仍Checking时不能局部占用第一个。
    assert(!barrier.Acquire({modem, euicc}, Step(1), ResourceUse::Mutation));
    assert(barrier.ReadState(modem) == ResourceState::Available);
    assert(barrier.AcceptInitialEvidence(euiccCheck, {true, true, true}));
    auto reservation = barrier.Acquire({euicc, modem, euicc}, Step(1), ResourceUse::Mutation);
    assert(reservation && reservation->resources.size() == 2);
    auto subset = *reservation;
    subset.resources.pop_back();
    assert(!barrier.CancelNotSent(subset));
    assert(!barrier.Acquire({euicc}, Step(2), ResourceUse::Query));
    assert(barrier.CancelNotSent(*reservation));
    assert(!barrier.CancelNotSent(*reservation));

    auto sent = barrier.Acquire({euicc}, Step(2), ResourceUse::Mutation);
    assert(sent && barrier.MarkSent(*sent));
    assert(!barrier.CancelNotSent(*sent));
    // 卡结果成功不证明关闭；结束/可复用同样不可单独代替写效果核对。
    assert(!barrier.Complete(*sent, {Step(2), true, false, true, EffectKnowledge::ConfirmedApplied}));
    assert(!barrier.Complete(*sent, {Step(2), true, true, true, EffectKnowledge::Unknown}));
    assert(barrier.Quarantine(*sent, EffectKnowledge::Unknown));
    assert(!barrier.AcceptRecoveryEvidence(*sent, {Step(2), true, false, true}));
    assert(!barrier.AcceptRecoveryEvidence(*sent, {Step(2, 2), true, true, true}));
    assert(!barrier.Acquire({euicc}, Step(3), ResourceUse::Mutation));
    assert(barrier.AcceptRecoveryEvidence(*sent, {Step(2), true, true, true}));

    auto fresh = barrier.Acquire({euicc}, Step(3), ResourceUse::Mutation);
    assert(fresh && barrier.MarkSent(*fresh));
    assert(!barrier.AcceptRecoveryEvidence(*sent, {Step(2), true, true, true}));
    assert(!barrier.Complete(*sent, {Step(2), true, true, true, EffectKnowledge::ConfirmedApplied}));
    assert(!barrier.AcceptInitialEvidence(euiccCheck, {true, true, true}));
    assert(barrier.ReadState(euicc) == ResourceState::InFlight);
    assert(barrier.Complete(*fresh, {Step(3), true, true, true, EffectKnowledge::ConfirmedApplied}));
    // 查询响应丢失也能凭命令结束/关闭/可复用释放，无需恢复旧payload。
    auto query = barrier.Acquire({euicc}, Step(4), ResourceUse::Query);
    assert(query && barrier.MarkSent(*query));
    assert(barrier.Complete(*query, {Step(4), true, true, true, EffectKnowledge::Unknown}));
    query = barrier.Acquire({euicc}, Step(5), ResourceUse::Query);
    assert(query && barrier.MarkSent(*query) && barrier.Quarantine(*query, EffectKnowledge::Unknown));
    assert(barrier.AcceptRecoveryEvidence(*query, {Step(5), true, false, true}));
    // 不能通过改复制租约的操作性质绕过原写资格。
    auto write = barrier.Acquire({euicc}, Step(6), ResourceUse::Mutation);
    assert(write && barrier.MarkSent(*write));
    auto forgedQuery = *write;
    forgedQuery.use = ResourceUse::Query;
    assert(!barrier.Complete(forgedQuery, {Step(6), true, true, true, EffectKnowledge::Unknown}));
}

void ExpectedTransitionScenarios()
{
    const RuntimeEpoch epoch {3, 9};
    const SessionRef old {epoch, {{0}, "euicc-app"}, 1};
    const SessionRef next {epoch, {{0}, "euicc-app"}, 2};
    const ExpectedTransition expected {Step(7), old, {old.application}, std::string("new-iccid"),
        TransitionCause::ProfileSwitch, 100, false};
    TransitionEvidence evidence {Step(7), TransitionCause::ProfileSwitch, 50, std::string("new-iccid"), true, next};
    SessionRegistry sessions(epoch);
    assert(!sessions.RegisterExpectedTransition(expected)); // 旧会话尚不可信。
    assert(sessions.PublishVerifiedSession(old));
    assert(sessions.RegisterExpectedTransition(expected));
    assert(!sessions.RegisterExpectedTransition(expected));
    assert(!sessions.EndExpectedTransitionWait(Step(7, 2), old, EffectKnowledge::NotSent));
    assert(sessions.EndExpectedTransitionWait(Step(7), old, EffectKnowledge::NotSent));
    assert(!sessions.AcceptExpectedContinuation(evidence));
    assert(sessions.RegisterExpectedTransition(expected));
    assert(sessions.EndExpectedTransitionWait(Step(7), old, EffectKnowledge::Unknown));
    assert(!sessions.AcceptExpectedContinuation(evidence)); // 期望不能自行创建新会话。
    // 模拟R已证实替换并完成T fence的会话发布输入；不模拟设备成功返回。
    sessions.InvalidateForVerifiedReplacement(old);
    assert(!sessions.StillCurrent(old));
    assert(!sessions.PublishVerifiedSession(old)); // 撤销不归零，不复活旧EF资格。
    assert(sessions.PublishVerifiedSession(next));
    auto unrelated = evidence;
    unrelated.protocolOrCorrelatedRequeryProvesCause = false;
    assert(!sessions.AcceptExpectedContinuation(unrelated));
    unrelated = evidence;
    unrelated.verifiedIdentity = "wrong-profile";
    assert(!sessions.AcceptExpectedContinuation(unrelated));
    const auto accepted = sessions.AcceptExpectedContinuation(evidence);
    assert(accepted && !accepted->mayAdvanceRequest && accepted->session.bindingGeneration == 2);
    assert(!sessions.AcceptExpectedContinuation(evidence)); // 一次消费。
    assert(!sessions.StillCurrent(old));
    auto nextExpected = expected;
    nextExpected.step = Step(8);
    nextExpected.oldSession = next;
    assert(sessions.RegisterExpectedTransition(nextExpected));
    assert(sessions.ExpireExpectedTransitions(99) == 0);
    assert(sessions.ExpireExpectedTransitions(100) == 1);
    assert(sessions.StillCurrent(next)); // 清理期望不清理有效会话/资源。
    nextExpected.step = Step(9);
    assert(sessions.RegisterExpectedTransition(nextExpected));
    assert(sessions.EndExpectedTransitionWait(Step(9), next, EffectKnowledge::ConfirmedNotApplied));
    assert(sessions.ExpireExpectedTransitions(100) == 0);
}

// 独立结果消费方：无需认识SetShowNumberUseCase/ActivateSimUseCase/AssignmentService。
struct RecordingSinks final : MsisdnResultSink, ActivationResultSink, RadioResultSink, CardDiscoverySink {
    int advances = 0;
    int lateEvidence = 0;
    std::optional<MsisdnWriteResult> number;
    std::optional<ActivationDeviceResult> active;
    std::optional<RadioSwitchResult> radio;
    void AcceptMsisdnResult(const MsisdnWriteResult &result) override { ++advances; number = result; }
    void AcceptLateEvidence(const MsisdnWriteResult &result) override { ++lateEvidence; number = result; }
    void AcceptDeviceResult(const ActivationDeviceResult &result) override { active = result; }
    void AcceptRadioStep(const RadioSwitchResult &result) override { radio = result; }
    bool FenceRuntimeSession(const SessionRef &, uint64_t) override { return false; }
    void ObserveCardDiscovery(const CardDiscoveryObservation &) override {}
};

void ResultSinkScenarios()
{
    RecordingSinks consumer;
    RuntimeResultSinks route {consumer, consumer, consumer, consumer};
    const SessionRef session {{5, 6}, {{1}, "application"}, 3};
    const MsisdnWriteResult completion {Step(11), session, EffectKnowledge::Unknown, false};
    route.DeliverMsisdn(completion, true);
    route.DeliverMsisdn(completion, false);
    assert(consumer.advances == 1 && consumer.lateEvidence == 1);
    assert(consumer.number->step.request.value == 11 && consumer.number->sentSession.bindingGeneration == 3);
    assert(consumer.number->effect == EffectKnowledge::Unknown && !consumer.number->legacyWaitSucceeded);
    route.DeliverActivation({Step(12), session, EffectKnowledge::ConfirmedNotApplied, -8});
    route.DeliverRadio({Step(13), {1}, 17, 2, EffectKnowledge::Unknown, -9});
    assert(consumer.active->step.request.value == 12 && consumer.active->legacyResult == -8);
    assert(consumer.radio->step.request.value == 13 && consumer.radio->protocolSession == 17);
    assert(consumer.radio->phase == 2 && consumer.radio->legacyResult == -9);
    assert(consumer.advances == 1); // 迟到证据不被该R路由转成号码推进出口。
}

// 只分配不透明测试存储；没有伪造ISimManager/IEsimManager定义或业务返回。
struct OpaqueStorage {
    int &destroyed;
    ~OpaqueStorage() { ++destroyed; }
};

void FactoryPublicationScenarios()
{
    int destroyed = 0;
    {
        auto esimStorage = std::shared_ptr<OpaqueStorage>(new OpaqueStorage {destroyed});
        auto simStorage = std::shared_ptr<OpaqueStorage>(new OpaqueStorage {destroyed});
        std::shared_ptr<IEsimManager> esim(esimStorage, reinterpret_cast<IEsimManager *>(esimStorage.get()));
        std::shared_ptr<ISimManager> sim(simStorage, reinterpret_cast<ISimManager *>(simStorage.get()));
        ManagerPublicationSequence publication(true);
        assert(!publication.RecordSimInitialization({sim, true}));
        assert(!publication.ConfirmEsimPublished(esim));
        // 原OnInit false只记录；不偷偷改变原Core的检查/发布策略。
        assert(publication.RecordEsimInitialization({esim, false}));
        assert(!publication.Esim().legacyOnInitResult);
        assert(!publication.RecordSimInitialization({sim, true}));
        // 同地址、不同控制块不能冒充原对象；不对无虚析构接口裸delete。
        std::shared_ptr<IEsimManager> unrelatedControlBlock(esim.get(), [](IEsimManager *) {});
        assert(!publication.ConfirmEsimPublished(unrelatedControlBlock));
        assert(publication.ConfirmEsimPublished(esim));
        assert(publication.RecordSimInitialization({sim, true}));
        assert(publication.ConfirmSimPublished(sim));
        assert(publication.Stage() == PublicationStage::SimPublished);
        assert(!publication.RecordEsimInitialization({esim, true}));
        esimStorage.reset();
        simStorage.reset();
        esim.reset();
        sim.reset();
        assert(destroyed == 0); // 模块仍拥有原控制块。
    }
    assert(destroyed == 2); // 匹配原对象控制块析构，各一次。
    ManagerPublicationSequence withoutEsim(false);
    assert(withoutEsim.Stage() == PublicationStage::EsimPublished);
}
}

int main()
{
    ResourceScenarios();
    FactoryPublicationScenarios();
    ExpectedTransitionScenarios();
    ResultSinkScenarios();
    std::cout << "resource / factory publication scenarios passed\n";
}
