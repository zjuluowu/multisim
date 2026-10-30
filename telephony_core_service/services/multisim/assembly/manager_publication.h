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
// PROPOSED: H的发布顺序/对象身份逻辑；不创建生产对象、不执行OnInit、不发布CMI。
#ifndef OHOS_TELEPHONY_MULTISIM_ASSEMBLY_MANAGER_PUBLICATION_H
#define OHOS_TELEPHONY_MULTISIM_ASSEMBLY_MANAGER_PUBLICATION_H

#include <memory>

namespace OHOS::Telephony { class ISimManager; class IEsimManager; }
namespace OHOS::Telephony::MultiSim {
struct EsimManagerInitialization { std::shared_ptr<IEsimManager> manager; bool legacyOnInitResult; };
struct SimManagerInitialization { std::shared_ptr<ISimManager> manager; bool legacyOnInitResult; };
enum class PublicationStage { BeforeEsim, EsimInitialized, EsimPublished, SimInitialized, SimPublished };

// 最终SIM侧工厂的私有成员。先原eSIM初始化/宿主发布，再原SIM初始化/宿主发布。
// bool只记录原OnInit返回；原Core不检查它，结构迁移不能据此新增强制失败。
class ManagerPublicationSequence final {
public:
    explicit ManagerPublicationSequence(bool hasEsim)
        : stage_(hasEsim ? PublicationStage::BeforeEsim : PublicationStage::EsimPublished) {}
    bool RecordEsimInitialization(const EsimManagerInitialization &observation)
    {
        if (stage_ != PublicationStage::BeforeEsim || !observation.manager) {
            return false;
        }
        esim_ = observation;
        stage_ = PublicationStage::EsimInitialized;
        return true;
    }
    bool ConfirmEsimPublished(const std::shared_ptr<IEsimManager> &published)
    {
        if (stage_ != PublicationStage::EsimInitialized || !SameControlBlock(esim_.manager, published)) {
            return false;
        }
        stage_ = PublicationStage::EsimPublished;
        return true;
    }
    bool RecordSimInitialization(const SimManagerInitialization &observation)
    {
        if (stage_ != PublicationStage::EsimPublished || !observation.manager) {
            return false;
        }
        sim_ = observation;
        stage_ = PublicationStage::SimInitialized;
        return true;
    }
    bool ConfirmSimPublished(const std::shared_ptr<ISimManager> &published)
    {
        if (stage_ != PublicationStage::SimInitialized || !SameControlBlock(sim_.manager, published)) {
            return false;
        }
        stage_ = PublicationStage::SimPublished;
        return true;
    }
    PublicationStage Stage() const { return stage_; }
    const EsimManagerInitialization &Esim() const { return esim_; }
    const SimManagerInitialization &Sim() const { return sim_; }
private:
    template<class Manager>
    static bool SameControlBlock(const std::shared_ptr<Manager> &original, const std::shared_ptr<Manager> &published)
    {
        return original && original.get() == published.get() &&
            !original.owner_before(published) && !published.owner_before(original);
    }
    PublicationStage stage_;
    EsimManagerInitialization esim_ {};
    SimManagerInitialization sim_ {};
};
} // namespace OHOS::Telephony::MultiSim
#endif // OHOS_TELEPHONY_MULTISIM_ASSEMBLY_MANAGER_PUBLICATION_H
