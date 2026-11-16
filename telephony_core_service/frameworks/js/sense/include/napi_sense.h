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

#ifndef NAPI_SENSE_H
#define NAPI_SENSE_H

#include "ffrt.h"

#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "base_context.h"
#include "hidden_camera_detection_callback.h"
#include "i_hidden_camera_detection_callback.h"
#include "napi_util.h"
#include "telephony_errors.h"
#include "telephony_log_wrapper.h"

namespace OHOS {
namespace Telephony {

constexpr int32_t DEFAULT_REF_COUNT = 1;
constexpr int32_t SENSE_PARAM_COUNT_TWO = 2;
constexpr int32_t SENSE_ARRAY_INDEX_FIRST = 0;
constexpr int32_t SENSE_ARRAY_INDEX_SECOND = 1;
constexpr int32_t SENSE_CALLBACK_VALUES_SIZE = 2;

struct SenseAsyncContext {
    BaseContext context;
    HiddenCameraDetectionConfig config;
    int32_t result = static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_INTERNAL_SYSTEM_ERROR);
};

struct SenseManagerState {
    ffrt::mutex mutex;
    sptr<HiddenCameraDetectionCallback> callback;
    bool registered = false;
    napi_env env = nullptr;
    napi_ref thisVar = nullptr;
    napi_ref callbackRef = nullptr;
};

SenseManagerState &GetSenseManagerState();

int32_t RegisterHiddenCameraDetection(napi_env env, napi_value &thisVar, napi_value *parameters);
int32_t UnregisterHiddenCameraDetection(napi_env env);

int32_t StartHiddenCameraDetectionAsync(const HiddenCameraDetectionConfig &config);
int32_t StopHiddenCameraDetectionAsync();
int32_t KeepHiddenCameraDetectionAsync();

} // namespace Telephony
} // namespace OHOS

#endif // NAPI_SENSE_H
