/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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
#ifndef HIDDEN_CAMERA_DETECTION_CALLBACK_PROXY_H
#define HIDDEN_CAMERA_DETECTION_CALLBACK_PROXY_H

#include "i_hidden_camera_detection_callback.h"
#include "iremote_proxy.h"

namespace OHOS {
namespace Telephony {
class HiddenCameraDetectionCallbackProxy : public IRemoteProxy<IHiddenCameraDetectionCallback> {
public:
    explicit HiddenCameraDetectionCallbackProxy(const sptr<IRemoteObject> &impl);
    virtual ~HiddenCameraDetectionCallbackProxy() = default;
    int32_t OnHiddenCameraDetectionStatusChanged(const HiddenCameraDetectionRptInfo& rptInfo) override;

private:
    static inline BrokerDelegator<HiddenCameraDetectionCallbackProxy> delegator_;
};
} // namespace Telephony
} // namespace OHOS

#endif // HIDDEN_CAMERA_DETECTION_CALLBACK_PROXY_H
