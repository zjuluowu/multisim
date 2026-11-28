# 正式 EF 读取与字段级 Refresh

输入 main 为 `6af154358f7fe215a2c39d24b33f1bc3598856ab`，验证使用正式 Target Host 组合根和受控设备端口。

ICCID 与 GID1 复用原卡应用、请求登记、资源屏障和透明 EF 两阶段命令。正式用例同时发出卡 A 的 ICCID、
卡 A 的 GID1 和卡 B 的 ICCID 三条 READ BINARY，经局部 Refresh 验证旧 ICCID 拒绝业务接纳、同卡 GID1
及邻卡正常完成、旧回包重复送达后不更新 Records。新 ICCID 经实际解码产生可信身份，正式发现路径建立绑定。
第二例在两条原设备命令之间变更路由版本，原请求未发 READ BINARY，随后合法新请求发令，GID1 失败清除原字段。
[生产路径执行审计 · 27 个方法](execution.json) [独立复跑 · 2/2](independent.log)

Target 全量 253/253。ASan 和 TSan 各执行 52 项，范围为正式 Primary、ICCID、EF Refresh、卡组合、PIN
和号码样板。12 个原冻结契约双端执行 24/24，后端及 Oracle 差分为零。
[Target](target.json.gz) [ASan](asan.json.gz) [TSan](tsan.json.gz) [Both](both.json.gz) [差分](differential.json)

Host 新增 EF CPP 后，旧固定清单首次实际链接缺少 `EfRecordsRuntime`。构建入口现按五个生产目录发现源码，
以现有逻辑 GN 专用清单确定 Host 边界。新增 EF CPP 进入归档及链接，无源码变更时保留清单时间戳。
Python 构建入口测试 21/21。资产校验覆盖 317 份归档原件、299 项去向及八项不可变合同或参考。
[发现时构建](source-discovery-build.json) [无变化构建](unchanged-source-build.json) [测试](backend-selector.log)
[架构及逻辑 GN/Ninja](architecture.log) [资产完整性](integrity.log)

| 最小变异 | 实际失败断言 | 原始输出 |
| --- | --- | --- |
| 卡应用的不同 EF 共用字段键 | 同卡 GID1 无法在原 ICCID 在途时登记 | [变异](merged_scope-mutated.log) [恢复](merged_scope-restored.log) |
| ICCID Refresh 失效 GID1 | 旧 ICCID 获得错误接纳，同时 GID1 结果丢失 | [变异](wrong_refresh-mutated.log) [恢复](wrong_refresh-restored.log) |
| READ BINARY 为原票重新选路 | 原票未经原路由版本核对即发出 READ BINARY | [变异](new_route_for_old_ticket-mutated.log) [恢复](new_route_for_old_ticket-restored.log) |
| 失败仍留 GID1 原字段 | 失败后仍可查询上一次的 GID1 | [变异](retain_failed_gid-mutated.log) [恢复](retain_failed_gid-restored.log) |

全图两遍关闭、跨阶段原 Radio 效果恢复、与角色及 Store 组合的旧实例重建仍待完成。
产品 RIL/HDI 路由与真实协议结果继续按独立产品门禁验证。
