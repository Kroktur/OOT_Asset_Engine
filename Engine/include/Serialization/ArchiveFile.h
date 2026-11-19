#pragma once
#include "BinaryArchive.h"
#include "TextArchive.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace oot::serialization
{
using ArchiveSetup = std::function<void(Archive&)>;

namespace detail
{
    void SaveFile(const std::filesystem::path& path, const std::string& payload, bool json, ArchiveLimits limits);
    std::string ReadFile(const std::filesystem::path& path, bool json, ArchiveLimits limits);

    template<class Format, class T>
    void Save(const std::filesystem::path& path, T& value, const ArchiveSetup& setup, ArchiveLimits limits)
    {
        std::ostringstream output(std::ios::binary);
        Format archive(output, limits);
        if (setup)
        {
            setup(archive);
        }
        archive.serialize(value);
        archive.finish();
        SaveFile(path, output.str(), std::is_same_v<Format, TextArchive>, limits);
    }

    template<class Format, class T>
    void Load(const std::filesystem::path& path, T& value, const ArchiveSetup& setup, ArchiveLimits limits)
    {
        static_assert(std::is_default_constructible_v<T> && std::is_nothrow_swappable_v<T>, "File loading requires a default-constructible, nothrow-swappable object");
        std::istringstream input(ReadFile(path, std::is_same_v<Format, TextArchive>, limits), std::ios::binary);

        T temporary{};
        Format archive(input, limits);
        if (setup)
        {
            setup(archive);
        }

        archive.deserialize(temporary);
        archive.finish();
        if (input.bad())
        {
            throw ArchiveError("Archive file read failure");
        }

        if (!input.eof() && input.peek() != std::char_traits<char>::eof())
        {
            throw ArchiveError("Trailing data in archive file");
        }

        if (input.bad())
        {
            throw ArchiveError("Archive file read failure");
        }

        using std::swap;
        swap(value, temporary);
    }
}

template<class T>
void SaveJson(const std::filesystem::path& path, T& value, const ArchiveSetup& setup = {}, ArchiveLimits limits = {})
{
    detail::Save<TextArchive>(path, value, setup, limits);
}

template<class T>
void LoadJson(const std::filesystem::path& path, T& value, const ArchiveSetup& setup = {}, ArchiveLimits limits = {})
{
    detail::Load<TextArchive>(path, value, setup, limits);
}

template<class T>
void SaveBinary(const std::filesystem::path& path, T& value, const ArchiveSetup& setup = {}, ArchiveLimits limits = {})
{
    detail::Save<BinaryArchive>(path, value, setup, limits);
}

template<class T>
void LoadBinary(const std::filesystem::path& path, T& value, const ArchiveSetup& setup = {}, ArchiveLimits limits = {})
{
    detail::Load<BinaryArchive>(path, value, setup, limits);
}
}