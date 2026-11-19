#pragma once
#include "Serialization/ArchiveFile.h"
#include <shared_mutex>
#include <mutex>
#include <unordered_set>

namespace oot::assets
{
using AssetId = std::uint64_t;
enum class AssetFormat : std::uint8_t { Binary, Json };

struct AssetMetadata
{
    AssetId id = 0;
    std::string name;
    std::string type;
    std::string path;

    AssetFormat format = AssetFormat::Binary;
    std::uint64_t fileBytes = 0;
    std::vector<std::string> tags;

    void serialize(serialization::Archive& ar) { ar(id, name, type, path, format, fileBytes, tags); }
};

class AssetRegistry
{
public:
    void Upsert(AssetMetadata metadata);
    bool Remove(AssetId id);

    std::optional<AssetMetadata> Find(AssetId id) const;
    std::optional<AssetMetadata> FindPath(const std::string& path) const;
    std::vector<AssetMetadata> FindName(const std::string& name) const;
    std::vector<AssetMetadata> FindType(const std::string& type) const;
    std::vector<AssetMetadata> FindTag(const std::string& tag) const;
    std::vector<AssetMetadata> Snapshot() const;

    void Save(const std::filesystem::path& path) const;
    void Load(const std::filesystem::path& path);

private:
    using Records = std::unordered_map<AssetId, AssetMetadata>;
    using Index = std::unordered_map<std::string, std::vector<AssetId>>;
    struct State
    {
        Records records;
        std::unordered_map<std::string, AssetId> paths;
        Index names, types, tags;
    };

    static State Build(Records records);
    std::vector<AssetMetadata> Query(const Index& index, const std::string& key) const;
    mutable std::shared_mutex mutex_;
    State state_;
};
}