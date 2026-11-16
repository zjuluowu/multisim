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

#include "napi_sense.h"

#include <memory>

#include "core_service_client.h"
#include "telephony_errors.h"
#include "telephony_log_wrapper.h"

namespace OHOS {
namespace Telephony {

SenseManagerState &GetSenseManagerState()
{
    static SenseManagerState state;
    return state;
}

static int32_t ReportInfo(const HiddenCameraDetectionRptInfo &info, napi_env envExecute)
{
    auto &state = GetSenseManagerState();
    if (envExecute != state.env) {
        TELEPHONY_LOGE("envExecute does not match env, outdated report");
        return TELEPHONY_ERROR;
    }
    if (state.env == nullptr || state.thisVar == nullptr || state.callbackRef == nullptr) {
        TELEPHONY_LOGE("callback info is not set");
        return TELEPHONY_ERROR;
    }
    napi_env env = state.env;
    napi_ref thisVar = state.thisVar;
    napi_ref callbackRef = state.callbackRef;
    napi_handle_scope scope = nullptr;
    napi_open_handle_scope(env, &scope);
    if (scope == nullptr) {
        TELEPHONY_LOGE("scope is nullptr");
        return TELEPHONY_ERROR;
    }
    napi_value callbackValues[SENSE_CALLBACK_VALUES_SIZE] = { 0 };
    napi_get_undefined(env, &callbackValues[0]);
    napi_create_object(env, &callbackValues[1]);
    NapiUtil::SetPropertyInt32(env, callbackValues[1], "stage", static_cast<int32_t>(info.stage));
    NapiUtil::SetPropertyInt32(env, callbackValues[1], "psdScore", static_cast<int32_t>(info.psdScore));
    napi_value thisVarValue = nullptr;
    napi_get_reference_value(env, thisVar, &thisVarValue);
    napi_value callbackFunc = nullptr;
    napi_get_reference_value(env, callbackRef, &callbackFunc);
    if (callbackFunc == nullptr) {
        TELEPHONY_LOGE("callbackFunc is nullptr!");
        napi_close_handle_scope(env, scope);
        return TELEPHONY_ERROR;
    }
    napi_value callbackResult = nullptr;
    napi_status ret = napi_call_function(
        env, thisVarValue, callbackFunc, std::size(callbackValues), callbackValues, &callbackResult);
    if (ret != napi_status::napi_ok) {
        TELEPHONY_LOGE("napi_call_function failed!");
        napi_close_handle_scope(env, scope);
        return TELEPHONY_ERROR;
    }
    napi_close_handle_scope(env, scope);
    return TELEPHONY_SUCCESS;
}

static int32_t ReportInfoToApp(const HiddenCameraDetectionRptInfo &info)
{
    auto &state = GetSenseManagerState();
    napi_env envExecute = nullptr;
    {
        std::lock_guard<ffrt::mutex> lock(state.mutex);
        if (state.env == nullptr || state.thisVar == nullptr || state.callbackRef == nullptr) {
            TELEPHONY_LOGE("callback info is not set");
            return TELEPHONY_ERROR;
        }
        envExecute = state.env;
    }

    auto task = [info, envExecute]() {
        int32_t ret = ReportInfo(info, envExecute);
        if (ret != TELEPHONY_SUCCESS) {
            TELEPHONY_LOGE("ReportInfo failed, result: %{public}d", ret);
            return;
        }
    };
    int32_t resultCode = napi_send_event(envExecute, task, napi_eprio_immediate);
    if (resultCode != TELEPHONY_SUCCESS) {
        TELEPHONY_LOGE("napi_send_event failed, result: %{public}d", resultCode);
        return TELEPHONY_ERROR;
    }
    return TELEPHONY_SUCCESS;
}

int32_t StartHiddenCameraDetectionAsync(const HiddenCameraDetectionConfig &config)
{
    auto &state = GetSenseManagerState();
    sptr<HiddenCameraDetectionCallback> callback = nullptr;
    {
        std::lock_guard<ffrt::mutex> lock(state.mutex);
        if (state.callback == nullptr) {
            TELEPHONY_LOGE("callback_ is nullptr, detection not registered");
            return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_INTERNAL_SYSTEM_ERROR);
        }
        callback = state.callback;
    }

