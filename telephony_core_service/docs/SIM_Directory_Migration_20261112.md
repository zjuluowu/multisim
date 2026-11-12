# SIM 目录迁移映射与实施记录

输入：`b283ad2f633686f4d829c5b005e08680f86695a7`，main。映射在移动前由实际文件、声明和GN源表生成；测试/tools保持原位。全量机器清单见 `services/multisim/test/multisim_architecture_rules/directory_migration_20261112.json`。本批不提交、不推送。

| 当前路径（multisim相对） | 目标路径 | 当前职责 | Owner | GN target | 方式 | 风险 |
|---|---|---|---|---|---|---|
| `BUILD.gn` | `BUILD.gn` | 构建入口或开发导航/设计副本 | 工程文档 | documentation/build | retain | 低 |
| `README.md` | `README.md` | 构建入口或开发导航/设计副本 | 工程文档 | documentation/build | retain | 低 |
| `SIM 重构代码目录结构设计方案.md` | `SIM 重构代码目录结构设计方案.md` | 构建入口或开发导航/设计副本 | 工程文档 | documentation/build | retain | 低 |
| `account/account_registry.cpp` | `account/account_registry.cpp` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：AccountRegistry::AcceptActiveProjection, AccountRegistry::AcceptDiscoveredAccount, AccountRegistry::AcceptDisplayName, AccountRegistry::AcceptImsPreference, AccountRegistry::AcceptLabel | Account | topology_domain | retain | 低 |
| `account/account_registry.h` | `account/account_registry.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：AccountProjection, AccountRecord, AccountAcceptance, AccountReloadAcceptance, AccountReloadInput | Account | topology_domain | retain | 低 |
| `account/account_role_planning_port.h` | `account/account_role_planning_port.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：AccountRolePlanningPort | Account | topology_contracts | retain | 低 |
| `account/account_store.h` | `account/account_store.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：AccountNumberPatch, AccountActivePatch, AccountNamePatch, AccountLabelPatch, AccountImsPreferencePatch | Account | topology_contracts | retain | 低 |
| `account/account_store_adapter.cpp` | `account/account_store_adapter.cpp` | 账户身份/绑定/资料及激活；窄端口与专属存储适配 | Account | topology_adapters | retain | 低 |
| `account/account_store_adapter.h` | `account/account_store_adapter.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：SimRdbHelper, AccountStoreAdapter | Account | topology_adapters | retain | 低 |
| `account/account_types.h` | `account/account_types.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：AccountReadView, AccountFieldRevisions | Account | topology_types | retain | 低 |
| `account/activation/activate_sim_use_case.cpp` | `account/activation/activate_sim_use_case.cpp` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ActivateSimUseCase::ActivateSimUseCase | Account | topology_cases | retain | 低 |
| `account/activation/activate_sim_use_case.h` | `account/activation/activate_sim_use_case.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ActivateSimUseCase | Account | topology_cases | retain | 低 |
| `account/activation/activation_assignment_port.h` | `account/activation/activation_assignment_port.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ActivationAssignmentPort | Account | topology_contracts | retain | 低 |
| `account/activation/activation_hook_adapter.h` | `account/activation/activation_hook_adapter.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ActivationHookAdapter | Account | topology_adapters | retain | 低 |
| `account/activation/activation_progress.cpp` | `account/activation/activation_progress.cpp` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ActivationProgress::Accept, ActivationProgress::Begin, ActivationProgress::Prepare, ActivationProgress::ReadLegacy | Account | topology_protocols | retain | 低 |
| `account/activation/activation_progress.h` | `account/activation/activation_progress.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ActivationTransition, ActivationProgress | Account | topology_protocols | retain | 低 |
| `account/activation/activation_runtime_port.h` | `account/activation/activation_runtime_port.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ActivationDeviceRequest, ActivationDeviceResult, ActivationResultSink, ActivationRuntimePort, ActivationHookPort | Account | topology_contracts | retain | 低 |
| `account/binding_index.cpp` | `account/binding_index.cpp` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：BindingIndex::AcceptAssemblyState, BindingIndex::AcceptBinding, BindingIndex::BindingIndex, BindingIndex::Find, BindingIndex::ReadPositionRevision | Account | topology_domain | retain | 低 |
| `account/binding_index.h` | `account/binding_index.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：SlotRuntimeEntry, BindingIndex | Account | topology_domain | retain | 低 |
| `account/discovery/card_observation_port.h` | `account/discovery/card_observation_port.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：CardObservationSink | Account | topology_contracts | retain | 低 |
| `account/discovery/discovery_observation_ledger.cpp` | `account/discovery/discovery_observation_ledger.cpp` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：DiscoveryObservationLedger::AcceptDiscovery, DiscoveryObservationLedger::AcceptInvalidation, DiscoveryObservationLedger::BeginStore, DiscoveryObservationLedger::CompleteStored, DiscoveryObservationLedger::EndPreparationFailure | Account | topology_protocols | retain | 低 |
| `account/discovery/discovery_observation_ledger.h` | `account/discovery/discovery_observation_ledger.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：DiscoveryStoreTicket, DiscoveryStoreAcceptance, DiscoveryObservationLedger, Entry | Account | topology_protocols | retain | 低 |
| `account/legacy_entry.h` | `account/legacy_entry.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：SetShowNumberUseCase, ShowNumberQuery, ActivateSimUseCase | Account | topology_contracts | retain | 低 |
| `account/number/number_query_host_port.h` | `account/number/number_query_host_port.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ImpuNumberResult, NumberBackwrite, NumberQueryHostPort | Account | topology_contracts | retain | 低 |
| `account/number/set_show_number_use_case.cpp` | `account/number/set_show_number_use_case.cpp` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：SetShowNumberUseCase::SetShowNumberUseCase | Account | topology_cases | retain | 低 |
| `account/number/set_show_number_use_case.h` | `account/number/set_show_number_use_case.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：SetShowNumberUseCase, PendingNumber | Account | topology_cases | retain | 低 |
| `account/number/show_number_completion_port.h` | `account/number/show_number_completion_port.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ShowNumberCompletion, ShowNumberCompletionPort | Account | topology_contracts | retain | 低 |
| `account/number/show_number_plan.cpp` | `account/number/show_number_plan.cpp` | 账户身份/绑定/资料及激活；窄端口与专属存储适配 | Account | topology_protocols | retain | 低 |
| `account/number/show_number_plan.h` | `account/number/show_number_plan.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：NumberUpdateNode, ShowNumberPlan | Account | topology_protocols | retain | 低 |
| `account/number/show_number_query.cpp` | `account/number/show_number_query.cpp` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ShowNumberQuery::ShowNumberQuery | Account | topology_cases | retain | 低 |
| `account/number/show_number_query.h` | `account/number/show_number_query.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ShowNumberQuery | Account | topology_cases | retain | 低 |
| `account/number/show_number_runtime_port.h` | `account/number/show_number_runtime_port.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：MsisdnWriteRequest, MsisdnWriteResult, MsisdnResultSink, CardNumberView, ShowNumberRuntimePort | Account | topology_contracts | retain | 低 |
| `account/topology_requests.h` | `account/topology_requests.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：SetShowNumberCommand, NumberSource, ShowNumberRead, ActivationKind, ActivateSimCommand | Account | topology_contracts | retain | 低 |
| `account/topology_service.cpp` | `account/topology_service.cpp` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：TopologyService::AcceptActivationProgress, TopologyService::BeginActivationProgress, TopologyService::BindRolePlanner, TopologyService::PrepareActivationProgress, TopologyService::ReadLegacyActivationProgress | Account | topology_cases | retain | 低 |
| `account/topology_service.h` | `account/topology_service.h` | 账户身份/绑定/资料及激活；窄端口与专属存储适配；实际声明/实现：ReloadAccountInput, ReloadRequest, AccountFieldReservation, TopologyService | Account | topology_cases | retain | 低 |
| `card/esim/legacy_profile_result.h` | `card/esim/legacy_profile_result.h` | 卡侧操作、应用会话/代次及Records加载 | Card | compatibility_core | retain | 低 |
| `card/esim/local_card_requests.cpp` | `card/esim/local_card_requests.cpp` | 卡侧操作、应用会话/代次及Records加载；实际声明/实现：LocalCardRequests::LocalCardRequests, LocalCardRequests::SwitchLocalProfile | Card | runtime_cases | retain | 低 |
| `card/esim/local_card_requests.h` | `card/esim/local_card_requests.h` | 卡侧操作、应用会话/代次及Records加载；实际声明/实现：LocalCardPort, LocalCardRequests | Card | runtime_contracts | retain | 低 |
| `card/esim/profile_requests.h` | `card/esim/profile_requests.h` | 卡侧操作、应用会话/代次及Records加载；实际声明/实现：VerifiedPortRoute, SwitchProfileRequest, EuiccPrimitiveResult, LocalProfileExecutionResult, LocalProfileTransitionRef | Card | runtime_contracts | retain | 低 |
| `card/records/ef_requests.h` | `card/records/ef_requests.h` | 卡侧操作、应用会话/代次及Records加载；实际声明/实现：ReadEfRequest | Card | runtime_contracts | retain | 低 |
| `card/records/load_session.cpp` | `card/records/load_session.cpp` | 卡侧操作、应用会话/代次及Records加载；实际声明/实现：LoadSession::AddMember, LoadSession::InvalidateAffectedReadMembers, LoadSession::LoadSession, LoadSession::TryConsumeRead | Card | runtime_mechanisms | retain | 低 |
| `card/records/load_session.h` | `card/records/load_session.h` | 卡侧操作、应用会话/代次及Records加载；实际声明/实现：LoadToken, LoadMemberState, LoadAccess, LoadMember, LoadSession | Card | runtime_mechanisms | retain | 低 |
| `card/security/pin_requests.h` | `card/security/pin_requests.h` | 卡侧操作、应用会话/代次及Records加载；实际声明/实现：VerifyPinRequest | Card | runtime_contracts | retain | 低 |
| `card/state/session_registry.cpp` | `card/state/session_registry.cpp` | 卡侧操作、应用会话/代次及Records加载；实际声明/实现：SessionRegistry::AcceptExpectedContinuation, SessionRegistry::CancelExpectedConfirmedNotSent, SessionRegistry::Current, SessionRegistry::EndExpectedWait, SessionRegistry::ExpireExpectedTransitions | Card | runtime_mechanisms | retain | 低 |
| `card/state/session_registry.h` | `card/state/session_registry.h` | 卡侧操作、应用会话/代次及Records加载；实际声明/实现：TransitionCause, ExpectedTransition, TransitionEvidence, ExpectedContinuation, SessionRegistry | Card | runtime_mechanisms | retain | 低 |
| `card_selection/account_role_protocol.h` | `role_assignment/account_role_protocol.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：AccountRoleProtocol, ReservedPlan, RoleProjection | Role Assignment | assignment_protocols | move | 低 |
| `card_selection/assignment_aggregate.cpp` | `role_assignment/assignment_aggregate.cpp` | 角色意图、分配聚合、主卡计划及专属端口/适配 | Role Assignment | assignment_domain | move | 低 |
| `card_selection/assignment_aggregate.h` | `role_assignment/assignment_aggregate.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：TransitionPhase, AssignmentTransition, TransitionAdmission, AssignmentAggregate | Role Assignment | assignment_domain | move | 低 |
| `card_selection/assignment_output_adapter.h` | `role_assignment/assignment_output_adapter.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：AssignmentOutputAdapter | Role Assignment | assignment_adapters | move | 低 |
| `card_selection/assignment_requests.h` | `role_assignment/assignment_requests.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：SetRoleCommand, SwitchPrimaryCommand, RadioAssignmentView, AssignmentView, AccountRolePlanningInput | Role Assignment | assignment_contracts | move | 低 |
| `card_selection/assignment_service.cpp` | `role_assignment/assignment_service.cpp` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：AssignmentService::AcceptDiscoveredRoleProjection, AssignmentService::AcceptLocalProfilePreference, AssignmentService::AcceptReloadedRoleProjection, AssignmentService::AcceptReloadedRoleProjectionSet, AssignmentService::AssignmentService | Role Assignment | assignment_cases | move | 低 |
| `card_selection/assignment_service.h` | `role_assignment/assignment_service.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：AssignmentService, ConflictKind, AssignmentConflictRegistration | Role Assignment | assignment_cases | move | 低 |
| `card_selection/assignment_store.h` | `role_assignment/assignment_store.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：PrimaryParameterSnapshot, AssignmentParameterResult, RoleStoreResult, AssignmentStore | Role Assignment | assignment_contracts | move | 低 |
| `card_selection/assignment_store_adapter.h` | `role_assignment/assignment_store_adapter.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：SimRdbHelper, AssignmentStoreAdapter | Role Assignment | assignment_adapters | move | 低 |
| `card_selection/assignment_types.h` | `role_assignment/assignment_types.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：DefaultRole | Role Assignment | assignment_types | move | 低 |
| `card_selection/legacy_entry.h` | `role_assignment/legacy_entry.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：AssignmentService | Role Assignment | assignment_contracts | move | 低 |
| `card_selection/primary/primary_execution.h` | `role_assignment/primary/primary_execution.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：PrimarySwitchMode, ParameterWritePosition, PrimaryDecisionFacts | Role Assignment | assignment_types | move | 低 |
| `card_selection/primary/primary_switch_plan.cpp` | `role_assignment/primary/primary_switch_plan.cpp` | 角色意图、分配聚合、主卡计划及专属端口/适配 | Role Assignment | assignment_protocols | move | 低 |
| `card_selection/primary/primary_switch_plan.h` | `role_assignment/primary/primary_switch_plan.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：PrimaryAction, PrimarySwitchPlan | Role Assignment | assignment_protocols | move | 低 |
| `card_selection/primary/primary_topology_port.h` | `role_assignment/primary/primary_topology_port.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：PrimaryTargetFacts, PrimaryTopologyPort | Role Assignment | assignment_contracts | move | 低 |
| `card_selection/primary/radio_execution_port.h` | `role_assignment/primary/radio_execution_port.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：PrimaryRadioObservation, RadioSwitchRequest, RadioSwitchResult, RadioResultSink, RadioExecutionPort | Role Assignment | assignment_contracts | move | 低 |
| `card_selection/profile_preference_port.h` | `role_assignment/profile_preference_port.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：ProfilePreferencePort | Role Assignment | assignment_contracts | move | 低 |
| `card_selection/role_selection.h` | `role_assignment/role_selection.h` | 角色意图、分配聚合、主卡计划及专属端口/适配；实际声明/实现：RoleSelection | Role Assignment | assignment_domain | move | 低 |
| `device/resource/resource_barrier.cpp` | `device/resource/resource_barrier.cpp` | 资源屏障、资格及恢复证据；实际声明/实现：ResourceBarrier::AcceptInitialRecoveryEvidence, ResourceBarrier::AcceptRecoveryEvidence, ResourceBarrier::Acquire, ResourceBarrier::CancelConfirmedNotSent, ResourceBarrier::Complete | Device | runtime_mechanisms | retain | 低 |
| `device/resource/resource_barrier.h` | `device/resource/resource_barrier.h` | 资源屏障、资格及恢复证据；实际声明/实现：ResourceState, EffectRequirement, ResourceKey, ResourceLease, InitialRecoveryEvidence | Device | runtime_mechanisms | retain | 低 |
| `integration/api/legacy_boundary_ports.h` | `infrastructure/compatibility/api/legacy_boundary_ports.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：LegacyCallerRef, LegacyCallbackRef, EntryCheck, LegacySynchronousCall, LegacyIngressPort | Infrastructure | compatibility_contracts | move | 低 |
| `integration/api/legacy_sim_facade.cpp` | `infrastructure/compatibility/api/legacy_sim_facade.cpp` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：LegacySimFacade::LegacySimFacade, LegacySimFacade::RunOrdinaryActivationAtExecutionPoint, LegacySimFacade::RunPrimarySwitchAtExecutionPoint, LegacySimFacade::SwitchLocalProfileAndAssignAtExecutionPoint, LegacySimFacade::SwitchLocalProfileAtExecutionPoint | Infrastructure | compatibility_core | move | 低 |
| `integration/api/legacy_sim_facade.h` | `infrastructure/compatibility/api/legacy_sim_facade.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：LegacySimFacade | Infrastructure | compatibility_core | move | 低 |
| `integration/api/real_sim_count_query.cpp` | `infrastructure/compatibility/api/real_sim_count_query.cpp` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：RealSimCountQuery::Read | Infrastructure | readonly_query | move | 低 |
| `integration/api/real_sim_count_query.h` | `infrastructure/compatibility/api/real_sim_count_query.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：RealSimCountHostPort, RealSimCountQuery | Infrastructure | readonly_query | move | 低 |
| `integration/events/completion_cell.h` | `infrastructure/compatibility/events/completion_cell.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：CompletionCell | Infrastructure | compatibility_core | move | 低 |
| `integration/events/legacy_completion.cpp` | `infrastructure/compatibility/events/legacy_completion.cpp` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：LegacyCompletion::CompleteLegacyInt | Infrastructure | compatibility_core | move | 低 |
| `integration/events/legacy_completion.h` | `infrastructure/compatibility/events/legacy_completion.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：LegacyCompletion | Infrastructure | compatibility_core | move | 低 |
| `integration/events/legacy_output_adapter.cpp` | `infrastructure/compatibility/events/legacy_output_adapter.cpp` | 技术机制/兼容交付/宿主装配；不写业务事实 | Infrastructure | compatibility_core | move | 低 |
| `integration/events/legacy_output_adapter.h` | `infrastructure/compatibility/events/legacy_output_adapter.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：MultiSimMonitor, LegacyOutputAdapter | Infrastructure | compatibility_core | move | 低 |
| `integration/lifecycle/card_runtime.cpp` | `card/runtime/card_runtime.cpp` | 持有卡Session/Load/MSISDN关联及Device ResourceBarrier/Radio引用的过渡骨架；本批仅改路径，不重新分配写权；实际声明/实现：CardRuntime::BindResultSinks, CardRuntime::CardRuntime, CardRuntime::EndLocalProfileWait, CardRuntime::InvalidateVerifiedCardSession, CardRuntime::PublishAcceptedCardDiscovery | Card（临时容纳；Device职责待拆） | runtime_core | move | 高（仅迁址，拆分待裁决） |
| `integration/lifecycle/card_runtime.h` | `card/runtime/card_runtime.h` | 持有卡Session/Load/MSISDN关联及Device ResourceBarrier/Radio引用的过渡骨架；本批仅改路径，不重新分配写权；实际声明/实现：SimStateManager, SimFileManager, SimSmsManager, IccDiallingNumbersManager, StkManager | Card（临时容纳；Device职责待拆） | runtime_core | move | 高（仅迁址，拆分待裁决） |
| `integration/lifecycle/exports/sim_host_ports.h` | `infrastructure/assembly/exports/sim_host_ports.h` | 技术机制/兼容交付/宿主装配；不写业务事实 | Infrastructure | repository_exports | move | 低 |
| `integration/lifecycle/exports/sim_module_factory.h` | `infrastructure/assembly/exports/sim_module_factory.h` | 技术机制/兼容交付/宿主装配；不写业务事实 | Infrastructure | repository_exports | move | 低 |
| `integration/lifecycle/host_ports.h` | `infrastructure/assembly/host_ports.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：ITelRilManager, ISimManager, IEsimManager, SimHostPorts, SimServicePublicationPort | Infrastructure | assembly_contracts | move | 低 |
| `integration/lifecycle/manager_publication.h` | `infrastructure/assembly/manager_publication.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：ISimManager, IEsimManager, EsimManagerInitialization, SimManagerInitialization, PublicationStage | Infrastructure | assembly_contracts | move | 低 |
| `integration/lifecycle/result_sinks.h` | `card/runtime/result_sinks.h` | 向原发起Owner交付执行结果/迟到证据的窄引用集合；非Legacy对外交付；实际声明/实现：RuntimeResultSinks | Card | runtime_core | move | 中 |
| `integration/lifecycle/sim_module.h` | `infrastructure/assembly/sim_module.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：ExistingSimBindings, SimModule, DrainResult, SimModuleFactoryAccess, SimModuleDeleter | Infrastructure | assembly_core | move | 低 |
| `integration/lifecycle/sim_module_factory.cpp` | `infrastructure/assembly/sim_module_factory.cpp` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：SimModule::SimModule | Infrastructure | assembly_core | move | 低 |
| `integration/lifecycle/sim_module_factory.h` | `infrastructure/assembly/sim_module_factory.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：SimModule, ExistingSimBindings, SimModuleDeleter, SimModuleInitFailure, Stage | Infrastructure | assembly_contracts | move | 低 |
| `support/request/request_context.h` | `infrastructure/request/request_context.h` | 用例私有请求身份、步骤、完成状态机制；无业务流程和重试决策；实际声明/实现：CompletionId, SynchronousWaitRef, InternalReplyRef, CallbackCompletionRoute, SynchronousCompletionRoute | Infrastructure | request_protocol | move | 低 |
| `support/request/runtime_operation_store.h` | `infrastructure/request/runtime_operation_store.h` | 用例私有请求身份、步骤、完成状态机制；无业务流程和重试决策；实际声明/实现：RuntimeOperationStore, ResultAdmission | Infrastructure | request_mechanism | move | 低 |
| `support/request_ref.h` | `infrastructure/request/request_ref.h` | 用例私有请求身份、步骤、完成状态机制；无业务流程和重试决策；实际声明/实现：RequestOwner, RequestId, StepRef, EffectKnowledge | Infrastructure | sim_values | move | 低 |
| `support/sim_identity.h` | `infrastructure/sim_identity.h` | 技术机制/兼容交付/宿主装配；不写业务事实；实际声明/实现：SubscriptionId, SlotId, RadioDomainId, SlotTarget, CardApplicationTarget | Infrastructure | sim_values | move | 低 |
| `support/slot_catalog.cpp` | `infrastructure/assembly/slot_catalog.cpp` | 宿主构建的只读静态槽描述；无动态绑定事实；实际声明/实现：SlotCatalog::Find, SlotCatalog::SlotCatalog | Infrastructure | sim_values | move | 低 |
| `support/slot_catalog.h` | `infrastructure/assembly/slot_catalog.h` | 宿主构建的只读静态槽描述；无动态绑定事实；实际声明/实现：EndpointKind, VisibilityClass, SlotDescriptor, LegacyCapacityMeasures, SlotCatalog | Infrastructure | sim_values | move | 低 |

## 高风险接缝裁决依据

`CardRuntime` 的 sessions_、loads_、msisdnAssociations_、observationSequences_ 属 Card 运行能力；resources_ 属 Device 设备资格；radio_/sharedRil_ 是借用设备执行接缝，catalog_ 是只读装配输入。构造参数、成员顺序、析构与异步引用均保持。实际已实现部分为构造、结果sink绑定、卡发现/失效和号码结果接纳，Primary/激活等完整执行仍只有声明。暂放 card/runtime 并非将 ResourceBarrier 写权改交 Card。按状态抽取 Device 执行实例需要另行证明原 session/serial、对象寿命及排空协议；本批不猜测实现，不创建替代设备对象。此为未完成职责拆分，不报告全部边界已收口。

`result_sinks.h` 是 RuntimeResultSinks：持有四个引用并把已判定结果送回 MsisdnResultSink、ActivationResultSink、RadioResultSink、CardObservationSink。mayAdvanceRequest 为入参；它不提交账户/角色聚合，不生成外部Legacy通知，不决定重试。随过渡运行接缝放 card/runtime，保持 runtime_core，不能搬成 Compatibility 的通用业务结果中心。后续拆分时按各执行能力分发给原发起者。

Profile专属 `card/esim/legacy_profile_result.h` 仅投影双结果，保持原位置及 compatibility_core 逻辑target。AccountStoreAdapter/ActivationHookAdapter、AssignmentStoreAdapter/AssignmentOutputAdapter 保留业务Owner；接口形态相似不是迁入Infrastructure的理由。

## 基线与既有失败

- Git：main@b283ad2f633686f4d829c5b005e08680f86695a7；暂存区为空，已有 default.profraw 未跟踪。当前授权不提交、不推送；未切换/清理工作区。完整worktree清单、远端查询及测试日志随本批证据归档。
- 结构基线：88 C++文件、157 include、25 source_set。旧 `--migration` 在73原始文件校验通过后因根设计文档副本未登记而失败；保留原日志，新增精确文档摘要登记，不能删掉文件以取绿。
- 旧 `test/check_relocation.py` 在 systemtest/BUILD.gn 的既有 followup 摘要处失败。历史 relocation_manifest 与原followup不重写为当时通过；本批将以独立的版本演进记录保存旧摘要、输入HEAD摘要与新摘要。
- 首次SIM Guard及G2a尝试因机器已有GoogleTest树不匹配锁定摘要而BLOCKED，无业务执行。随后下载既定commit归档并经原prepare_dependencies.py验证摘要，不修改依赖锁或使用未知系统库。

## 典型业务的结构核验

| 业务 | 入口/规则 | 事实及执行 | 公共机制 | 当前限制 |
|---|---|---|---|---|
| SetPrimarySlotId | Compatibility 的 LegacySimFacade → Role Assignment 的 StartLegacyPrimarySwitch/AssignmentService、PrimarySwitchPlan | Account 的 PrimaryTopologyPort提供绑定；Device负责Radio观察/资源；运行桥暂在card/runtime | Request仅提供请求/步骤；Assembly仅注入端口，Compatibility交付旧结果 | Target StartPrimarySwitch/SwitchPrimaryRadio仍未形成完整生产闭包，不宣称Target业务测试通过 |
| SetActiveSim | Account 的 ActivateSimUseCase、ActivationProgress | Account接纳active；Card维护应用会话；Device执行状态与Account事实分开 | ActivationAssignmentPort/ActivationRuntimePort/ActivationResultSink注入；请求由用例持有 | 原业务仍在services/sim；无本批生产写权迁移 |
| eSIM Profile | Card 的 LocalCardRequests、Profile请求与SessionRegistry期望转换 | Profile协议/库存归Card，绑定归Account，偏好归Role Assignment，真实资源资格归Device | ProfilePreferencePort、RequestContext、窄结果投影 | 无新增eSIM平台/设备验收，未搬迁66250任务状态机 |

所有检查以实际实现/声明区分能力；目录迁移不补伪成功、不修复旧业务缺陷。

## 目录调整结果与实际统计

五组一级源码目录已落地，旧card_selection/integration/support目录已移空并移除；common/startup/顶层compatibility在输入中不存在，也未新增。完整业务树见[详细设计§3.1](SIM_Multisim_Source_Architecture_Detailed_Design.md#source-tree)，包含全部测试、工具与证据的完整实际交付树见[complete-tree.txt](../services/multisim/test/multisim_architecture_rules/evidence/directory_20261112/complete-tree.txt)。该树由Git已跟踪与非忽略新文件的实际存在路径生成，排除本次清理的生成bytecode。

输入模块树共1023项：上述91项业务/构建/导航文件及932项test/tools文件。辅助文件逐项原路径、当前路径、Owner、Git blob及演进结论见[directory_auxiliary_inventory.json](../services/multisim/test/multisim_architecture_rules/directory_auxiliary_inventory.json)；所有932项保留原路径，919项字节相同，13项只涉及路径、护栏、导航或精确溯源元数据。无业务Golden/Scenario/Contract/Oracle/Fake断言改写。

| 统计口径 | 实测数量 |
|---|---:|
| 移动原文件 | 44 |
| 拆分类/文件 | 0 |
| 删除原文件（不含迁出路径） | 0 |
| 保留原路径的模块输入 | 47 |
| 保留原路径的测试/工具输入 | 932 |
| 保留原路径输入合计 | 979 |
| 新增交付文件（不含移动） | 13：模块内12项护栏/清单/证据，docs新增本报告1项 |
| 原输入归属未处理项 | 0（含932项辅助文件逐项归属） |
| 仍待职责拆分 | CardRuntime两文件与result_sinks接缝；不按“无人归属”隐去风险 |

当前模块交付树为1035项。新增内容均是工程护栏、溯源或证据，不新增生产业务实现。默认profile初始SHA保存在映射清单，最终摘要一致；暂存区仍为空，HEAD不变。使用文件系统rename保持原文件内容与可供Git识别的迁移关系，未操作索引或提交。

| 调整前目录 | 文件数 | 文本行数 | 涉及逻辑target数 | 实际责任/混合点 |
|---|---:|---:|---:|---|
| (root) | 3 | 1313 | 0 | BUILD/README及已提交设计副本（非C++） |
| account | 32 | 2147 | 6 | 账户及绑定/资料/激活，专属Adapter就近 |
| card | 10 | 609 | 4 | 卡请求/会话/加载；Profile专属兼容双结果保留 |
| card_selection | 18 | 957 | 6 | 角色/Primary/窄协议及专属Adapter |
| device | 2 | 285 | 1 | 真实资源资格纯规则；成熟设备执行未迁入 |
| integration | 20 | 1094 | 7 | 兼容/装配及CardRuntime混合运行接缝 |
| support | 6 | 326 | 3 | 身份/请求机制与只读槽描述，target仍细分 |

代码范围仍为64头/24源（88个C++单元，5418行），移动不改变其文本行数或非预处理token；文本行数含注释/空行。test/tools中142个C++文件合计15404行，含历史源码快照，不当作全部被当前target编译。辅助工程规模逐项保存在inventory的scale中。

| 调整后目录 | 文件数 | 文本行数 | 涉及逻辑target数 |
|---|---:|---:|---:|
| (root) | 3 | 1316 | 0 |
| account | 32 | 2147 | 6 |
| card | 13 | 873 | 5 |
| device | 2 | 285 | 1 |
| infrastructure | 23 | 1156 | 9 |
| role_assignment | 18 | 957 | 6 |

## Owner、数据与接口核验

| Owner | 管理的业务事实 | 核心类（实际存在） | 主要接口/关联 |
|---|---|---|---|
| Account | 订阅身份、已接纳绑定/资料/active与激活进度 | AccountRegistry、BindingIndex、TopologyService、ActivateSimUseCase、ActivationProgress | AccountStore、AccountRolePlanningPort、PrimaryTopologyPort、CardObservationSink、BindingRef；无SubscriptionId=SlotId转换 |
| Card | 卡应用代次/期望转换、加载成员、卡侧请求及结果关联 | SessionRegistry、LoadSession、LocalCardRequests；过渡CardRuntime | LocalCardPort、SessionRef、CardApplicationTarget、原发起Owner的窄result sinks |
| Role Assignment | 各默认业务角色与Primary分配、冲突/过渡及业务重试规则 | RoleSelection、AssignmentAggregate、AssignmentService、PrimarySwitchPlan | ProfilePreferencePort、ActivationAssignmentPort、RadioExecutionPort；保留不同角色身份语义 |
| Device | 真实资源资格、租约/结束/恢复证据 | ResourceBarrier | ResourceKey、ResourceLease、CompletionEvidence、RecoveryEvidence；设备原始观察与A角色投影分开 |
| Infrastructure | 无业务事实；持请求完成/回调等技术状态及模块装配关系 | RequestContext、RuntimeOperationStore、LegacySimFacade、LegacyCompletion、RealSimCountQuery、SlotCatalog、SimModule及工厂声明 | SimHostPorts、ManagerPublicationSequence、RealSimCountHostPort、精确exports；类型定义不授予写权 |

以上是当前骨架职责及不变量；生产成熟实现仍在services/sim，未声称已经迁移或收敛生产所有写点。CardRuntime混合成员已在本报告逐项列出，Card临时目录不改变Device资源写权，未完成真实注入/拆分与生命周期验收。

## GN target与依赖对照

25个source_set全部保留原名、testonly、visibility、check_includes及public_configs；16个仅修改source/public路径，9个源路径不变。新增target 0、删除target 0、合并target 0。deps/public_deps全部不变，原业务契约/Domain/用例/Adapter继续分别隔离。`sim_values`横跨身份、Request纯值与只读Catalog，`runtime_mechanisms`横跨Card/Device，未因目录收敛合并。

完整前后sources/public/deps等见[输入及映射](../services/multisim/test/multisim_architecture_rules/directory_migration_20261112.json)与[GN对照](../services/multisim/test/multisim_architecture_rules/directory-gn-comparison.json)。精确映射后的BUILD字节比较通过；GN/include两个DAG无新增循环和反向边，15项正负控覆盖旧目录、私有include、环、逆向target、宏include、缺源、C++成员变动和visibility变动。原始实际GN desc（含source/public/include_dirs/deps/public_deps等）随证据归档；首次未传显式root的desc失败亦保留。

| 逻辑target | 调整前物理区域 | 调整后物理区域 | 依赖变化 |
|---|---|---|---|
| assembly_contracts | integration/lifecycle | infrastructure/assembly | 无 |
| assembly_core | integration/lifecycle | infrastructure/assembly | 无 |
| assignment_adapters | card_selection | role_assignment | 无 |
| assignment_cases | card_selection | role_assignment | 无 |
| assignment_contracts | card_selection, card_selection/primary | role_assignment, role_assignment/primary | 无 |
| assignment_domain | card_selection | role_assignment | 无 |
| assignment_protocols | card_selection, card_selection/primary | role_assignment, role_assignment/primary | 无 |
| assignment_types | card_selection, card_selection/primary | role_assignment, role_assignment/primary | 无 |
| compatibility_contracts | integration/api | infrastructure/compatibility/api | 无 |
| compatibility_core | card/esim, integration/api, integration/events | card/esim, infrastructure/compatibility/api, infrastructure/compatibility/events | 无 |
| readonly_query | integration/api | infrastructure/compatibility/api | 无 |
| repository_exports | integration/lifecycle/exports | infrastructure/assembly/exports | 无 |
| request_mechanism | support/request | infrastructure/request | 无 |
| request_protocol | support/request | infrastructure/request | 无 |
| runtime_cases | card/esim | card/esim | 无 |
| runtime_contracts | card/esim, card/records, card/security | card/esim, card/records, card/security | 无 |
| runtime_core | integration/lifecycle | card/runtime | 无 |
| runtime_mechanisms | card/records, card/state, device/resource | card/records, card/state, device/resource | 无 |
| sim_values | support | infrastructure, infrastructure/assembly, infrastructure/request | 无 |
| topology_adapters | account, account/activation | account, account/activation | 无 |
| topology_cases | account, account/activation, account/number | account, account/activation, account/number | 无 |
| topology_contracts | account, account/activation, account/discovery, account/number | account, account/activation, account/discovery, account/number | 无 |
| topology_domain | account | account | 无 |
| topology_protocols | account/activation, account/discovery, account/number | account/activation, account/discovery, account/number | 无 |
| topology_types | account | account | 无 |

产品根BUILD、原Core测试的三份显式源表及systemtest共五份源表，只调整同一个real_sim_count_query.cpp及其窄include目录；Core C++/旧SIM/RIL/公共接口没有改动。源表14项审计通过，不把有限文本审计当完整OHOS GN/Feature解释器。

## 实际测试与命令

下列命令从Git根执行。`SIM_GTEST_ROOT`须经既定prepare_dependencies.py校验锁定归档；本轮使用commit b514bdc898e2951020cbdca1304b75f5950d1f59。输出变量均指定仓外位置，仓内证据可独立复核，不依赖旧/tmp恢复。

```sh
python3 -B telephony_core_service/services/multisim/test/multisim_architecture_rules/run.py --with-gn --migration --output "$SIM_CHECK_OUT"
python3 -B telephony_core_service/services/multisim/test/check_relocation.py
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/test_g2a_source_tables.py
python3 -B telephony_core_service/services/multisim/test/sim_guard/oracle/test_primary_oracle.py
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/test_legacy_primary_contract.py
python3 -B telephony_core_service/services/multisim/test/systemtest/tools/test_ci_sim028.py
python3 -B telephony_core_service/services/multisim/test/systemtest/tools/host.py all --gtest-root "$SIM_GTEST_ROOT" --out "$SIM_HOST_OUT" --jobs 4
python3 -B telephony_core_service/services/multisim/test/systemtest/scripts/run_g2a_query.py --gtest-root "$SIM_GTEST_ROOT" --output "$SIM_QUERY_OUT"
python3 -B telephony_core_service/services/multisim/test/sim_guard/runner/run_primary_contract.py --adapter target --output "$SIM_TARGET_OUT"
```

| 检查/测试 | 基线实际结果 | 迁移后实际结果 | 分类 |
|---|---|---|---|
| 结构/include/GN DAG | 88文件/157 include/25 targets PASS | 同数量PASS | 无新增依赖边 |
| 完整骨架GN、独立语法及合并头 | PASS；四组纯规则PASS | PASS；四组纯规则PASS | 对象构建，不是产品链接 |
| 边界突变负控 | 12/12 PASS | 15/15 PASS | 新增三个拒绝门禁，不弱化原12项 |
| 公开声明/ABI候选负控 | 5/5检出 | 5/5检出 | L1声明，不证明产品ABI |
| 源表审计 | 14/14 PASS | 14/14 PASS | query精确源表/窄include |
| Oracle技术测试 | 15/15 PASS | 15/15 PASS | 共用Oracle算法，不是Target业务 |
| Legacy分类器 | 13/13 PASS | 13/13 PASS | 不把额外失败算清理失败 |
| CI策略 | 未作为本轮前置基线运行 | 13/13 PASS | 原有工具无改动 |
| Host全套 | 64项59 PASS/5 FAIL | 64项59 PASS/相同5 FAIL | 原始状态仍FAIL；前置构建开始于迁移前、执行完成于迁移后，Legacy生产/测试语义未变，不声称干净旧目录产品重跑 |
| Host分组 | 兼容17、PIN4、设施23、容量1、主卡入口14通过；缺陷5失败 | 完全同数量及缺陷集合 | 无新增业务失败 |
| G2a真实生产查询 | 锁定依赖首次构建300秒超时，无业务PASS | 7 PASS/0 FAIL/0 BLOCKED | 实际原Core→Query→原Manager/EXT，六函数正执行 |
| 真实READY→LOCKED突变 | 原SIM Guard包装构建360秒超时，未执行突变 | 同一原负控：1项预期FAIL被检出；原实现恢复17/17 PASS | 只改仓外源码副本，GN重新编译并链接；生产SHA不变 |
| 原迁移溯源 | 73原始校验通过；未登记设计副本导致布局FAIL | 73+88历史溯源及91项新映射PASS | 精确登记文档摘要，不排除文件 |
| 测试迁移溯源 | 原followup摘要FAIL；6项既有漂移明列 | 219项PASS，127项C++/JSON原字节，21项显式followup | 历史manifest/快照不改写 |
| Target主卡adapter | 原设计即未实现 | 实际BLOCKED/exit2；差分NOT_RUN | 无伪Target执行 |

READY负控复用原run_sim_guard.py的唯一锚点、LIFE_001及原17项恢复过滤器，使用Host已构建对象副本以GN选择既有sim_guard_state_source覆盖点，重新编译/链接。真实命令、复制源码SHA、负控XML、恢复XML及可复现脚本随原始证据归档，未增加成功桩。

首次after运行期间检查器模块演进造成ImportError已保留；固定脚本后完整重验成功。辅助突变包装首次使用原始CRLF字节定位锚点被断言拒绝，改为原runner使用的read_text换行规范化后才执行；不改变原负控或生产源码。首次系统GTest不匹配、两次构建watchdog、原始业务FAIL均完整保存。

[验证摘要](../services/multisim/test/multisim_architecture_rules/evidence/directory_20261112/validation.json)、[原始日志/XML及复现脚本归档](../services/multisim/test/multisim_architecture_rules/evidence/directory_20261112/raw-evidence.tar.gz)、[证据摘要索引](../services/multisim/test/multisim_architecture_rules/evidence/directory_20261112/sha256.json)是本批独立证据。历史17×5等结果不改写成本轮通过数，Host五条缺陷不因目录移动改预期。

## 文档同步与旧路径分类

- 根AGENTS、部件AGENTS：五组落点、唯一写权、Infrastructure禁止事项、跨Owner窄契约与GN约束。
- 部件README、模块README、docs/README：开发导航、当前目录及实施限制。
- SIM_Multisim_Source_Architecture_Detailed_Design.md：当前完整业务树、稳态树、具体路径、功能归属与跨Owner接缝；§3.3/4.3历史迁移及裁决保留。
- SIM_Target_Architecture_Design_v1.2.md：当前§6.4/6.6物理路径；业务裁决和历史基线不变。
- SIM_Architecture_Refactor_Design.md：§4.4过期目标路径更新；重构前services/sim事实和SIM/WU编号不变。
- test/README、architecture rules/README、systemtest/README：测试位置、现行路径及历史结果口径。
- 唯一执行状态：新增本批范围、真实结果、已知失败及后续最小项；本报告承载逐文件映射与独立总结。

全局检索按用途处理：当前C++/GN/活动脚本已无旧路径引用；新测试中card_selection等裸目录名仅用于拒绝负控。source_migration的old/layout_history/历史responsibility、测试relocation_manifest及冻结configs为历史追溯，保留原路径。目录方案中的“原card_selection/integration/support”是迁移来源，不修改方案原则。两份2026-10-31 ADR、开闭源交叉审视、SetPrimarySlotId对比说明的固定f2acb72fa83d203cea9422a83da0d2dcac19a2c2链接及执行状态旧小节按历史基线解释；没有将旧services/sim事实替换为新目录。当前详细设计源码链接均指向实际新文件，历史提交链接不迁改。

## 遗留与未验收范围

1. CardRuntime仍同时持Card会话/加载和Device资源/Radio接缝。只完成安全迁址与责任标注，未完成成员拆分、回调寿命及设备资格接管；拆分需原session/serial、epoch/weak owner、排空及共享资源证据，不能靠目录反推Owner。
2. Target主卡、激活及完整eSIM Profile业务闭包未实现/未运行，SetPrimarySlotId/SetActiveSim/eSIM目前仅有本批静态对象与依赖核验及原纯规则。主卡入口14项和Legacy业务组件测试不等于Target整链；无真实新旧业务差分。
3. 旧Host五条缺陷继续FAIL，不修改services/sim/Core/RIL/公共接口、不应用历史Stop或并发候选补丁、不转移生产写权。
4. 无hb及匹配产品树/Manifest/Feature/设备配置；完整OHOS产品构建、SO/SA/IPC/HDI、设备Radio/资源结束及eSIM卡行为NOT_RUN/BLOCKED。不能把GN对象/Host通过当产品验收。
5. 无遗留未迁移的旧一级代码目录，无新增或未解决GN依赖变更；仍有上述已明确的运行职责接缝，不报告架构与功能已全部完成。
