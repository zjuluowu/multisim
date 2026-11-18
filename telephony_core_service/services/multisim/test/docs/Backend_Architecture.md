# Legacy / Target 双后端测试架构

## 当前审计（2026-11-18，HEAD 1fd7c9c）

| 项 | 实际状态 |
|---|---|
| Legacy Host | `sim_test_host`、容量、Primary、生命周期、G2a/原版查询等 7 个程序独立链接；生产源码来自 `services/sim`，通过 `component_sources.gni` 和 Host GN 组织。 |
| Target | 仍是 `services/multisim` 的 25 个 `testonly source_set` 骨架。`AssignmentService` 主流程、持久化、输出、Radio 外部闭包和生命周期没有完整实现/链接。 |
| Shared case | 卡状态、PIN/PUK、主卡选择、查询和生命周期候选可共享 `SimTest`；实现专项继续在 `cases/internal`。 |
| C0 | Legacy 已有真实 `CoreServiceSim -> SimManager -> MultiSimController` 用例。Target 只有 `PlanPrimarySwitch` 的真实决策代码，没有可执行的完整服务闭环。 |
| Device | Legacy 设备框架仍存在；Target 产品/SA/IPC 链未接入，配置为 BLOCKED。 |
| VS Code | Testing/TestMate 可识别 Legacy 和 `sim_test_target_host`；Both 由 CLI 调度两个独立进程。 |

## 设计

- `config/test_backend.json` 是统一配置；CLI > 配置 > 内置默认值。
- 默认 backend 是 `legacy`。所有现有 Legacy target 名、`all/build/run/list/debug/device` 和过滤参数保留。
- `sim_target_production_host` 是只含 Target 源码的静态库；`sim_test_target_host` 不依赖任何 `services/sim` 或 Legacy adapter。
- Legacy 与 Target 永不在同一进程混链。Both 逐后端构建/运行，并写入独立 `backend_reports`。
- `backend_capabilities.json` 记录场景支持状态和明确阻塞原因。`BLOCKED` 会产生非零结果，不能回退为 Legacy PASS。
- `compare_backends.py` 只比较外部业务字段，排除地址、线程、内部类和 ID。当前 Oracle 字段标记不会伪造通过。

## C0 分层结论

`TargetArchitecture.C0PrimaryPlanUsesNoDeviceSwitch` 执行真实 Target `PlanPrimarySwitch`，验证 AlreadyPrimary 计划：参数写入与角色/通知后续计划、不发送 Modem 切换操作。这是决策层证据，不是 C0 业务闭环 PASS。

`SIM028-LEGACY-C0-DUAL-PRIMARY0` 在 Target 上明确 BLOCKED：缺少 `AssignmentService::StartPrimarySwitch`、聚合状态实现、Store/Output/Topology/Radio 实际装配和生命周期闭环。Both 因 Target BLOCKED 整体为 BLOCKED，即使 Legacy 通过。

## 使用

```sh
python3 -B test/scripts/test.py run --backend legacy --filter <name>
python3 -B test/scripts/test.py run --backend target \
  --filter TargetArchitecture.C0PrimaryPlanUsesNoDeviceSwitch
python3 -B test/scripts/test.py run --backend both \
  --filter RealSimPrimary.C0CoreToRealManagerAndController
```

结果分别保存在 `backend_reports`、`legacy.json`、`target.json` 和 `differential.json`。所有设备组合先检查后端能力；Target Device 不执行 Legacy 设备程序。

## 后续迁移

下一步只在授权 Target 生产迁移后补齐 C0 的 Assignment 聚合提交、持久化/角色输出、Radio/Topology 端口和真实资源清理。随后将同一 C0 Shared Case 绑定到 Target Adapter，并继续按场景扩展 PIN/PUK、卡状态和生命周期。产品 SA/IPC 和实机资源仍分别验收，Host 结果不替代产品证据。
