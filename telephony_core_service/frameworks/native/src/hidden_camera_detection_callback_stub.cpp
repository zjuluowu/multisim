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

#include "hidden_camera_detection_callback_stub.h"

#include "telephony_errors.h"
#include "telephony_log_wrapper.h"

namespace OHOS {
namespace Telephony {

int32_t HiddenCameraDetectionCallbackStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    if (data.ReadInterfaceToken() != GetDescriptor()) {
        TELEPHONY_LOGE("descriptor checked fail");
        return TELEPHONY_ERR_DESCRIPTOR_MISMATCH;
    }
    if (code != static_cast<uint32_t>(IHiddenCameraDetectionCallback::ON_HIDDEN_CAMERA_DETECTION_STATUS_CHANGED)) {
        TELEPHONY_LOGE("unknown request code: %{public}u", code);
        return TELEPHONY_ERR_FAIL;
    }
    auto rptInfo = HiddenCameraDetectionRptInfo::Unmarshalling(data);
    if (rptInfo == nullptr) {
        TELEPHONY_LOGE("Unmarshalling rptInfo failed!");
        return TELEPHONY_ERR_READ_DATA_FAIL;
    }

    return OnHiddenCameraDetectionStatusChanged(*rptInfo);
}

} // namespace Telephony
} // namespace OHOS
