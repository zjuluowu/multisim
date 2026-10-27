#include <gtest/gtest.h>
#include "host_refptr.h"

namespace {
struct Tracked {
    explicit Tracked(int &destroyed) : destroyed_(destroyed) {}
    virtual ~Tracked() { ++destroyed_; }
    int &destroyed_;
};
struct LocalInterface { virtual ~LocalInterface() = default; };
struct LocalObject : Tracked, LocalInterface { using Tracked::Tracked; };
struct OtherInterface { virtual ~OtherInterface() = default; };
}

TEST(NativePointerSubset, SharedOwnership)
{
    RecordProperty("case_id", "INFRA-PTR-001");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "PTR-OWNERSHIP");
    int destroyed = 0;
    {
        OHOS::sptr<LocalObject> original = new LocalObject(destroyed);
        OHOS::sptr<Tracked> copy = original;
        EXPECT_TRUE(copy.GetRefPtr() == original.GetRefPtr()) << "PTR-OWNERSHIP";
        original = nullptr;
        EXPECT_EQ(destroyed, 0) << "PTR-OWNERSHIP";
        EXPECT_TRUE(copy != nullptr) << "PTR-OWNERSHIP";
    }
    EXPECT_EQ(destroyed, 1) << "PTR-OWNERSHIP";
}

TEST(NativePointerSubset, WeakIdentityAndPromotion)
{
    RecordProperty("case_id", "INFRA-PTR-002");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "PTR-WEAK");
    int destroyed = 0;
    OHOS::wptr<Tracked> weak;
    {
        OHOS::sptr<Tracked> owner = new Tracked(destroyed);
        weak = OHOS::wptr<Tracked>(owner);
        EXPECT_TRUE(weak.promote().GetRefPtr() == owner.GetRefPtr()) << "PTR-WEAK";
    }
    EXPECT_EQ(destroyed, 1) << "PTR-WEAK";
    EXPECT_TRUE(weak != nullptr) << "PTR-WEAK"; // Identity is not an alive-object assertion.
    EXPECT_TRUE(weak.promote() == nullptr) << "PTR-WEAK";
    weak = nullptr;
    EXPECT_TRUE(weak == nullptr) << "PTR-WEAK";
}

TEST(NativePointerSubset, LocalCastRequiresImplementedInterface)
{
    RecordProperty("case_id", "INFRA-PTR-003");
    RecordProperty("level", "INFRA");
    RecordProperty("oracle_id", "PTR-LOCAL-CAST");
    int destroyed = 0;
    {
        OHOS::sptr<Tracked> object = new LocalObject(destroyed);
        auto supported = OHOS::iface_cast<LocalInterface>(object);
        auto unsupported = OHOS::iface_cast<OtherInterface>(object);
        EXPECT_TRUE(supported != nullptr) << "PTR-LOCAL-CAST";
        EXPECT_TRUE(unsupported == nullptr) << "PTR-LOCAL-CAST";
        OHOS::sptr<Tracked> missing;
        EXPECT_TRUE(OHOS::iface_cast<LocalInterface>(missing) == nullptr) << "PTR-LOCAL-CAST";
    }
    EXPECT_EQ(destroyed, 1) << "PTR-LOCAL-CAST";
}
