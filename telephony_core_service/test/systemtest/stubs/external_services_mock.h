#pragma once
#include <gmock/gmock.h>
#include "compile_declarations.h"
namespace SimHost {
class MockExternalServices {
public:
    MOCK_METHOD(bool, Publish, (const OHOS::EventFwk::CommonEventData &,
        const OHOS::EventFwk::CommonEventPublishInfo &), ());
    MOCK_METHOD(std::shared_ptr<OHOS::DataShare::DataShareHelper>, CreateDataShare,
        (const OHOS::sptr<OHOS::IRemoteObject> &, const std::string &, const std::string &, int, bool), ());
};
class ScopedExternalServices {
public:
    explicit ScopedExternalServices(MockExternalServices &);
    ~ScopedExternalServices();
    ScopedExternalServices(const ScopedExternalServices &) = delete;
    ScopedExternalServices &operator=(const ScopedExternalServices &) = delete;
};
class MockDataShareHelper : public OHOS::DataShare::DataShareHelper {
public:
    MOCK_METHOD(bool, Release, (), (override));
    MOCK_METHOD(int, Insert, (OHOS::Uri &, const OHOS::DataShare::DataShareValuesBucket &), (override));
    MOCK_METHOD(int, Update, (OHOS::Uri &, const OHOS::DataShare::DataSharePredicates &,
        const OHOS::DataShare::DataShareValuesBucket &), (override));
    MOCK_METHOD(int, Delete, (OHOS::Uri &, const OHOS::DataShare::DataSharePredicates &), (override));
    MOCK_METHOD(int, BatchInsert, (OHOS::Uri &, const std::vector<OHOS::DataShare::DataShareValuesBucket> &), (override));
    MOCK_METHOD(std::shared_ptr<OHOS::DataShare::DataShareResultSet>, Query,
        (OHOS::Uri &, const OHOS::DataShare::DataSharePredicates &, std::vector<std::string> &,
         OHOS::DataShare::DatashareBusinessError *), (override));
    MockDataShareHelper()
    {
        ON_CALL(*this, Release()).WillByDefault(testing::Return(false));
        ON_CALL(*this, Insert(testing::_, testing::_)).WillByDefault(testing::Return(-1));
        ON_CALL(*this, Update(testing::_, testing::_, testing::_)).WillByDefault(testing::Return(-1));
        ON_CALL(*this, Delete(testing::_, testing::_)).WillByDefault(testing::Return(-1));
        ON_CALL(*this, BatchInsert(testing::_, testing::_)).WillByDefault(testing::Return(-1));
        ON_CALL(*this, Query(testing::_, testing::_, testing::_, testing::_)).WillByDefault(testing::Return(nullptr));
    }
};
}