    int32_t ret = DelayedRefSingleton<CoreServiceClient>::GetInstance().StartHiddenCameraDetection(
        callback, config);
    if (ret != static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS)) {
        TELEPHONY_LOGE("StartHiddenCameraDetection failed, ret %{public}d", ret);
        return ret;
    }
    return ret;
}

int32_t RegisterHiddenCameraDetection(napi_env env, napi_value &thisVar, napi_value *parameters)
{
    napi_ref thisVarRef = nullptr;
    napi_ref callbackRef = nullptr;
    napi_status status = napi_create_reference(env, thisVar, DEFAULT_REF_COUNT, &thisVarRef);
    if (status != napi_ok) {
        TELEPHONY_LOGE("create thisVar reference failed");
        return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_INTERNAL_SYSTEM_ERROR);
    }
    status = napi_create_reference(env, parameters[SENSE_ARRAY_INDEX_FIRST], DEFAULT_REF_COUNT, &callbackRef);
    if (status != napi_ok) {
        TELEPHONY_LOGE("create callback reference failed");
        napi_delete_reference(env, thisVarRef);
        return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_INTERNAL_SYSTEM_ERROR);
    }

    auto &state = GetSenseManagerState();
    std::lock_guard<ffrt::mutex> lock(state.mutex);
    if (state.registered) {
        TELEPHONY_LOGI("HiddenCameraDetection callback is already registered, ignore");
        napi_delete_reference(env, thisVarRef);
        napi_delete_reference(env, callbackRef);
        return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_HIDDEN_CAMERA_DETECTION_USE);
    }
    
    state.callback = sptr<HiddenCameraDetectionCallback>::MakeSptr();
    if (state.callback == nullptr) {
        TELEPHONY_LOGE("Create HiddenCameraDetectionCallback failed");
        napi_delete_reference(env, thisVarRef);
        napi_delete_reference(env, callbackRef);
        return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_INTERNAL_SYSTEM_ERROR);
    }
   
    state.env = env;
    state.thisVar = thisVarRef;
    state.callbackRef = callbackRef;
    state.callback->SetAppCallback([](const HiddenCameraDetectionRptInfo &info) {
        ReportInfoToApp(info);
    });
    state.registered = true;
    return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS);
}

int32_t UnregisterHiddenCameraDetection(napi_env env)
{
    auto &state = GetSenseManagerState();
    std::lock_guard<ffrt::mutex> lock(state.mutex);
    if (state.env != nullptr && state.env != env) {
        TELEPHONY_LOGE("sense APIs must be called on the registering thread");
        return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_API_CALLED_IN_DIFFERENT_THREAD);
    }
    state.registered = false;
    if (state.callback != nullptr) {
        state.callback->SetAppCallback(nullptr);
        state.callback = nullptr;
    }
    if (state.thisVar != nullptr) {
        uint32_t refCount = 0;
        if (napi_reference_unref(state.env, state.thisVar, &refCount) == napi_ok && refCount == 0) {
            napi_delete_reference(state.env, state.thisVar);
        }
        state.thisVar = nullptr;
    }
    if (state.callbackRef != nullptr) {
        uint32_t refCount = 0;
        if (napi_reference_unref(state.env, state.callbackRef, &refCount) == napi_ok && refCount == 0) {
            napi_delete_reference(state.env, state.callbackRef);
        }
        state.callbackRef = nullptr;
    }
    state.env = nullptr;

    return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS);
}

