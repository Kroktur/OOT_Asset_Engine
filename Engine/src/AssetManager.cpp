#include "Assets/AssetManager.h"

namespace oot::assets
{
AssetManager::AssetManager(const AssetRegistry& registry, std::filesystem::path root, AssetManagerOptions options)
    : root_(std::filesystem::weakly_canonical(std::filesystem::absolute(root))), options_(options)
{
    if (!options.workerCount || options.workerCount > 64 || !options.maxPendingLoads)
    {
        throw serialization::ArchiveError("Invalid asset manager limits");
    }

    for (auto& asset : registry.Snapshot())
    {
        metadata_.emplace(asset.id, std::move(asset));
    }

    try
    {
        for (std::size_t i = 0; i < options.workerCount; ++i)
        {
            workers_.emplace_back([this] { Worker(); });
        }
    }

    catch (...)
    {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }

        wake_.notify_all();
        for (auto& worker : workers_)
        {
            worker.join();
        }

        throw;
    }
}

AssetManager::~AssetManager()
{
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }

    wake_.notify_all();
    for (auto& worker : workers_)
    {
        worker.join();
    }
}

std::filesystem::path AssetManager::Resolve(const AssetMetadata& metadata) const
{
    const std::u8string utf8(metadata.path.begin(), metadata.path.end());
    const auto path = std::filesystem::weakly_canonical(root_ / std::filesystem::path(utf8));
    auto base = root_.begin(), child = path.begin();
    for (; base != root_.end(); ++base, ++child)
    {
        if (child == path.end() || *base != *child)
        {
            throw serialization::ArchiveError("Asset path escapes its root");
        }
    }

    return path;
}

std::shared_future<AssetManager::Resource> AssetManager::Request(AssetId id, std::type_index type)
{
    std::lock_guard lock(mutex_);
    if (stopping_)
    {
        throw serialization::ArchiveError("Asset manager is stopping");
    }

    const auto record = metadata_.find(id);
    if (record == metadata_.end())
    {
        throw serialization::ArchiveError("Unknown asset ID");
    }

    const auto loader = loaders_.find(record->second.type);
    if (loader == loaders_.end())
    {
        throw serialization::ArchiveError("No loader for asset type");
    }

    if (loader->second.type != type)
    {
        throw serialization::ArchiveError("Requested asset type mismatch");
    }
    auto& entry = entries_[id];
    entry.lastUse = ++clock_;
    if (entry.state == AssetState::Loading)
    {
        return entry.pending;
    }

    if (entry.resource)
    {
        std::promise<Resource> promise;
        promise.set_value(entry.resource);
        return promise.get_future().share();
    }

    if (pending_ >= options_.maxPendingLoads)
    {
        throw serialization::ArchiveError("Asset loading queue is full");
    }

    auto promise = std::make_shared<std::promise<Resource>>();
    auto future = promise->get_future().share();

    jobs_.push_back([this, id, metadata = record->second, loader = loader->second, promise]() mutable
    {
        Resource resource;
        std::exception_ptr error;
        try
        {
            resource = loader.load(metadata, Resolve(metadata));
            if (!resource)
            {
                throw serialization::ArchiveError("Asset loader returned null");
            }
        }

        catch (...)
        {
            error = std::current_exception();
        }

        std::lock_guard lock(mutex_);
        auto& result = entries_.at(id);
        --pending_;
        result.pending = {};

        if (error)
        {
            result.state = AssetState::Failed;
            promise->set_exception(error);
            promise.reset();
        }

        else
        {
            result.resource = resource;
            result.state = AssetState::Ready;
            result.lastUse = ++clock_;
            promise->set_value(std::move(resource));
            promise.reset();
            Trim();
        }
    });

    entry.pending = future;
    entry.state = AssetState::Loading;
    ++pending_;
    wake_.notify_one();
    return future;
}

void AssetManager::Worker()
{
    for (;;)
    {
        std::function<void()> job;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this]
                {
                    return stopping_ || !jobs_.empty();
                });

            if (jobs_.empty())
            {
                return;
            }

            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        job();
    }
}

void AssetManager::Trim()
{
    std::size_t cached = 0;
    for (const auto& [id, entry] : entries_)
    {
        if (entry.resource)
        {
            ++cached;
        }
    }

    while (cached > options_.maxCachedAssets)
    {
        auto oldest = entries_.end();
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
        {
            if (it->second.resource && (oldest == entries_.end() || it->second.lastUse < oldest->second.lastUse))
            {
                oldest = it;
            }
        }

        entries_.erase(oldest);
        --cached;
    }
}

AssetState AssetManager::State(AssetId id) const
{
    std::lock_guard lock(mutex_);
    const auto it = entries_.find(id);
    return it == entries_.end() ? AssetState::Unloaded : it->second.state;
}

bool AssetManager::Unload(AssetId id)
{
    std::lock_guard lock(mutex_);
    const auto it = entries_.find(id);
    if (it == entries_.end() || it->second.state == AssetState::Loading)
    {
        return false;
    }

    entries_.erase(it);
    return true;
}

std::size_t AssetManager::CollectUnused()
{
    std::lock_guard lock(mutex_);
    std::size_t count = 0;
    for (auto it = entries_.begin(); it != entries_.end();)
    {
        if (it->second.resource && it->second.resource.use_count() == 1)
        {
            it = entries_.erase(it); ++count;
        }

        else ++it;
    }

    return count;
}

void AssetManager::ClearCache()
{
    std::lock_guard lock(mutex_);
    for (auto it = entries_.begin(); it != entries_.end();)
    {
        if (it->second.state != AssetState::Loading)
        {
            it = entries_.erase(it);
        }

        else
        {
            ++it;
        }
    }
}
}