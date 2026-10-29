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
// TARGET ARCHITECTURE SKELETON / PROPOSED: 尚未接入生产或构建，未转移写权。
// Design: docs/SIM_Target_Architecture_Design_v1.2.md §6.3、§9.2～9.5、§16.2。
// 未定义的方法有意仅保留声明；不提供伪成功或可执行设备路径。

#include "topology/application/topology_service.h"

namespace OHOS::Telephony::MultiSim {
TopologyService::TopologyService(const SlotCatalog &catalog, AccountStore &store)
    : bindings_(catalog), store_(store) {}

/* 伪代码，AcceptNumberAfterCardWrite / AcceptIndependentNumberBackwrite 尚无定义：
 * 1. 在 T 短提交域复核 target.binding / accountVersion，ReserveNumberField(target)。
 * 2. 锁外 store_.WriteDisplayNumber({target.binding.subscription, number})。
 *    来源 MultiSimController::SetShowNumberToDB -> SimRdbHelper::UpdateDataBySimId。
 * 3. 返回同一提交域复核相关输入，按旧错误依次接纳：
 *    accounts_.AcceptNumberVisibility(..., CurrentCache, number)；
 *    accounts_.AcceptNumberVisibility(..., HistoricalCache, number)。
 *    每一步的可见性独立；DB成功/当前成功/历史失败不得整体回滚或隐藏。
 * 4. 只有调用已结束且结果已接纳才 ReleaseAfterStoreCallFinished；DB已写但目标失效
 *    进入核对，不能只拒绝内存发布就宣称没有效果。未知效果不盲目重写。
 *
 * 这里是固定目标的 G3b 推荐。G2 结构切片须保留旧 SetShowNumberToDB 在此步骤才
 * GetTargetSimId(slot) 的解析时点，并保留 slot 越界返回 false(整数0) 等旧异常轨迹；
 * 这些兼容错误不能从本骨架自造的新枚举推导，修正须独立裁决。
 *
 * 迁移闭包：InitData/InitIccId/InitShowNumber/SetShowName/SaveImsSwitch、
 * GetListFromDataBase/GetAllListFromDataBase、Monitor RefreshData/用户切换/provider重连、
 * ForgetAllData、卡探测、InsertEsimData、force/卫星 active 及查询异步回写全部委托同一入口。
 * 建行/重载的角色字段使用 A 应用提供的片段；T 不持有或重写角色事实。
 */
} // namespace OHOS::Telephony::MultiSim
