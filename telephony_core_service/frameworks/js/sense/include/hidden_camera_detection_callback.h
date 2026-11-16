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

#ifndef HIDDEN_CAMERA_DETECTION_CALLBACK_H
#define HIDDEN_CAMERA_DETECTION_CALLBACK_H

#include "ffrt.h"
#include "hidden_camera_detection_callback_stub.h"

namespace OHOS {
namespace Telephony {

class HiddenCameraDetectionCallback : public HiddenCameraDetectionCallbackStub {
public:
    using ReportCallback = std::function<void(const HiddenCameraDetectionRptInfo&)>;
    void SetAppCallback(ReportCallback callback);
    int32_t OnHiddenCameraDetectionStatusChanged(const HiddenCameraDetectionRptInfo& rptInfo) override;

private:
    ffrt::mutex mutex_;
    ReportCallback appCallback_;
};

} // namespace Telephony
} // namespace OHOS

#endif // HIDDEN_CAMERA_DETECTION_CALLBACK_H
