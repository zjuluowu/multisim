/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */
#ifndef TELEPHONY_EXT_I_HIDDEN_CAMERA_DETECTION_CALLBACK_H
#define TELEPHONY_EXT_I_HIDDEN_CAMERA_DETECTION_CALLBACK_H

#include "errors.h"
#include "parcel.h"
#include "iremote_broker.h"

namespace OHOS {
namespace Telephony {

/**
 * Hidden camera detection timeout period configuration.
 * After the timer expires, the detection will automatically stop and release resources.
 */
enum class HiddenCameraDetectionTimeoutPeriod : uint32_t {
    PERIOD_16S = 0,   // 16 seconds
    PERIOD_32S = 1,   // 32 seconds
    PERIOD_64S = 2,   // 64 seconds
    PERIOD_128S = 3,  // 128 seconds
    PERIOD_RESERVED = 31
};

/**
 * Result codes for Hidden Camera Detection.
 */
enum class SenseHiddenCameraDetectionRsltCode : uint32_t {
    SUCCESS = 0,
    ERR_NOT_SUPPORT_HIDDEN_CAMERA_DETECTION = static_cast<uint32_t>(ErrCodeOffset(SUBSYS_TELEPONY, 0x12)),
    ERR_HIDDEN_CAMERA_DETECTION_USE,
    ERR_NO_MODEM_CAN_USE,
    ERR_SKY_TONE_USE,
    ERR_D2D_USE,
    ERR_SATELLITE_OPEN,
    ERR_PARAMETER_INCORRECT,
    ERR_NO_HIDDEN_CAMERA_DETECTION,
    ERR_API_CALLED_IN_DIFFERENT_THREAD,
    ERR_INTERNAL_SYSTEM_ERROR
};

/**
 * Hidden camera detection configuration parameters.
 */
struct HiddenCameraDetectionConfig : public Parcelable {
    HiddenCameraDetectionTimeoutPeriod period = HiddenCameraDetectionTimeoutPeriod::PERIOD_16S;

    bool Marshalling(Parcel &parcel) const override
    {
        return parcel.WriteUint32(static_cast<uint32_t>(period));
    }

    static HiddenCameraDetectionConfig Unmarshalling(Parcel &parcel)
    {
        HiddenCameraDetectionConfig config;
        uint32_t periodVal = 0;
        if (parcel.ReadUint32(periodVal)) {
            config.period = static_cast<HiddenCameraDetectionTimeoutPeriod>(periodVal);
        }
        return config;
    }
};

/**
 * Hidden camera detection report info.
 */
struct HiddenCameraDetectionRptInfo : public Parcelable {
    uint32_t stage = 0;
    uint32_t psdScore = 0;

    bool Marshalling(Parcel &parcel) const override
    {
        if (!parcel.WriteUint32(stage)) {
            return false;
        }
        if (!parcel.WriteUint32(psdScore)) {
            return false;
        }
        return true;
    }

    bool ReadFromParcel(Parcel &parcel)
    {
        if (!parcel.ReadUint32(stage)) {
            return false;
        }
        if (!parcel.ReadUint32(psdScore)) {
            return false;
        }
        return true;
    }

    static std::shared_ptr<HiddenCameraDetectionRptInfo> Unmarshalling(Parcel &parcel)
    {
        auto param = std::make_shared<HiddenCameraDetectionRptInfo>();
        if (param == nullptr || !param->ReadFromParcel(parcel)) {
            return nullptr;
        }
        return param;
    }

    bool operator==(const HiddenCameraDetectionRptInfo &p) const
    {
        return (this->stage == p.stage && this->psdScore == p.psdScore);
    }
};

class IHiddenCameraDetectionCallback : public IRemoteBroker {
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"OHOS.Telephony.IHiddenCameraDetectionCallback");

    enum {
        ON_HIDDEN_CAMERA_DETECTION_STATUS_CHANGED = 0,
    };

    virtual ~IHiddenCameraDetectionCallback() = default;

    /**
     * Notify when hidden camera detection status changes.
     * @param rptInfo The current detection report info containing stage and psdScore.
     * @return Returns 0 on success, non-zero on failure.
     */
    virtual int32_t OnHiddenCameraDetectionStatusChanged(const HiddenCameraDetectionRptInfo& rptInfo) = 0;
};

}  // namespace Telephony
}  // namespace OHOS

#endif // TELEPHONY_EXT_I_HIDDEN_CAMERA_DETECTION_CALLBACK_H
