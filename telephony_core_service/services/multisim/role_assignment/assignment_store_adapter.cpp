/*
 * Copyright (c) 2026 multisim contributors.
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
#include "role_assignment/assignment_store_adapter.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace OHOS::Telephony::MultiSim {
namespace {
constexpr auto kPrimarySlot = "persist.telephony.MainSlotId";
constexpr auto kPrimaryIccid = "persist.telephony.MainCard.Iccid";
constexpr auto kDefaultDataSlot = "persist.telephony.MainCellularDataSlotId";
constexpr auto kUserPreference = "persist.telephony.userPreferPrimarySlot";
constexpr std::array<uint32_t, 64> k {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
uint32_t Rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
std::string Sha256Hex(const std::string &input)
{
    std::vector<uint8_t> data(input.begin(), input.end());
    const uint64_t bitLength = static_cast<uint64_t>(data.size()) * 8;
    data.push_back(0x80);
    while (data.size() % 64 != 56) { data.push_back(0); }
    for (int shift = 56; shift >= 0; shift -= 8) {
        data.push_back(static_cast<uint8_t>((bitLength >> shift) & 0xff));
    }
    std::array<uint32_t, 8> h {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    for (size_t offset = 0; offset < data.size(); offset += 64) {
        uint32_t w[64] {};
        for (size_t i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(data[offset + 4 * i]) << 24) |
                (static_cast<uint32_t>(data[offset + 4 * i + 1]) << 16) |
                (static_cast<uint32_t>(data[offset + 4 * i + 2]) << 8) |
                static_cast<uint32_t>(data[offset + 4 * i + 3]);
        }
        for (size_t i = 16; i < 64; ++i) {
            const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], hh=h[7];
        for (size_t i = 0; i < 64; ++i) {
            const uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
            const uint32_t ch = (e & f) ^ ((~e) & g);
            const uint32_t t1 = hh + s1 + ch + k[i] + w[i];
            const uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = s0 + maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    for (uint32_t part : h) { out << std::setw(8) << part; }
    return out.str();
}
bool LegacySlotTextValid(const std::string &text)
{
    if (text.empty()) { return false; }
    for (char digit : text) {
        if (digit < '0' || digit > '9') { return false; }
    }
    return true;
}
} // namespace

C0ParameterOutcome AssignmentParameterPort::WriteC0Parameter(const C0ParameterOperation &,
    const std::string &key, const std::string &value)
{
    const auto call = Write(key, value);
    return {call, call.callFinished && call.parameterResult == 0 ?
        EffectKnowledge::ConfirmedApplied : EffectKnowledge::Unknown};
}

C0ParameterOutcome AssignmentStoreAdapter::WriteC0Parameter(const C0ParameterOperation &operation, SlotId slot,
    const std::string &rawIccid, bool userSet)
{
    switch (operation.key) {
        case C0ParameterKey::Preference:
            return parameters_.WriteC0Parameter(operation, kUserPreference, userSet ? "true" : "false");
        case C0ParameterKey::PrimarySlot:
            return parameters_.WriteC0Parameter(operation, kPrimarySlot, std::to_string(slot.legacySlotId));
        case C0ParameterKey::PrimaryIccid:
            return parameters_.WriteC0Parameter(operation, kPrimaryIccid, Sha256Hex(rawIccid));
        case C0ParameterKey::DefaultData:
            return parameters_.WriteC0Parameter(operation, kDefaultDataSlot, std::to_string(slot.legacySlotId));
        case C0ParameterKey::Count:
            return {};
    }
    return {};
}

std::optional<C0ParameterEvidence> AssignmentStoreAdapter::ReadC0ParameterEvidence(
    const C0ParameterOperation &operation)
{
    return parameters_.ReadC0ParameterEvidence(operation);
}

RoleStoreResult AssignmentStoreAdapter::WriteDefaultRole(const SetRoleCommand &)
{ return {-1, false, true}; }

PrimaryParameterSnapshot AssignmentStoreAdapter::ReadPrimaryParameters()
{
    PrimaryParameterSnapshot result;
    const auto slot = parameters_.Read(kPrimarySlot, "", result.slotText);
    const auto iccid = parameters_.Read(kPrimaryIccid, "", result.mainIccidText);
    if (!slot.callFinished || !iccid.callFinished || slot.parameterResult != 0 || iccid.parameterResult != 0) {
        result = {};
    }
    return result;
}

AssignmentParameterResult AssignmentStoreAdapter::WritePrimaryParameters(SlotId slot, const std::string &rawIccid)
{
    const auto slotWrite = parameters_.Write(kPrimarySlot, std::to_string(slot.legacySlotId));
    if (!slotWrite.callFinished || rawIccid.empty()) { return slotWrite; }
    const auto iccidWrite = parameters_.Write(kPrimaryIccid, Sha256Hex(rawIccid));
    if (!iccidWrite.callFinished) { return iccidWrite; }
    // Preserve both Legacy writes. The completed parameter errors are intentionally
    // observable to A, but C0's Legacy outer contract does not propagate them.
    return iccidWrite.parameterResult != 0 ? iccidWrite : slotWrite;
}

AssignmentParameterResult AssignmentStoreAdapter::RestorePrimaryIfLegacyValid(const PrimaryParameterSnapshot &old)
{
    if (!LegacySlotTextValid(old.slotText)) { return {0, true}; }
    const auto slot = parameters_.Write(kPrimarySlot, old.slotText);
    if (!slot.callFinished) { return slot; }
    const auto iccid = parameters_.Write(kPrimaryIccid, old.mainIccidText);
    if (!iccid.callFinished) { return iccid; }
    return iccid.parameterResult != 0 ? iccid : slot;
}

AssignmentParameterResult AssignmentStoreAdapter::WriteDefaultData(SlotId slot)
{ return parameters_.Write(kDefaultDataSlot, std::to_string(slot.legacySlotId)); }

AssignmentParameterResult AssignmentStoreAdapter::WriteUserPrimaryPreference(bool userSet)
{ return parameters_.Write(kUserPreference, userSet ? "true" : "false"); }
} // namespace OHOS::Telephony::MultiSim
