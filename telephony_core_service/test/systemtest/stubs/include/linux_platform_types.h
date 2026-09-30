// Minimal native-Linux external API subset. Sources and deviations: upstream_external_contracts.md.
#pragma once
#include "compile_declarations.h"
#include "datashare_value_object.h"
#include "datashare_values_bucket.h"
#include "datashare_errno.h"
#include "global_configuration_key.h"
#include <algorithm>
#include <ostream>
#include <stdexcept>
#include <variant>
namespace OHOS {
class Uri {
public:
    explicit Uri(const std::string &value) : value_(value) {}
    std::string ToString() const { return value_; }
private:
    std::string value_;
};
namespace AAFwk {
class Want {
public:
    Want &SetAction(const std::string &action) { action_ = action; return *this; }
    std::string GetAction() const { return action_; }
    Want &SetBundle(const std::string &bundle) { bundle_ = bundle; return *this; }
    Want &SetElementName(const std::string &bundle, const std::string &ability)
    { bundle_ = bundle; ability_ = ability; return *this; }
    Want &SetParam(const std::string &key, int value) { params_[key] = value; return *this; }
    Want &SetParam(const std::string &key, bool value) { params_[key] = value; return *this; }
    Want &SetParam(const std::string &key, const std::string &value) { params_[key] = value; return *this; }
    int GetIntParam(const std::string &key, int fallback) const { return Read(key, fallback); }
    bool GetBoolParam(const std::string &key, bool fallback) const { return Read(key, fallback); }
    std::string GetStringParam(const std::string &key) const { return Read(key, std::string()); }
private:
    template<class T> T Read(const std::string &key, T fallback) const
    {
        auto it = params_.find(key);
        if (it == params_.end()) return fallback;
        if (const auto *value = std::get_if<T>(&it->second)) return *value;
        return fallback;
    }
    std::string action_, bundle_, ability_;
    std::map<std::string, std::variant<int, bool, std::string>> params_;
};
inline void PrintTo(const Want &, std::ostream *stream) { *stream << "Want(redacted)"; }
}
namespace EventFwk {
using Want = AAFwk::Want;
class CommonEventData {
public:
    CommonEventData() = default;
    explicit CommonEventData(const Want &want) : want_(want) {}
    CommonEventData(const Want &want, int32_t code, const std::string &data) : want_(want), code_(code), data_(data) {}
    void SetWant(const Want &want) { want_ = want; }
    const Want &GetWant() const { return want_; }
    void SetCode(const int32_t &code) { code_ = code; }
    int32_t GetCode() const { return code_; }
    void SetData(const std::string &data) { data_ = data; }
    std::string GetData() const { return data_; }
private:
    Want want_;
    int32_t code_ = 0;
    std::string data_;
};
inline void PrintTo(const CommonEventData &, std::ostream *stream) { *stream << "CommonEventData(redacted)"; }
class CommonEventPublishInfo {
public:
    void SetOrdered(bool value) { ordered_ = value; }
    bool IsOrdered() const { return ordered_; }
    void SetSticky(bool value) { sticky_ = value; }
    bool IsSticky() const { return sticky_; }
    void SetSubscriberPermissions(const std::vector<std::string> &); // not implemented
private:
    bool ordered_ = false, sticky_ = false;
};
class MatchingSkills {
public:
    void AddEvent(const std::string &action) { actions_.push_back(action); }
private:
    std::vector<std::string> actions_;
};
class CommonEventSubscribeInfo {
public:
    explicit CommonEventSubscribeInfo(const MatchingSkills &) {}
    void SetUserId(int32_t); // declaration only: no subscriber runtime
    void SetPermission(const std::string &);
};
class CommonEventSubscriber {
public:
    explicit CommonEventSubscriber(const CommonEventSubscribeInfo &) {}
    virtual ~CommonEventSubscriber() = default;
    virtual void OnReceiveEvent(const CommonEventData &) = 0;
};
class CommonEventManager {
public:
    static bool PublishCommonEvent(const CommonEventData &);
    static bool PublishCommonEvent(const CommonEventData &, const CommonEventPublishInfo &,
        const std::shared_ptr<CommonEventSubscriber> & = nullptr);
    static bool SubscribeCommonEvent(const std::shared_ptr<CommonEventSubscriber> &);
    static bool UnSubscribeCommonEvent(const std::shared_ptr<CommonEventSubscriber> &);
};
class CommonEventSupport {
public:
    inline static const std::string COMMON_EVENT_SIM_STATE_CHANGED = "usual.event.SIM_STATE_CHANGED";
};
}
namespace DataShare {
class DatashareBusinessError;
class DataSharePredicates {
public:
    struct Operation { std::string kind, field; DataShareValueObject value; };
    DataSharePredicates *EqualTo(const std::string &field, const DataShareValueObject &value)
    { operations_.push_back({"EqualTo", field, value}); return this; }
    DataSharePredicates *GreaterThan(const std::string &field, const DataShareValueObject &value)
    { operations_.push_back({"GreaterThan", field, value}); return this; }
    DataSharePredicates *NotEqualTo(const std::string &field, const DataShareValueObject &value)
    { operations_.push_back({"NotEqualTo", field, value}); return this; }
    DataSharePredicates *Contains(const std::string &field, const std::string &value)
    { operations_.push_back({"Contains", field, DataShareValueObject(value)}); return this; }
    // Captured boundary requests only; never evaluated as SIM business logic.
    const std::vector<Operation> &Requests() const { return operations_; }
private:
    std::vector<Operation> operations_;
};
inline void PrintTo(const DataSharePredicates &, std::ostream *stream) { *stream << "Predicates(redacted)"; }
inline void PrintTo(const DataShareValuesBucket &, std::ostream *stream) { *stream << "ValuesBucket(redacted)"; }
class DataShareResultSet {
public:
    using Row = std::vector<DataShareValueObject::Type>;
    DataShareResultSet(std::vector<std::string> columns, std::vector<Row> rows)
        : columns_(std::move(columns)), rows_(std::move(rows))
    {
        for (const auto &row : rows_) if (row.size() != columns_.size())
            throw std::invalid_argument("External row width does not match columns");
    }
    int GetRowCount(int &count) { if (closed_) return E_ERROR; count = rows_.size(); return E_OK; }
    int GoToRow(int position)
    {
        if (closed_) return E_ERROR;
        position_ = position < 0 ? -1 : std::min(position, static_cast<int>(rows_.size()));
        return position >= 0 && position < static_cast<int>(rows_.size()) ? E_OK : E_ERROR;
    }
    int GoToFirstRow() { return GoToRow(0); }
    int GoToNextRow() { return GoToRow(position_ + 1); }
    int GetColumnIndex(const std::string &name, int &index)
    {
        if (closed_) return E_ERROR;
        auto it = std::find(columns_.begin(), columns_.end(), name);
        if (it == columns_.end()) { index = -1; return E_ERROR; }
        index = it - columns_.begin(); return E_OK;
    }
    int GetString(int index, std::string &value) { return Read(index, value); }
    int GetInt(int index, int &value)
    {
        int64_t wide = 0;
        int result = Read(index, wide);
        if (result == E_OK) value = static_cast<int>(wide);
        return result;
    }
    int Close() { closed_ = true; rows_.clear(); return E_OK; }
private:
    template<class T> int Read(int index, T &value)
    {
        if (closed_ || position_ < 0 || position_ >= static_cast<int>(rows_.size())) return E_ERROR;
        if (index < 0 || index >= static_cast<int>(columns_.size())) return E_INVALID_COLUMN_INDEX;
        const auto *typed = std::get_if<T>(&rows_[position_][index]);
        if (typed == nullptr) return E_INVALID_OBJECT_TYPE;
        value = *typed; return E_OK;
    }
    std::vector<std::string> columns_;
    std::vector<Row> rows_;
    int position_ = -1;
    bool closed_ = false;
};
class DataShareHelper {
public:
    virtual ~DataShareHelper() = default;
    static std::shared_ptr<DataShareHelper> Creator(const sptr<IRemoteObject> &, const std::string &,
        const std::string & = "", int = 2, bool = false);
    virtual bool Release() = 0;
    virtual int Insert(Uri &, const DataShareValuesBucket &) = 0;
    virtual int Update(Uri &, const DataSharePredicates &, const DataShareValuesBucket &) = 0;
    virtual int Delete(Uri &, const DataSharePredicates &) = 0;
    virtual int BatchInsert(Uri &, const std::vector<DataShareValuesBucket> &) = 0;
    virtual std::shared_ptr<DataShareResultSet> Query(Uri &, const DataSharePredicates &,
        std::vector<std::string> &, DatashareBusinessError * = nullptr) = 0;
};
}
namespace AppExecFwk {
class Configuration { public: bool AddItem(const std::string &, const std::string &); };
class AppMgrClient { public: int32_t UpdateConfiguration(const Configuration &); };
}
}
