// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace SimGuard
{
enum class ResourceForm
{
    Removable,
    EmbeddedProfile,
    Virtual
};
struct ResourceLocation
{
    ResourceForm form;
    std::optional<int> physicalSlot;
    std::optional<int> port;
    std::string profile;
    std::optional<int> modem;
    std::string virtualEndpoint{};
};
struct TestResource
{
    std::string name;
    ResourceLocation location;
};
// Resource declarations describe test equipment, not production SIM identity.
// Cardinality is independent of the selected Legacy/product capability.
class ResourceCatalog
{
  public:
    void Declare(std::string name, ResourceLocation location)
    {
        if (name.empty())
            throw std::invalid_argument("Empty resource name");
        for (const auto &resource : resources_)
            if (resource.name == name)
                throw std::invalid_argument("Duplicate resource name");
        resources_.push_back({std::move(name), std::move(location)});
    }
    const TestResource &At(const std::string &name) const
    {
        for (const auto &resource : resources_)
            if (resource.name == name)
                return resource;
        throw std::out_of_range("Unknown test resource");
    }
    size_t Size() const { return resources_.size(); }

  private:
    std::vector<TestResource> resources_;
};
} // namespace SimGuard
