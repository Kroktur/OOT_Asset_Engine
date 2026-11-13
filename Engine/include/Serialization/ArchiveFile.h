#pragma once
#include "BinaryArchive.h"
#include "TextArchive.h"
#include <filesystem>
#include <fstream>

namespace oot::serialization
{
using ArchiveSetup = std::function<void(Archive&)>;

namespace detail
{
    void SaveFile(const std::filesystem::path& path, const std::function<void(std::ostream&)>& write);

    template<class Format, class T>
    void Save(const std::filesystem::path& path, T& value, const ArchiveSetup& setup, ArchiveLimits limits)
    {
        SaveFile(path, [&](std::ostream& output)
        {
            Format archive(output, limits);
            if (setup)
            {
                setup(archive);
            }
            archive.serialize(value);
            archive.finish();
        });
    }

    template<class Format, class T>
    void Load(const std::filesystem::path& path, T& value, const ArchiveSetup& setup, ArchiveLimits limits)
    {
        static_assert(std::is_default_constructible_v<T> && std::is_nothrow_swappable_v<T>, "File loading requires a default-constructible, nothrow-swappable object");
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);

        if (error)
        {
            throw ArchiveError("Cannot inspect archive file: " + error.message());
        }

        if (size > limits.maxArchiveBytes)
        {
            throw ArchiveError("Archive byte limit exceeded");
        }

        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            throw ArchiveError("Cannot open archive file for reading");
        }

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

        input.clear();
        input.close();
        if (input.fail())
        {
            throw ArchiveError("Cannot close archive file");
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
