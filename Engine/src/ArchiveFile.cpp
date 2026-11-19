#include "Serialization/ArchiveFile.h"
#include "Serialization/CRC32.h"
#include <atomic>
#include <chrono>
#include <charconv>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace oot::serialization::detail
{
namespace
{
    constexpr std::string_view JsonPrefix = "{\n  \"format\": \"OOT-CRC32\",\n  \"version\": 1,\n  \"payload\": ";
    constexpr std::string_view JsonChecksum = ",\n  \"crc32\": \"";
    constexpr std::string_view JsonEnd = "\"\n}\n";
    constexpr std::string_view BinaryMagic = "OOTCRC1B";

    std::string Envelope(const std::string& payload, bool json)
    {
        std::string result(json ? JsonPrefix : BinaryMagic);
        if (!json)
        {
            const auto size = static_cast<std::uint64_t>(payload.size());
            for (unsigned i = 0; i < 8; ++i)
            {
                result += static_cast<char>(size >> (8 * i));
            }
        }

        result += payload;
        const auto crc = CRC32::Compute(result);
        if (json)
        {
            result += JsonChecksum;
            constexpr char hex[] = "0123456789abcdef";
            for (int i = 7; i >= 0; --i)
            {
                result += hex[(crc >> (4 * i)) & 15];
            }

            result += JsonEnd;
        }

        else
        {
            for (unsigned i = 0; i < 4; ++i)
            {
                result += static_cast<char>(crc >> (8 * i));
            }
        }

        return result;
    }

    std::string Verify(const std::string& bytes, bool json, ArchiveLimits limits)
    {
        const std::string_view data(bytes);
        std::size_t start, end;
        std::uint32_t expected = 0;
        if (json)
        {
            const auto trailerSize = JsonChecksum.size() + 8 + JsonEnd.size();
            if (data.size() < JsonPrefix.size() + trailerSize || !data.starts_with(JsonPrefix) || !data.ends_with(JsonEnd))
            {
                throw ArchiveError("Invalid CRC JSON envelope");
            }

            start = JsonPrefix.size();
            end = data.size() - trailerSize;
            if (data.substr(end, JsonChecksum.size()) != JsonChecksum)
            {
                throw ArchiveError("Missing JSON checksum");
            }

            const auto hex = data.substr(end + JsonChecksum.size(), 8);
            const auto parsed = std::from_chars(hex.data(), hex.data() + hex.size(), expected, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != hex.data() + hex.size())
            {
                throw ArchiveError("Invalid JSON checksum");
            }
        }

        else
        {
            if (data.size() < 20 || !data.starts_with(BinaryMagic))
            {
                throw ArchiveError("Invalid CRC binary envelope");
            }

            start = 16;
            end = data.size() - 4;
            std::uint64_t length = 0;
            for (unsigned i = 0; i < 8; ++i)
            {
                length |= std::uint64_t(static_cast<unsigned char>(data[8 + i])) << (8 * i);
            }

            if (length != end - start)
            {
                throw ArchiveError("Invalid binary payload length");
            }

            for (unsigned i = 0; i < 4; ++i)
            {
                expected |= std::uint32_t(static_cast<unsigned char>(data[end + i])) << (8 * i);
            }
        }

        if (end - start > limits.maxArchiveBytes)
        {
            throw ArchiveError("Archive byte limit exceeded");
        }

        if (CRC32::Compute(data.substr(0, end)) != expected)
        {
            throw ArchiveError("Archive checksum mismatch: corrupted file");
        }

        return std::string(data.substr(start, end - start));
    }

    struct TemporaryFile
    {
        std::filesystem::path directory;
        std::filesystem::path file;
        ~TemporaryFile()
        {
            std::error_code ignored;
            std::filesystem::remove(file, ignored);
            std::filesystem::remove(directory, ignored);
        }
    };

    void ReplaceFile(const std::filesystem::path& temporary, const std::filesystem::path& target)
    {
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            const auto error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
            throw ArchiveError("Cannot replace archive file: " + error.message());
        }
#else
        std::filesystem::rename(temporary, target);
#endif
    }
}

std::string ReadFile(const std::filesystem::path& path, bool json, ArchiveLimits limits)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw ArchiveError("Cannot open archive file for reading");
    }

    const std::uint64_t overhead = json ? JsonPrefix.size() + JsonChecksum.size() + 8 + JsonEnd.size() : 20;
    constexpr auto max = std::numeric_limits<std::uint64_t>::max();
    const auto bound = limits.maxArchiveBytes > max - overhead ? max : limits.maxArchiveBytes + overhead;

    std::string bytes;
    std::array<char, 4096> buffer{};
    while (input)
    {
        input.read(buffer.data(), buffer.size());
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > bound - bytes.size())
        {
            throw ArchiveError("Archive byte limit exceeded");
        }
        bytes.append(buffer.data(), count);
    }

    if (input.bad() || !input.eof())
    {
        throw ArchiveError("Archive file read failure");
    }

    input.clear();
    input.close();

    if (input.fail())
    {
        throw ArchiveError("Cannot close archive file");
    }
    return Verify(bytes, json, limits);
}

void SaveFile(const std::filesystem::path& path, const std::string& payload, bool json, ArchiveLimits limits)
{
    if (payload.size() > limits.maxArchiveBytes)
    {
        throw ArchiveError("Archive byte limit exceeded");
    }

    const auto encoded = Envelope(payload, json);
    try
    {
        if (path.empty() || path.filename().empty() || path.filename() == "." || path.filename() == "..")
        {
            throw ArchiveError("Invalid archive file path");
        }

        const auto target = std::filesystem::absolute(path);
        std::filesystem::create_directories(target.parent_path());
        static std::atomic<std::uint64_t> sequence{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        std::filesystem::path directory;
        bool created = false;

        for (unsigned attempt = 0; attempt < 128; ++attempt)
        {
            directory = target.parent_path() / (".oot-archive-" + std::to_string(stamp) + "-" + std::to_string(sequence.fetch_add(1)));
            if (std::filesystem::create_directory(directory))
            {
                created = true;
                break;
            }
        }

        if (!created)
        {
            throw ArchiveError("Cannot reserve a temporary archive directory");
        }

        TemporaryFile temporary{directory, directory / "payload"};
        std::ofstream output(temporary.file, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            throw ArchiveError("Cannot open temporary archive file");
        }

        output.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
        output.flush();
        if (!output)
        {
            throw ArchiveError("Cannot flush archive file");
        }
        output.close();
        if (output.fail())
        {
            throw ArchiveError("Cannot close archive file");
        }

        if (ReadFile(temporary.file, json, limits) != payload)
        {
            throw ArchiveError("Archive verification after close failed");
        }

        ReplaceFile(temporary.file, target);
    }

    catch (const std::filesystem::filesystem_error& error)
    {
        throw ArchiveError(error.what());
    }

    catch (const std::ios_base::failure& error)
    {
        throw ArchiveError(error.what());
    }
}
}