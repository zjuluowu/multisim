// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <map>
#include <chrono>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace SimGuard::Primary {
using Fields = std::map<std::string, std::string>;
// Test port only. Implementations must call real business code, never infer a result.
class Adapter {
public:
    virtual ~Adapter() = default;
    virtual int SetPrimarySlotId(int slot) = 0;
};
inline std::string Json(const std::string &value)
{
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) { const char *hex = "0123456789abcdef"; out << "\\u00" << hex[c >> 4] << hex[c & 15]; }
        else out << c;
    }
    out << '"'; return out.str();
}
inline std::string Json(const Fields &fields)
{
    std::string out = "{"; bool first = true;
    for (const auto &[key, value] : fields) {
        if (!first) out += ','; first = false; out += Json(key) + ':' + Json(value);
    }
    return out + '}';
}
class Observation {
public:
    void Begin() { std::lock_guard<std::mutex> lock(mutex_); events_.clear(); origin_ = std::chrono::steady_clock::now(); active_ = true; }
    void Add(const std::string &kind, Fields fields)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_) {
            fields["at_us"] = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - origin_).count());
            events_.push_back({kind, std::move(fields)});
        }
    }
    // Snapshot is copied, then serialized without holding any production/fixture lock.
    std::string Snapshot(const Fields &parameters, const Fields &publicState)
    {
        std::vector<Event> copy;
        { std::lock_guard<std::mutex> lock(mutex_); copy = events_; }
        std::string out = "{\"schema\":1,\"parameters\":" + Json(parameters) +
            ",\"public\":" + Json(publicState) + ",\"events\":[";
        bool first = true;
        for (const auto &event : copy) {
            if (!first) out += ','; first = false;
            out += "{\"kind\":" + Json(event.kind) + ",\"fields\":" + Json(event.fields) + '}';
        }
        return out + "]}";
    }
private:
    struct Event { std::string kind; Fields fields; };
    std::mutex mutex_;
    std::chrono::steady_clock::time_point origin_;
    bool active_ = false;
    std::vector<Event> events_;
};
} // namespace SimGuard::Primary
