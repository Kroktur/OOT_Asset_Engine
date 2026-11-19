#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace sample::arthur
{
// Plain boundary object: Arthur's Archive/BaseObject never leak into Noe's TU.
struct AssetData
{
    int value = 0;
    std::string name;
    std::map<std::string, std::vector<int>> values;
    bool operator==(const AssetData&) const = default;
};
void Save(const std::filesystem::path& path, AssetData data, bool json);
AssetData Load(const std::filesystem::path& path, bool json);
void VerifyRoundTrips(const std::filesystem::path& directory);
}
