#include "Assets/AssetRegistry.h"
#include <algorithm>

namespace oot::assets
{
namespace
{
    std::string Normalize(const std::string& value)
    {
        const std::u8string utf8Path(value.begin(), value.end());
        const auto path = std::filesystem::path(utf8Path).lexically_normal();
        if (value.find('\0') != std::string::npos || path.empty() || path == "." || path.has_root_path())
        {
            throw serialization::ArchiveError("Asset path must be relative");
        }

        for (const auto& part : path)
        {
            if (part == "..")
            {
                throw serialization::ArchiveError("Asset path escapes its root");
            }
        }

        const auto utf8 = path.generic_u8string();
        return std::string(utf8.begin(), utf8.end());
    }

    struct RegistryDocument
    {
        std::uint32_t version = 1;
        std::vector<AssetMetadata> assets;
        void serialize(serialization::Archive& ar)
        {
            ar(version, assets);
        }
    };
}

AssetRegistry::State AssetRegistry::Build(Records records)
{
    State state;
    state.records = std::move(records);
    for (auto& [id, asset] : state.records)
    {
        if (!id || asset.id != id || asset.name.empty() || asset.type.empty() || (asset.format != AssetFormat::Binary && asset.format != AssetFormat::Json))
        {
            throw serialization::ArchiveError("Invalid asset metadata");
        }

        asset.path = Normalize(asset.path);
        if (!state.paths.emplace(asset.path, id).second)
        {
            throw serialization::ArchiveError("Asset path already registered");
        }

        state.names[asset.name].push_back(id);
        state.types[asset.type].push_back(id);
        std::sort(asset.tags.begin(), asset.tags.end());
        asset.tags.erase(std::unique(asset.tags.begin(), asset.tags.end()), asset.tags.end());

        for (const auto& tag : asset.tags)
        {
            state.tags[tag].push_back(id);
        }
    }
        
    return state;
}

void AssetRegistry::Upsert(AssetMetadata metadata)
{
    std::unique_lock lock(mutex_);
    auto records = state_.records;
    records[metadata.id] = std::move(metadata);
    auto next = Build(std::move(records));
    std::swap(state_, next);
}

bool AssetRegistry::Remove(AssetId id)
{
    std::unique_lock lock(mutex_);
    auto records = state_.records;
    if (!records.erase(id))
    {
        return false;
    }

    auto next = Build(std::move(records));
    std::swap(state_, next);
    return true;
}

std::optional<AssetMetadata> AssetRegistry::Find(AssetId id) const
{
    std::shared_lock lock(mutex_);
    const auto it = state_.records.find(id);
    if (it == state_.records.end())
    {
        return {};
    }

    return it->second;
}

std::optional<AssetMetadata> AssetRegistry::FindPath(const std::string& path) const
{
    const auto key = Normalize(path);
    std::shared_lock lock(mutex_);
    const auto it = state_.paths.find(key);
    if (it == state_.paths.end())
    {
        return {};
    }

    return state_.records.at(it->second);
}

std::vector<AssetMetadata> AssetRegistry::Query(const Index& index, const std::string& key) const
{
    std::vector<AssetMetadata> result;
    if (const auto it = index.find(key); it != index.end())
    {
        for (const auto id : it->second)
        {
            result.push_back(state_.records.at(id));
        }
    }

    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return result;
}

std::vector<AssetMetadata> AssetRegistry::FindName(const std::string& name) const
{
    std::shared_lock lock(mutex_);
    return Query(state_.names, name);
}

std::vector<AssetMetadata> AssetRegistry::FindType(const std::string& type) const
{
    std::shared_lock lock(mutex_);
    return Query(state_.types, type);
}

std::vector<AssetMetadata> AssetRegistry::FindTag(const std::string& tag) const
{
    std::shared_lock lock(mutex_);
    return Query(state_.tags, tag);
}

std::vector<AssetMetadata> AssetRegistry::Snapshot() const
{
    std::shared_lock lock(mutex_);
    std::vector<AssetMetadata> result;
    for (const auto& [id, asset] : state_.records)
    {
        result.push_back(asset);
    }

    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b)
        {
            return a.id < b.id;
        });

    return result;
}

void AssetRegistry::Save(const std::filesystem::path& path) const
{
    RegistryDocument document{1, Snapshot()};
    serialization::SaveJson(path, document);
}

void AssetRegistry::Load(const std::filesystem::path& path)
{
    RegistryDocument document;
    serialization::LoadJson(path, document);
    if (document.version != 1)
    {
        throw serialization::ArchiveError("Unknown asset registry version");
    }

    Records records;
    for (auto& asset : document.assets)
    {
        if (!records.emplace(asset.id, std::move(asset)).second)
        {
            throw serialization::ArchiveError("Duplicate asset ID");
        }
    }

    auto next = Build(std::move(records));
    std::unique_lock lock(mutex_);
    std::swap(state_, next);
}
}