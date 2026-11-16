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

#ifndef HIDDEN_CAMERA_DETECTION_CALLBACK_STUB_H
#define HIDDEN_CAMERA_DETECTION_CALLBACK_STUB_H

#include <functional>

#include "iremote_stub.h"
#include "i_hidden_camera_detection_callback.h"

namespace OHOS {
namespace Telephony {

class HiddenCameraDetectionCallbackStub : public IRemoteStub<IHiddenCameraDetectionCallback> {
public:
    int32_t OnRemoteRequest(
        uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;
    int32_t OnHiddenCameraDetectionStatusChanged(const HiddenCameraDetectionRptInfo& rptInfo) override = 0;
};

} // namespace Telephony
} // namespace OHOS

#endif // HIDDEN_CAMERA_DETECTION_CALLBACK_STUB_H