int32_t StopHiddenCameraDetectionAsync()
{
    auto &state = GetSenseManagerState();
    {
        std::lock_guard<ffrt::mutex> lock(state.mutex);
        if (!state.registered) {
            TELEPHONY_LOGI("HiddenCameraDetection is not started, cannot stop");
            return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_NO_HIDDEN_CAMERA_DETECTION);
        }
    }

    int32_t ret = DelayedRefSingleton<CoreServiceClient>::GetInstance().StopHiddenCameraDetection();
    if (ret != static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS)) {
        TELEPHONY_LOGE("StopHiddenCameraDetection failed, ret %{public}d", ret);
        return ret;
    }

    return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS);
}

int32_t KeepHiddenCameraDetectionAsync()
{
    auto &state = GetSenseManagerState();
    {
        std::lock_guard<ffrt::mutex> lock(state.mutex);
        if (!state.registered) {
            TELEPHONY_LOGI("HiddenCameraDetection is not started, cannot keep");
            return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_NO_HIDDEN_CAMERA_DETECTION);
        }
    }

    int32_t ret = DelayedRefSingleton<CoreServiceClient>::GetInstance().KeepHiddenCameraDetection();
    if (ret != static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS)) {
        TELEPHONY_LOGE("KeepHiddenCameraDetection failed, ret %{public}d", ret);
        return ret;
    }

    return static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS);
}

static napi_value CreateEnumConstructor(napi_env env, napi_callback_info info)
{
    napi_value thisArg = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, &thisArg, &data);
    return thisArg;
}

static napi_value CreateHiddenCameraDetectionTimeoutPeriod(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_STATIC_PROPERTY("PERIOD_16S",
            NapiUtil::ToInt32Value(env, static_cast<int32_t>(HiddenCameraDetectionTimeoutPeriod::PERIOD_16S))),
        DECLARE_NAPI_STATIC_PROPERTY("PERIOD_32S",
            NapiUtil::ToInt32Value(env, static_cast<int32_t>(HiddenCameraDetectionTimeoutPeriod::PERIOD_32S))),
        DECLARE_NAPI_STATIC_PROPERTY("PERIOD_64S",
            NapiUtil::ToInt32Value(env, static_cast<int32_t>(HiddenCameraDetectionTimeoutPeriod::PERIOD_64S))),
        DECLARE_NAPI_STATIC_PROPERTY("PERIOD_128S",
            NapiUtil::ToInt32Value(env, static_cast<int32_t>(HiddenCameraDetectionTimeoutPeriod::PERIOD_128S))),
    };
    napi_value result = nullptr;
    napi_define_class(env, "HiddenCameraDetectionTimeoutPeriod", NAPI_AUTO_LENGTH, CreateEnumConstructor, nullptr,
        sizeof(desc) / sizeof(*desc), desc, &result);
    napi_set_named_property(env, exports, "HiddenCameraDetectionTimeoutPeriod", result);
    return exports;
}

static void ConvertErrorCode(int32_t errorCode, SenseHiddenCameraDetectionRsltCode &rsltCode)
{
    auto minCode = static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_NOT_SUPPORT_HIDDEN_CAMERA_DETECTION);
    auto maxCode = static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_INTERNAL_SYSTEM_ERROR);
    if (errorCode == TELEPHONY_SUCCESS || (errorCode >= minCode && errorCode <= maxCode)) {
        rsltCode = static_cast<SenseHiddenCameraDetectionRsltCode>(errorCode);
    } else {
        rsltCode = SenseHiddenCameraDetectionRsltCode::ERR_INTERNAL_SYSTEM_ERROR;
    }
}

