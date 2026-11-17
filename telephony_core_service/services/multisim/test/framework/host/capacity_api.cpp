#include "sim_test.h"
#include "parameter_mock.h"
#include "capacity_probe.h"
namespace SimGuard {
extern "C" unsigned int SimHostParameterReads();
struct SimTest::Environment {
    testing::StrictMock<SimHost::MockParameterService> parameters_;
    std::unique_ptr<SimHost::ScopedParameterService> scope_;
    Environment() {
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
};
SimTest::SimTest()=default;
SimTest::~SimTest()=default;
void SimTest::SetUp(){environment_=std::make_unique<Environment>();}
void SimTest::TearDown(){environment_.reset();}
int SimTest::ReadCapacity(){return SimHost::ObserveCapacity();}
bool SimTest::ParameterBoundaryObserved(){return SimHostParameterReads()>0;}
}
