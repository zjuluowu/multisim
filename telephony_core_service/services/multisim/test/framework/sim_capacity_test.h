// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <gmock/gmock.h>
#include "host/stubs/parameter_mock.h"
#include "host/probes/capacity_probe.h"
#include <memory>
extern "C" unsigned int SimHostParameterReads();
class SimComponent : public testing::Test {
protected:
    void SetUp() override {
    // Exact counts/order are intentionally unconstrained: queries may repeat.
    EXPECT_CALL(parameters_, Read(testing::StrEq("const.telephony.slotCount"), testing::_, testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly([](const char *, const char *, char *value, unsigned int length) {
            return SimHost::CopyParameterValue("1", value, length);
        });
    EXPECT_CALL(parameters_, Read(testing::StrEq("const.booster.virtual_modem_switch"), testing::_, testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly([](const char *, const char *, char *value, unsigned int length) {
            return SimHost::CopyParameterValue("false", value, length);
        });
    EXPECT_CALL(parameters_, Read(testing::StrEq("const.product.devicetype"), testing::_, testing::_, testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly([](const char *, const char *, char *value, unsigned int length) {
            return SimHost::CopyParameterValue("phone", value, length);
        });
    scope_ = std::make_unique<SimHost::ScopedParameterService>(parameters_);
    }
    int ReadCapacity() { return SimHost::ObserveCapacity(); }
    bool ParameterBoundaryObserved() { return SimHostParameterReads() > 0; }
private:
    testing::StrictMock<SimHost::MockParameterService> parameters_;
    std::unique_ptr<SimHost::ScopedParameterService> scope_;
};