static napi_value CreateSenseHiddenCameraDetectionRsltCode(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_STATIC_PROPERTY("SUCCESS", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS))),
        DECLARE_NAPI_STATIC_PROPERTY("ERR_NOT_SUPPORT_HIDDEN_CAMERA_DETECTION", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_NOT_SUPPORT_HIDDEN_CAMERA_DETECTION))),
        DECLARE_NAPI_STATIC_PROPERTY("ERR_HIDDEN_CAMERA_DETECTION_USE", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_HIDDEN_CAMERA_DETECTION_USE))),
        DECLARE_NAPI_STATIC_PROPERTY("ERR_NO_MODEM_CAN_USE", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_NO_MODEM_CAN_USE))),
        DECLARE_NAPI_STATIC_PROPERTY("ERR_SKY_TONE_USE", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_SKY_TONE_USE))),
        DECLARE_NAPI_STATIC_PROPERTY("ERR_D2D_USE", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_D2D_USE))),
        DECLARE_NAPI_STATIC_PROPERTY("ERR_SATELLITE_OPEN", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_SATELLITE_OPEN))),
        DECLARE_NAPI_STATIC_PROPERTY("ERR_PARAMETER_INCORRECT", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_PARAMETER_INCORRECT))),
        DECLARE_NAPI_STATIC_PROPERTY("ERR_NO_HIDDEN_CAMERA_DETECTION", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_NO_HIDDEN_CAMERA_DETECTION))),
        DECLARE_NAPI_STATIC_PROPERTY("ERR_API_CALLED_IN_DIFFERENT_THREAD", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_API_CALLED_IN_DIFFERENT_THREAD))),
        DECLARE_NAPI_STATIC_PROPERTY("ERR_INTERNAL_SYSTEM_ERROR", NapiUtil::ToInt32Value(env,
            static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_INTERNAL_SYSTEM_ERROR))),
    };
    napi_value result = nullptr;
    napi_define_class(env, "SenseHiddenCameraDetectionRsltCode", NAPI_AUTO_LENGTH, CreateEnumConstructor, nullptr,
        sizeof(desc) / sizeof(*desc), desc, &result);
    napi_set_named_property(env, exports, "SenseHiddenCameraDetectionRsltCode", result);
    return exports;
}

void NativeStartHiddenCameraDetection(napi_env env, void *data)
{
    auto asyncContext = static_cast<SenseAsyncContext *>(data);
    if (asyncContext == nullptr) {
        TELEPHONY_LOGE("asyncContext is nullptr");
        return;
    }
    asyncContext->result = StartHiddenCameraDetectionAsync(asyncContext->config);
}

void StartHiddenCameraDetectionCallback(napi_env env, napi_status status, void *data)
{
    std::unique_ptr<SenseAsyncContext> context(static_cast<SenseAsyncContext *>(data));
    if (context == nullptr) {
        TELEPHONY_LOGE("context is nullptr");
        return;
    }
    if (context->result != static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS)) {
        UnregisterHiddenCameraDetection(env);
    }
    if (context->context.deferred != nullptr) {
        context->context.resolved = true;
        SenseHiddenCameraDetectionRsltCode rsltCode;
        ConvertErrorCode(context->result, rsltCode);
        napi_value resultValue = NapiUtil::ToInt32Value(env, static_cast<int32_t>(rsltCode));
        napi_resolve_deferred(env, context->context.deferred, resultValue);
    }
    napi_delete_async_work(env, context->context.work);
}

