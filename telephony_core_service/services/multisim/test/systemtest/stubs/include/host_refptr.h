// Controlled local ownership subset of c_utils refbase.h@3bdc11e0.
// Shared control blocks replace intrusive OHOS counts; no Binder/proxy or ABI compatibility.
#pragma once
#include <memory>
#include <type_traits>

namespace OHOS {
template<class T> class sptr : public std::shared_ptr<T> {
public:
    using Base = std::shared_ptr<T>;
    using Base::operator=;
    sptr() = default;
    sptr(std::nullptr_t) : Base(nullptr) {}
    sptr(T *value) : Base(value) {} // Adopt newly allocated local objects only.
    template<class U> sptr(const std::shared_ptr<U> &value) : Base(value) {}
    template<class U> sptr(const sptr<U> &value) : Base(static_cast<const std::shared_ptr<U> &>(value)) {}
    sptr &operator=(T *value) { this->reset(value); return *this; }
    T *GetRefPtr() const { return this->get(); }
};

template<class T> class wptr {
public:
    wptr() = default;
    wptr(std::nullptr_t) {}
    template<class U> wptr(const sptr<U> &value)
        : weak_(static_cast<const std::shared_ptr<U> &>(value)), identity_(value.get()) {}
    template<class U> wptr(const std::shared_ptr<U> &value) : weak_(value), identity_(value.get()) {}
    wptr &operator=(std::nullptr_t) { weak_.reset(); identity_ = nullptr; return *this; }
    T *GetRefPtr() const { return identity_; }
    bool operator==(const T *value) const { return identity_ == value; }
    bool operator!=(const T *value) const { return !(*this == value); }
    sptr<T> promote() const { return sptr<T>(weak_.lock()); }
private:
    std::weak_ptr<T> weak_;
    T *identity_ = nullptr; // Identity only; never dereference after promotion fails.
};

template<class T, class U> sptr<T> iface_cast(const sptr<U> &object)
{
    // Local objects must implement the requested interface. Remote Proxy construction is unsupported.
    return sptr<T>(std::dynamic_pointer_cast<T>(static_cast<const std::shared_ptr<U> &>(object)));
}
}
