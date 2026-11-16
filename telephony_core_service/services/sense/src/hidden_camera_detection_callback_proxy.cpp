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
#include "hidden_camera_detection_callback_proxy.h"

#include "telephony_errors.h"
#include "telephony_log_wrapper.h"

namespace OHOS {
namespace Telephony {

HiddenCameraDetectionCallbackProxy::HiddenCameraDetectionCallbackProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IHiddenCameraDetectionCallback>(impl)
{}

int32_t HiddenCameraDetectionCallbackProxy::OnHiddenCameraDetectionStatusChanged(
    const HiddenCameraDetectionRptInfo& rptInfo)
{
    MessageParcel dataParcel;
    if (!dataParcel.WriteInterfaceToken(GetDescriptor())) {
        TELEPHONY_LOGE("HiddenCameraDetectionCallbackProxy WriteInterfaceToken failed!");
        return TELEPHONY_ERR_WRITE_DATA_FAIL;
    }
    if (!rptInfo.Marshalling(dataParcel)) {
        TELEPHONY_LOGE("HiddenCameraDetectionCallbackProxy Marshalling rptInfo failed!");
        return TELEPHONY_ERR_WRITE_DATA_FAIL;
    }
    sptr<IRemoteObject> remote = Remote();
    if (remote == nullptr) {
        TELEPHONY_LOGE("HiddenCameraDetectionCallbackProxy remote is null!");
        return TELEPHONY_ERR_LOCAL_PTR_NULL;
    }
    MessageOption option(MessageOption::TF_ASYNC);
    MessageParcel replyParcel;
    int32_t errorCode = remote->SendRequest(
        static_cast<uint32_t>(IHiddenCameraDetectionCallback::ON_HIDDEN_CAMERA_DETECTION_STATUS_CHANGED),
        dataParcel, replyParcel, option);
    if (errorCode != ERR_NONE) {
        TELEPHONY_LOGE("HiddenCameraDetectionCallbackProxy SendRequest error = %{public}d.", errorCode);
    }
    return errorCode;
}

} // namespace Telephony
} // namespace OHOS