static napi_value StartHiddenCameraDetection(napi_env env, napi_callback_info info)
{
    size_t parameterCount = SENSE_PARAM_COUNT_TWO;
    napi_value parameters[SENSE_PARAM_COUNT_TWO] = { 0 };
    napi_value thisVar = nullptr;
    napi_get_cb_info(env, info, &parameterCount, parameters, &thisVar, nullptr);
    if (parameterCount != SENSE_PARAM_COUNT_TWO ||
        !NapiUtil::MatchParameters(env, parameters, { napi_function, napi_object })) {
        TELEPHONY_LOGE("StartHiddenCameraDetection parameter type mismatch");
        NapiUtil::ThrowParameterError(env);
        return nullptr;
    }

    napi_value periodValue = NapiUtil::GetNamedProperty(env, parameters[SENSE_ARRAY_INDEX_SECOND], "period");
    int32_t period = 0;
    napi_get_value_int32(env, periodValue, &period);

    int32_t ret = RegisterHiddenCameraDetection(env, thisVar, parameters);
    if (ret != static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS)) {
        TELEPHONY_LOGE("RegisterHiddenCameraDetection failed, ret %{public}d", ret);
        napi_deferred deferred = nullptr;
        napi_value promise = nullptr;
        napi_create_promise(env, &deferred, &promise);
        napi_value resultValue = NapiUtil::ToInt32Value(env, ret);
        napi_resolve_deferred(env, deferred, resultValue);
        return promise;
    }

    auto asyncContext = std::make_unique<SenseAsyncContext>();
    asyncContext->config.period = static_cast<HiddenCameraDetectionTimeoutPeriod>(period);

    napi_value result = nullptr;
    napi_create_promise(env, &asyncContext->context.deferred, &result);

    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "StartHiddenCameraDetection", NAPI_AUTO_LENGTH, &resourceName);
    SenseAsyncContext *pContext = asyncContext.release();
    napi_create_async_work(env, nullptr, resourceName,
        NativeStartHiddenCameraDetection, StartHiddenCameraDetectionCallback,
        static_cast<void *>(pContext), &pContext->context.work);
    if (napi_queue_async_work_with_qos(env, pContext->context.work, napi_qos_default) != napi_ok) {
        TELEPHONY_LOGE("napi_queue_async_work_with_qos failed");
        UnregisterHiddenCameraDetection(env);
        delete pContext;
        result = nullptr;
    }
    return result;
}

void NativeStopHiddenCameraDetection(napi_env env, void *data)
{
    auto asyncContext = static_cast<SenseAsyncContext *>(data);
    if (asyncContext == nullptr) {
        TELEPHONY_LOGE("asyncContext is nullptr");
        return;
    }
    asyncContext->result = StopHiddenCameraDetectionAsync();
}

void StopHiddenCameraDetectionCallback(napi_env env, napi_status status, void *data)
{
    std::unique_ptr<SenseAsyncContext> context(static_cast<SenseAsyncContext *>(data));
    if (context == nullptr) {
        TELEPHONY_LOGE("context is nullptr");
        return;
    }
    if (context->result == static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::SUCCESS)) {
        UnregisterHiddenCameraDetection(env);
    }
    if (context->context.deferred != nullptr) {
        SenseHiddenCameraDetectionRsltCode rsltCode;
        ConvertErrorCode(context->result, rsltCode);
        napi_value resultValue = NapiUtil::ToInt32Value(env, static_cast<int32_t>(rsltCode));
        napi_resolve_deferred(env, context->context.deferred, resultValue);
    }
    napi_delete_async_work(env, context->context.work);
}

static napi_value StopHiddenCameraDetection(napi_env env, napi_callback_info info)
{
    auto &state = GetSenseManagerState();
    {
        std::lock_guard<ffrt::mutex> lock(state.mutex);
        if (state.env != nullptr && state.env != env) {
            TELEPHONY_LOGE("sense APIs must be called on the registering thread");
            napi_deferred deferred = nullptr;
            napi_value promise = nullptr;
            napi_create_promise(env, &deferred, &promise);
            napi_value resultValue = NapiUtil::ToInt32Value(env,
                static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_API_CALLED_IN_DIFFERENT_THREAD));
            napi_resolve_deferred(env, deferred, resultValue);
            return promise;
        }
    }

    auto asyncContext = std::make_unique<SenseAsyncContext>();

    napi_value result = nullptr;
    napi_create_promise(env, &asyncContext->context.deferred, &result);

    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "StopHiddenCameraDetection", NAPI_AUTO_LENGTH, &resourceName);
    SenseAsyncContext *pContext = asyncContext.release();
    napi_create_async_work(env, nullptr, resourceName,
        NativeStopHiddenCameraDetection, StopHiddenCameraDetectionCallback,
        static_cast<void *>(pContext), &pContext->context.work);
    if (napi_queue_async_work_with_qos(env, pContext->context.work, napi_qos_default) != napi_ok) {
        TELEPHONY_LOGE("napi_queue_async_work_with_qos failed");
        delete pContext;
        result = nullptr;
    }
    return result;
}

