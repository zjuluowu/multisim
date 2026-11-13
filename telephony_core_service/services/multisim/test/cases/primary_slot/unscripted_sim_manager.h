// Generated from real ISimManager; unsupported calls throw, never fabricate business results.
// Interface SHA256: 0456f3753dfafdcb55d5bb18002e5a9626513b3340be806150dba6967b944230
#pragma once
#include "i_sim_manager.h"
#include <stdexcept>
namespace PrimarySlotHost {
using namespace OHOS::Telephony;
using OHOS::sptr;
class UnscriptedSimManager : public OHOS::Telephony::ISimManager {
public:
    bool OnInit(int32_t slotCount) override
    { throw std::logic_error("ISimManager seam: unexpected OnInit"); }
    int32_t InitTelExtraModule(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected InitTelExtraModule"); }
    int32_t HasSimCard(int32_t slotId, bool &hasSimCard) override
    { throw std::logic_error("ISimManager seam: unexpected HasSimCard"); }
    int32_t GetSimState(int32_t slotId, SimState &simState) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimState"); }
    int32_t GetSimIccStatus(int32_t slotId, IccSimStatus &iccStatus) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimIccStatus"); }
    int32_t GetCardType(int32_t slotId, CardType &cardType) override
    { throw std::logic_error("ISimManager seam: unexpected GetCardType"); }
    int32_t SetModemInit(int32_t slotId, bool state) override
    { throw std::logic_error("ISimManager seam: unexpected SetModemInit"); }
    int32_t UnlockPin(int32_t slotId, const std::string &pin, LockStatusResponse &response) override
    { throw std::logic_error("ISimManager seam: unexpected UnlockPin"); }
    int32_t UnlockPuk(int32_t slotId, const std::string &newPin, const std::string &puk, LockStatusResponse &response) override
    { throw std::logic_error("ISimManager seam: unexpected UnlockPuk"); }
    int32_t AlterPin(int32_t slotId, const std::string &newPin, const std::string &oldPin, LockStatusResponse &response) override
    { throw std::logic_error("ISimManager seam: unexpected AlterPin"); }
    int32_t SetLockState(int32_t slotId, const LockInfo &options, LockStatusResponse &response) override
    { throw std::logic_error("ISimManager seam: unexpected SetLockState"); }
    int32_t GetLockState(int32_t slotId, LockType lockType, LockState &lockState) override
    { throw std::logic_error("ISimManager seam: unexpected GetLockState"); }
    int32_t RefreshSimState(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected RefreshSimState"); }
    int32_t UnlockPin2(int32_t slotId, const std::string &pin2, LockStatusResponse &response) override
    { throw std::logic_error("ISimManager seam: unexpected UnlockPin2"); }
    int32_t UnlockPuk2(int32_t slotId, const std::string &newPin2, const std::string &puk2, LockStatusResponse &response) override
    { throw std::logic_error("ISimManager seam: unexpected UnlockPuk2"); }
    int32_t AlterPin2(int32_t slotId, const std::string &newPin2, const std::string &oldPin2, LockStatusResponse &response) override
    { throw std::logic_error("ISimManager seam: unexpected AlterPin2"); }
    int32_t UnlockSimLock(int32_t slotId, const PersoLockInfo &lockInfo, LockStatusResponse &response) override
    { throw std::logic_error("ISimManager seam: unexpected UnlockSimLock"); }
    bool IsSimActive(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected IsSimActive"); }
    int32_t SetActiveSim(int32_t slotId, int32_t enable) override
    { throw std::logic_error("ISimManager seam: unexpected SetActiveSim"); }
    int32_t SetActiveSimSatellite(int32_t slotId, int32_t enable) override
    { throw std::logic_error("ISimManager seam: unexpected SetActiveSimSatellite"); }
    int32_t ResetSimLoadAccount(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected ResetSimLoadAccount"); }
    int32_t GetSimAccountInfo(int32_t slotId, bool denied, IccAccountInfo &info) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimAccountInfo"); }
    int32_t SetDefaultVoiceSlotId(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected SetDefaultVoiceSlotId"); }
    int32_t SetDefaultSmsSlotId(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected SetDefaultSmsSlotId"); }
    int32_t SetDefaultCellularDataSlotId(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected SetDefaultCellularDataSlotId"); }
    int32_t SetPrimarySlotId(int32_t slotId, bool isUserSet) override
    { throw std::logic_error("ISimManager seam: unexpected SetPrimarySlotId"); }
    int32_t SetShowNumber(int32_t slotId, const std::u16string &number) override
    { throw std::logic_error("ISimManager seam: unexpected SetShowNumber"); }
    int32_t SetShowName(int32_t slotId, const std::u16string &name) override
    { throw std::logic_error("ISimManager seam: unexpected SetShowName"); }
    int32_t GetDefaultVoiceSlotId() override
    { throw std::logic_error("ISimManager seam: unexpected GetDefaultVoiceSlotId"); }
    int32_t GetDefaultVoiceSimId(int32_t &simId) override
    { throw std::logic_error("ISimManager seam: unexpected GetDefaultVoiceSimId"); }
    int32_t GetDefaultSmsSlotId() override
    { throw std::logic_error("ISimManager seam: unexpected GetDefaultSmsSlotId"); }
    int32_t GetDefaultSmsSimId(int32_t &simId) override
    { throw std::logic_error("ISimManager seam: unexpected GetDefaultSmsSimId"); }
    int32_t GetDefaultCellularDataSlotId() override
    { throw std::logic_error("ISimManager seam: unexpected GetDefaultCellularDataSlotId"); }
    int32_t GetDefaultCellularDataSimId(int32_t &simId) override
    { throw std::logic_error("ISimManager seam: unexpected GetDefaultCellularDataSimId"); }
    int32_t RegisterSimAccountCallback(const int32_t tokenId, const sptr<SimAccountCallback> &callback) override
    { throw std::logic_error("ISimManager seam: unexpected RegisterSimAccountCallback"); }
    int32_t UnregisterSimAccountCallback(const sptr<SimAccountCallback> &callback) override
    { throw std::logic_error("ISimManager seam: unexpected UnregisterSimAccountCallback"); }
    int32_t GetPrimarySlotId(int32_t &slotId) override
    { throw std::logic_error("ISimManager seam: unexpected GetPrimarySlotId"); }
    int32_t GetShowNumber(int32_t slotId, std::u16string &showNumber) override
    { throw std::logic_error("ISimManager seam: unexpected GetShowNumber"); }
    int32_t GetShowName(int32_t slotId, std::u16string &showName) override
    { throw std::logic_error("ISimManager seam: unexpected GetShowName"); }
    int32_t GetActiveSimAccountInfoList(bool denied, std::vector<IccAccountInfo> &iccAccountInfoList) override
    { throw std::logic_error("ISimManager seam: unexpected GetActiveSimAccountInfoList"); }
    int32_t GetOperatorConfigs(int slotId, OperatorConfig &poc) override
    { throw std::logic_error("ISimManager seam: unexpected GetOperatorConfigs"); }
    int32_t UpdateOperatorConfigs(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected UpdateOperatorConfigs"); }
    int32_t HasOperatorPrivileges(const int32_t slotId, bool &hasOperatorPrivileges) override
    { throw std::logic_error("ISimManager seam: unexpected HasOperatorPrivileges"); }
    int32_t SimAuthentication(int32_t slotId, AuthType authType, const std::string &authData, SimAuthenticationResponse &response) override
    { throw std::logic_error("ISimManager seam: unexpected SimAuthentication"); }
    int32_t GetRadioProtocolTech(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected GetRadioProtocolTech"); }
    void GetRadioProtocol(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected GetRadioProtocol"); }
    int32_t GetRadioProtocolSlotIdByModemId(int32_t modemId) override
    { throw std::logic_error("ISimManager seam: unexpected GetRadioProtocolSlotIdByModemId"); }
    int32_t GetDsdsMode(int32_t &dsdsMode) override
    { throw std::logic_error("ISimManager seam: unexpected GetDsdsMode"); }
    int32_t SetDsdsMode(int32_t dsdsMode) override
    { throw std::logic_error("ISimManager seam: unexpected SetDsdsMode"); }
    int32_t SendSimMatchedOperatorInfo(int32_t slotId, int32_t state, const std::string &operName, const std::string &operKey) override
    { throw std::logic_error("ISimManager seam: unexpected SendSimMatchedOperatorInfo"); }
    int32_t SendEnvelopeCmd(int32_t slotId, const std::string &cmd) override
    { throw std::logic_error("ISimManager seam: unexpected SendEnvelopeCmd"); }
    int32_t SendTerminalResponseCmd(int32_t slotId, const std::string &cmd) override
    { throw std::logic_error("ISimManager seam: unexpected SendTerminalResponseCmd"); }
    int32_t SendCallSetupRequestResult(int32_t slotId, bool accept) override
    { throw std::logic_error("ISimManager seam: unexpected SendCallSetupRequestResult"); }
    int32_t GetSimOperatorNumeric(int32_t slotId, std::u16string &operatorNumeric) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimOperatorNumeric"); }
    int32_t GetISOCountryCodeForSim(int32_t slotId, std::u16string &countryCode) override
    { throw std::logic_error("ISimManager seam: unexpected GetISOCountryCodeForSim"); }
    int32_t GetSimSpn(int32_t slotId, std::u16string &spn) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimSpn"); }
    int32_t GetSimIccId(int32_t slotId, std::u16string &iccId) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimIccId"); }
    int32_t GetIMSI(int32_t slotId, std::u16string &imsi) override
    { throw std::logic_error("ISimManager seam: unexpected GetIMSI"); }
    int32_t GetSpdiPlmns(int32_t slotId, std::set<std::string> &spdiPlmns) override
    { throw std::logic_error("ISimManager seam: unexpected GetSpdiPlmns"); }
    int32_t GetEhPlmns(int32_t slotId, std::set<std::string> &ehPlmns) override
    { throw std::logic_error("ISimManager seam: unexpected GetEhPlmns"); }
    std::u16string GetLocaleFromDefaultSim(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected GetLocaleFromDefaultSim"); }
    int32_t GetSlotId(int32_t simId) override
    { throw std::logic_error("ISimManager seam: unexpected GetSlotId"); }
    int32_t GetSimId(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimId"); }
    int32_t GetSimGid1(int32_t slotId, std::u16string &gid1) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimGid1"); }
    std::u16string GetSimGid2(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimGid2"); }
    int32_t GetOpName(int32_t slotId, std::u16string &opname) override
    { throw std::logic_error("ISimManager seam: unexpected GetOpName"); }
    int32_t GetOpKey(int32_t slotId, std::u16string &opkey) override
    { throw std::logic_error("ISimManager seam: unexpected GetOpKey"); }
    int32_t GetOpKeyExt(int32_t slotId, std::u16string &opkeyExt) override
    { throw std::logic_error("ISimManager seam: unexpected GetOpKeyExt"); }
    int32_t GetSimTelephoneNumber(int32_t slotId, std::u16string &telephoneNumber) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimTelephoneNumber"); }
    std::u16string GetSimTeleNumberIdentifier(const int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimTeleNumberIdentifier"); }
    int32_t GetVoiceMailIdentifier(int32_t slotId, std::u16string &voiceMailIdentifier) override
    { throw std::logic_error("ISimManager seam: unexpected GetVoiceMailIdentifier"); }
    int32_t GetVoiceMailNumber(int32_t slotId, std::u16string &voiceMailNumber) override
    { throw std::logic_error("ISimManager seam: unexpected GetVoiceMailNumber"); }
    int32_t GetVoiceMailCount(int32_t slotId, int32_t &voiceMailCount) override
    { throw std::logic_error("ISimManager seam: unexpected GetVoiceMailCount"); }
    int32_t SetVoiceMailCount(int32_t slotId, int32_t voiceMailCount) override
    { throw std::logic_error("ISimManager seam: unexpected SetVoiceMailCount"); }
    int32_t SetVoiceCallForwarding(int32_t slotId, bool enable, const std::string &number) override
    { throw std::logic_error("ISimManager seam: unexpected SetVoiceCallForwarding"); }
    std::u16string GetSimIst(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimIst"); }
    int ObtainSpnCondition(int32_t slotId, bool roaming, std::string operatorNum) override
    { throw std::logic_error("ISimManager seam: unexpected ObtainSpnCondition"); }
    int32_t SetVoiceMailInfo(int32_t slotId, const std::u16string &mailName, const std::u16string &mailNumber) override
    { throw std::logic_error("ISimManager seam: unexpected SetVoiceMailInfo"); }
    std::u16string GetSimEons(int32_t slotId, const std::string &plmn, int32_t lac, bool longNameRequired) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimEons"); }
    int32_t IsCTSimCard(int32_t slotId, bool &isCTSimCard) override
    { throw std::logic_error("ISimManager seam: unexpected IsCTSimCard"); }
    int32_t AddSmsToIcc(int32_t slotId, int status, std::string &pdu, std::string &smsc) override
    { throw std::logic_error("ISimManager seam: unexpected AddSmsToIcc"); }
    int32_t UpdateSmsIcc(int32_t slotId, int index, int status, std::string &pduData, std::string &smsc) override
    { throw std::logic_error("ISimManager seam: unexpected UpdateSmsIcc"); }
    int32_t DelSmsIcc(int32_t slotId, int index) override
    { throw std::logic_error("ISimManager seam: unexpected DelSmsIcc"); }
    std::vector<std::string> ObtainAllSmsOfIcc(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected ObtainAllSmsOfIcc"); }
    int32_t QueryIccDiallingNumbers(int slotId, int type, std::vector<std::shared_ptr<DiallingNumbersInfo>> &result) override
    { throw std::logic_error("ISimManager seam: unexpected QueryIccDiallingNumbers"); }
    int32_t AddIccDiallingNumbers(int slotId, int type, const std::shared_ptr<DiallingNumbersInfo> &diallingNumber) override
    { throw std::logic_error("ISimManager seam: unexpected AddIccDiallingNumbers"); }
    int32_t DelIccDiallingNumbers(int slotId, int type, const std::shared_ptr<DiallingNumbersInfo> &diallingNumber) override
    { throw std::logic_error("ISimManager seam: unexpected DelIccDiallingNumbers"); }
    int32_t UpdateIccDiallingNumbers(int slotId, int type, const std::shared_ptr<DiallingNumbersInfo> &diallingNumber) override
    { throw std::logic_error("ISimManager seam: unexpected UpdateIccDiallingNumbers"); }
    void RegisterCoreNotify(int32_t slotId, const HANDLE &handler, int what) override
    { throw std::logic_error("ISimManager seam: unexpected RegisterCoreNotify"); }
    void UnRegisterCoreNotify(int32_t slotId, const HANDLE &observerCallBack, int what) override
    { throw std::logic_error("ISimManager seam: unexpected UnRegisterCoreNotify"); }
    int32_t SaveImsSwitch(int32_t slotId, int32_t imsSwitchValue) override
    { throw std::logic_error("ISimManager seam: unexpected SaveImsSwitch"); }
    int32_t QueryImsSwitch(int32_t slotId, int32_t &imsSwitchValue) override
    { throw std::logic_error("ISimManager seam: unexpected QueryImsSwitch"); }
    bool IsSetActiveSimInProgress(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected IsSetActiveSimInProgress"); }
    bool IsSetPrimarySlotIdInProgress() override
    { throw std::logic_error("ISimManager seam: unexpected IsSetPrimarySlotIdInProgress"); }
    int32_t GetSimIO(int32_t slotId, int32_t command, int32_t fileId, const std::string &data, const std::string &path, SimAuthenticationResponse &response) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimIO"); }
    int32_t SavePrimarySlotId(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected SavePrimarySlotId"); }
    bool IsDataShareError() override
    { throw std::logic_error("ISimManager seam: unexpected IsDataShareError"); }
    void ResetDataShareError() override
    { throw std::logic_error("ISimManager seam: unexpected ResetDataShareError"); }
    void UpdateImsCapFromChip(int32_t slotId, const ImsCapFromChip &imsCapFromChip) override
    { throw std::logic_error("ISimManager seam: unexpected UpdateImsCapFromChip"); }
    int32_t GetDefaultMainSlotByIccId() override
    { throw std::logic_error("ISimManager seam: unexpected GetDefaultMainSlotByIccId"); }
    int32_t GetAllSimAccountInfoList(bool denied, std::vector<IccAccountInfo> &iccAccountInfoList) override
    { throw std::logic_error("ISimManager seam: unexpected GetAllSimAccountInfoList"); }
    int32_t GetSimLabel(int32_t slotId, SimLabel &simLabel) override
    { throw std::logic_error("ISimManager seam: unexpected GetSimLabel"); }
    int32_t SetSimLabelIndex(int32_t simId, int32_t simLabelIndex) override
    { throw std::logic_error("ISimManager seam: unexpected SetSimLabelIndex"); }
    int32_t SetSimLabelIndexByIccId(const std::string &iccId, int32_t labelIndex) override
    { throw std::logic_error("ISimManager seam: unexpected SetSimLabelIndexByIccId"); }
    int32_t NotifySimSlotsMapping(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected NotifySimSlotsMapping"); }
    int32_t InsertEsimData(const std::string &iccId, int32_t esimLabel, const std::string &operatorName) override
    { throw std::logic_error("ISimManager seam: unexpected InsertEsimData"); }
    bool IsEsim(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected IsEsim"); }
    int32_t ClearSimLabel(SimType simType) override
    { throw std::logic_error("ISimManager seam: unexpected ClearSimLabel"); }
    int32_t UpdateSimPresent(int32_t slotId, bool isShowPresent) override
    { throw std::logic_error("ISimManager seam: unexpected UpdateSimPresent"); }
    int32_t UpdateEsimOpName(const std::string &iccId, const std::string &operatorName) override
    { throw std::logic_error("ISimManager seam: unexpected UpdateEsimOpName"); }
    void CheckIfNeedSwitchMainSlotId(bool isUserSet) override
    { throw std::logic_error("ISimManager seam: unexpected CheckIfNeedSwitchMainSlotId"); }
    int32_t SetIccCardState(int32_t slotId, int32_t simStatus) override
    { throw std::logic_error("ISimManager seam: unexpected SetIccCardState"); }
    int32_t SetTargetPrimarySlotId(bool isDualCard, int32_t primarySlotId) override
    { throw std::logic_error("ISimManager seam: unexpected SetTargetPrimarySlotId"); }
    bool IsModemInitDone(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected IsModemInitDone"); }
    int32_t GetMaxSimCount() override
    { throw std::logic_error("ISimManager seam: unexpected GetMaxSimCount"); }
    int32_t GetRealSimCount() override
    { throw std::logic_error("ISimManager seam: unexpected GetRealSimCount"); }
    void SetMatchSimStateTracker(int8_t matchSimStateTracker, int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected SetMatchSimStateTracker"); }
    int32_t SwapM0M2SimCards(int32_t slotId) override
    { throw std::logic_error("ISimManager seam: unexpected SwapM0M2SimCards"); }
    std::string GetOverseasCarrierBySimInfo(const SimCardInfo &simCardInfo) override
    { throw std::logic_error("ISimManager seam: unexpected GetOverseasCarrierBySimInfo"); }
    int32_t SaveCardFileDetectData(const CardFileDetectData &data) override
    { throw std::logic_error("ISimManager seam: unexpected SaveCardFileDetectData"); }
    int32_t GetAllSimCardInfo(std::vector<SimCardInfo> &results) override
    { throw std::logic_error("ISimManager seam: unexpected GetAllSimCardInfo"); }
    void SetSpecifiedIccidBySlotId(int32_t slotId, std::string &iccid) override
    { throw std::logic_error("ISimManager seam: unexpected SetSpecifiedIccidBySlotId"); }
};
}
