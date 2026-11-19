#pragma once
#include "AssetRegistry.h"
#include <condition_variable>
#include <deque>
#include <future>
#include <thread>
#include <typeindex>

namespace oot::assets
{
enum class AssetState { Unloaded, Loading, Ready, Failed };

template<class T> class AssetFuture
{
public:
    explicit AssetFuture(std::shared_future<std::shared_ptr<void>> future) : future_(std::move(future)) 
    {
    }

    std::shared_ptr<T> get() const
    {
        return std::static_pointer_cast<T>(future_.get());
    }

    void wait() const
    {
        future_.wait();
    }

    bool ready() const
    {
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

private:
    std::shared_future<std::shared_ptr<void>> future_;
};

struct AssetManagerOptions
{
    std::size_t workerCount = 2;
    std::size_t maxPendingLoads = 128;
    std::size_t maxCachedAssets = 64;
};

class AssetManager
{
public:
    AssetManager(const AssetRegistry& registry, std::filesystem::path root, AssetManagerOptions options = {});
    ~AssetManager();
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    template<class T>
    void RegisterLoader(std::string type, std::function<std::shared_ptr<T>(const AssetMetadata&, const std::filesystem::path&)> loader)
    {
        if (type.empty() || !loader)
        {
            throw serialization::ArchiveError("Invalid asset loader");
        }

        std::lock_guard lock(mutex_);
        if (!loaders_.emplace(std::move(type), Loader{typeid(T), [loader = std::move(loader)](const auto& metadata, const auto& path) -> std::shared_ptr<void>
            {
                return loader(metadata, path);
            }}).second)

            throw serialization::ArchiveError("Asset loader already registered");
    }

    template<class T>
    void RegisterArchiveType(std::string type = OOT::TypeInfo::Name<T>(), serialization::ArchiveSetup setup = {}, serialization::ArchiveLimits limits = {})
    {
        RegisterLoader<T>(std::move(type), [setup = std::move(setup), limits](const auto& metadata, const auto& path)
        {
            if (metadata.format == AssetFormat::Json)
            {
                return ReadNew<T, serialization::TextArchive>(path, setup, limits);
            }

            return ReadNew<T, serialization::BinaryArchive>(path, setup, limits);
        });
    }

    template<class T> AssetFuture<T> LoadAsync(AssetId id) { return AssetFuture<T>(Request(id, typeid(T))); }
    template<class T> std::shared_ptr<T> Load(AssetId id) { return LoadAsync<T>(id).get(); }
    AssetState State(AssetId id) const;
    bool Unload(AssetId id);
    std::size_t CollectUnused();
    void ClearCache();

private:
    template<class T, class Format>
    static std::shared_ptr<T> ReadNew(const std::filesystem::path& path, const serialization::ArchiveSetup& setup, serialization::ArchiveLimits limits)
    {
        std::istringstream input(serialization::detail::ReadFile(path, std::is_same_v<Format, serialization::TextArchive>, limits), std::ios::binary);
        auto resource = std::make_shared<T>();
        Format archive(input, limits);
        if (setup)
        {
            setup(archive);
        }

        archive.deserialize(*resource);
        archive.finish();
        if (input.bad() || (!input.eof() && input.peek() != std::char_traits<char>::eof()))
        {
            throw serialization::ArchiveError("Invalid trailing asset data");
        }

        return resource;
    }

    using Resource = std::shared_ptr<void>;
    struct Loader
    {
        std::type_index type;
        std::function<Resource(const AssetMetadata&, const std::filesystem::path&)> load;
    };

    struct Entry
    {
        AssetState state = AssetState::Unloaded;
        Resource resource;
        std::shared_future<Resource> pending;
        std::uint64_t lastUse = 0;
    };

    std::shared_future<Resource> Request(AssetId id, std::type_index type);
    std::filesystem::path Resolve(const AssetMetadata& metadata) const;
    void Worker();
    void Trim();

    std::filesystem::path root_;
    AssetManagerOptions options_;
    std::unordered_map<AssetId, AssetMetadata> metadata_;
    std::unordered_map<std::string, Loader> loaders_;
    std::unordered_map<AssetId, Entry> entries_;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::function<void()>> jobs_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;

    std::size_t pending_ = 0;
    std::uint64_t clock_ = 0;
};
}