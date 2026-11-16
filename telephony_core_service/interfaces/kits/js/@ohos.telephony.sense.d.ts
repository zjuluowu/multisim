/*
 * Copyright (C) 2026-2026 Huawei Device Co., Ltd.
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
/**
 * @file
 * @kit TelephonyKit
 */
import type { AsyncCallback } from './@ohos.base';
/**
 * Provides applications with APIs for detecting the presence or status of a hidden camera.
 *
 * @namespace sense
 * @syscap SystemCapability.Telephony.CoreService.Sense
 * @since 7
 */
declare namespace sense {
    /**
     * Start Hidden Camera Detection.
     *
     * @param { AsyncCallback<HiddenCameraDetectionRptInfo> } callback - Indicates the callback for reporting detection info.
     * @param { HiddenCameraDetectionConfig } config - Indicates the detection config including timeout period.
     * @returns { Promise<SenseHiddenCameraDetectionRsltCode> } Returns a Promise that resolves with the detection result.
     * returns {@code SUCCESS} if start detecting hidden camera success;
     * returns {@code ERR_NOT_SUPPORT_HIDDEN_CAMERA_DETECTION} if this device do not support detecting hidden camera;
     * returns {@code ERR_HIDDEN_CAMERA_DETECTION_USE} if this device can't start detecting hidden camera because it has already started;
     * returns {@code ERR_NO_MODEM_CAN_USE} if this device has activated at least two sims. You must close one sim at least to detect hidden camera;
     * returns {@code ERR_SKY_TONE_USE} if this device has activated sky tone. You must close sky tone to detect hidden camera;
     * returns {@code ERR_D2D_USE} if this device has activated D2D. You must close D2D to detect hidden camera;
     * returns {@code ERR_SATELLITE_OPEN} if this device has activated satellite communication. You must close satellite communication;
     * returns {@code ERR_PARAMETER_INCORRECT} if the parameter transferred to the interface is incorrect;
     * returns {@code ERR_API_CALLED_IN_DIFFERENT_THREAD} if api is called in different thread;
     * returns {@code ERR_INTERNAL_SYSTEM_ERROR} if internal system error occurred;
     * @syscap SystemCapability.Telephony.CoreService.Sense
     * @since 7
     */
    function startHiddenCameraDetection(callback: AsyncCallback<HiddenCameraDetectionRptInfo>,
        config: HiddenCameraDetectionConfig): Promise<SenseHiddenCameraDetectionRsltCode>;

    /**
     * Defines the hidden camera detection report infomation.
     *
     * @interface HiddenCameraDetectionRptInfo
     * @syscap SystemCapability.Telephony.CoreService.Sense
     * @since 7
     */
    export interface HiddenCameraDetectionRptInfo {
        stage: number;
        psdScore: number;
    }

    /**
     * Defines the hidden camera detection config.
     *
     * @interface HiddenCameraDetectionConfig
     * @syscap SystemCapability.Telephony.CoreService.Sense
     * @since 7
     */
    export interface HiddenCameraDetectionConfig {
        period: HiddenCameraDetectionTimeoutPeriod;
    }

    /**
     * Indicates the timeout period. After the detection is started, the timer is started.
     * When the timer expires, the camera detection automatically stops and the occupied resources are released.
     *
     * @enum { number }
     * @syscap SystemCapability.Telephony.CoreService.Sense
     * @since 7
     */
    export enum HiddenCameraDetectionTimeoutPeriod {
        /**
         * Indicates timeout period is 16 seconds.
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        PERIOD_16S = 0,
        /**
         * Indicates timeout period is 32 seconds.
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        PERIOD_32S = 1,
        /**
         * Indicates timeout period is 64 seconds.
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        PERIOD_64S = 2,
        /**
         * Indicates timeout period is 128 seconds.
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        PERIOD_128S = 3,
    }

    /**
     * Indicates the result for function SenseHiddenCameraDetectionRsltCode
     *
     * @enum { number }
     * @syscap SystemCapability.Telephony.CoreService.Sense
     * @since 7
     */
    export enum SenseHiddenCameraDetectionRsltCode {
        /**
         * Indicates func run successfully.
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        SUCCESS = 0,
        /**
         * Indicates this device do not support detecting hidden camera.
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        ERR_NOT_SUPPORT_HIDDEN_CAMERA_DETECTION = 85065728,
        /**
         * Indicates this device can't start detecting hidden camera because it has already started
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        ERR_HIDDEN_CAMERA_DETECTION_USE = 85065729,
        /**
         * Indicates this device has activated at least two sims. You must close one sim at least to detect hidden camera
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        ERR_NO_MODEM_CAN_USE = 85065730,
        /**
         * Indicates this device has activated sky tone. You must close sky tone to detect hidden camera
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        ERR_SKY_TONE_USE = 85065731,
        /**
         * Indicates this device has activated D2D. You must close D2D to detect hidden camera
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        ERR_D2D_USE = 85065732,
        /**
         * Indicates this device has activated satellite communication. You must close satellite communication
         * to detect hidden camera
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        ERR_SATELLITE_OPEN = 85065733,
        /**
         * Indicates the parameter transferred to the interface is incorrect
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        ERR_PARAMETER_INCORRECT = 85065734,
        /**
         * Detection not started, can not stop or keep
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        ERR_NO_HIDDEN_CAMERA_DETECTION = 85065735,
        /**
         * Detection not started, can not stop or keep
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        ERR_API_CALLED_IN_DIFFERENT_THREAD = 85065736,
        /**
         * Internal system error, ig allocate memory fail, system function fail
         *
         * @syscap SystemCapability.Telephony.CoreService.Sense
         * @since 7
         */
        ERR_INTERNAL_SYSTEM_ERROR = 85065737,
    }

    /**
     * Stop Hidden Camera Detection.
     *
     * returns {@code 0} if stop detecting hidden camera success;
     * returns {@code ERR_NO_HIDDEN_CAMERA_DETECTION} if this device can't stop detecting hidden camera because it do not start;
     * returns {@code ERR_API_CALLED_IN_DIFFERENT_THREAD} if api is called in different thread;
     * @syscap SystemCapability.Telephony.CoreService.Sense
     * @since 7
     */
    function stopHiddenCameraDetection(): Promise<SenseHiddenCameraDetectionRsltCode>;

    /**
     * Keep Hidden Camera Detection.
     *
     * returns {@code 0} if keep detecting hidden camera success;
     * returns {@code ERR_NO_HIDDEN_CAMERA_DETECTION} if this device can't keep detecting hidden camera because it do not start;
     * returns {@code ERR_API_CALLED_IN_DIFFERENT_THREAD} if api is called in different thread;
     * @syscap SystemCapability.Telephony.CoreService.Sense
     * @since 7
     */
    function keepHiddenCameraDetection(): Promise<SenseHiddenCameraDetectionRsltCode>;
}
export default sense;