void NativeKeepHiddenCameraDetection(napi_env env, void *data)
{
    auto asyncContext = static_cast<SenseAsyncContext *>(data);
    if (asyncContext == nullptr) {
        TELEPHONY_LOGE("asyncContext is nullptr");
        return;
    }
    asyncContext->result = KeepHiddenCameraDetectionAsync();
}

void KeepHiddenCameraDetectionCallback(napi_env env, napi_status status, void *data)
{
    std::unique_ptr<SenseAsyncContext> context(static_cast<SenseAsyncContext *>(data));
    if (context == nullptr) {
        TELEPHONY_LOGE("context is nullptr");
        return;
    }
    if (context->context.deferred != nullptr) {
        SenseHiddenCameraDetectionRsltCode rsltCode;
        ConvertErrorCode(context->result, rsltCode);
        napi_value resultValue = NapiUtil::ToInt32Value(env, static_cast<int32_t>(rsltCode));
        napi_resolve_deferred(env, context->context.deferred, resultValue);
    }
    napi_delete_async_work(env, context->context.work);
}

static napi_value KeepHiddenCameraDetection(napi_env env, napi_callback_info info)
{
    auto &state = GetSenseManagerState();
    {
        std::lock_guard<ffrt::mutex> lock(state.mutex);
        if (state.env != nullptr && state.env != env) {
            TELEPHONY_LOGE("sense APIs must be called on the registering thread");
            napi_deferred deferred = nullptr;
            napi_value promise = nullptr;
            napi_create_promise(env, &deferred, &promise);
            napi_value resultValue = NapiUtil::ToInt32Value(env,
                static_cast<int32_t>(SenseHiddenCameraDetectionRsltCode::ERR_API_CALLED_IN_DIFFERENT_THREAD));
            napi_resolve_deferred(env, deferred, resultValue);
            return promise;
        }
    }

    auto asyncContext = std::make_unique<SenseAsyncContext>();

    napi_value result = nullptr;
    napi_create_promise(env, &asyncContext->context.deferred, &result);

    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "KeepHiddenCameraDetection", NAPI_AUTO_LENGTH, &resourceName);
    SenseAsyncContext *pContext = asyncContext.release();
    napi_create_async_work(env, nullptr, resourceName,
        NativeKeepHiddenCameraDetection, KeepHiddenCameraDetectionCallback,
        static_cast<void *>(pContext), &pContext->context.work);
    if (napi_queue_async_work_with_qos(env, pContext->context.work, napi_qos_default) != napi_ok) {
        TELEPHONY_LOGE("napi_queue_async_work_with_qos failed");
        delete pContext;
        result = nullptr;
    }
    return result;
}

static napi_value CreateFunctions(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_WRITABLE_FUNCTION("startHiddenCameraDetection", StartHiddenCameraDetection),
        DECLARE_NAPI_WRITABLE_FUNCTION("stopHiddenCameraDetection", StopHiddenCameraDetection),
        DECLARE_NAPI_WRITABLE_FUNCTION("keepHiddenCameraDetection", KeepHiddenCameraDetection),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}

EXTERN_C_START
napi_value InitNapiSense(napi_env env, napi_value exports)
{
    CreateFunctions(env, exports);
    CreateHiddenCameraDetectionTimeoutPeriod(env, exports);
    CreateSenseHiddenCameraDetectionRsltCode(env, exports);
    return exports;
}
EXTERN_C_END

static napi_module _senseModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = InitNapiSense,
    .nm_modname = "telephony.sense",
    .nm_priv = ((void *)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterSenseModule(void)
{
    napi_module_register(&_senseModule);
}

} // namespace Telephony
} // namespace OHOS
