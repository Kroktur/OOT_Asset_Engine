#include "Serialization/ArchiveFile.h"
#include <atomic>
#include <chrono>
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

void SaveFile(const std::filesystem::path& path,
              const std::function<void(std::ostream&)>& write)
{
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

        write(output);
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
