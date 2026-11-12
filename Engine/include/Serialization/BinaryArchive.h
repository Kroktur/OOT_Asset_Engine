#pragma once
#include "Archive.h"
#include <cmath>
#include <istream>
#include <ostream>

namespace oot::serialization 
{

class BinaryArchive final : public Archive 
{
public:
    explicit BinaryArchive(std::istream& input, ArchiveLimits limits = {})
        : Archive(Mode::Read, limits), input_(&input) { header(); }

    explicit BinaryArchive(std::ostream& output, ArchiveLimits limits = {})
        : Archive(Mode::Write, limits), output_(&output) { header(); }

private:
    void bytes(char* data, std::size_t count) 
    {
        consume(count);
        try 
        {
            if (isReading())
            {
                input_->read(data, static_cast<std::streamsize>(count));
            }

            else
            {
                output_->write(data, static_cast<std::streamsize>(count));
            }
        }
        
        catch (const std::ios_base::failure&) 
        {
            throw ArchiveError("Binary archive I/O failure");
        }

        if ((input_ && !*input_) || (output_ && !*output_))
        {
            throw ArchiveError("Binary archive I/O failure or truncated input");
        }
    }

    void header() 
    {
        std::array<char, 8> signature{'O','O','T','B',1,0,0,0};
        const auto expected = signature;
        bytes(signature.data(), signature.size());

        if (signature != expected)
        {
            throw ArchiveError("Invalid binary archive signature or version");
        }
    }

    void integer(std::uint64_t& value) override 
    {
        std::array<char, 8> encoded{};
        if (isWriting())
        {
            for (unsigned i = 0; i < 8; ++i)
            {
                encoded[i] = static_cast<char>((value >> (8 * i)) & 255);
            }

        }

        bytes(encoded.data(), encoded.size());

        if (isReading())
        {
            value = 0;
            for (unsigned i = 0; i < 8; ++i)
            {
                value |= std::uint64_t(static_cast<unsigned char>(encoded[i])) << (8 * i);
            }
        }
    }

    void signedInteger(std::int64_t& value) override 
    {
        auto encoded = std::bit_cast<std::uint64_t>(value);
        integer(encoded);

        if (isReading())
        {
            value = std::bit_cast<std::int64_t>(encoded);
        }
    }

    void real(double& value) override 
    {
        static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
        if (isWriting() && !std::isfinite(value))
        {
            throw ArchiveError("Non-finite numbers are unsupported");
        }

        auto encoded = std::bit_cast<std::uint64_t>(value);
        integer(encoded);

        if (isReading())
        {
            const double decoded = std::bit_cast<double>(encoded);
            if (!std::isfinite(decoded))
            {
                throw ArchiveError("Non-finite numbers are unsupported");
            }

            value = decoded;
        }
    }

    void boolean(bool& value) override 
    {
        char encoded = value ? 1 : 0;
        bytes(&encoded, 1);

        if (encoded != 0 && encoded != 1)
        {
            throw ArchiveError("Invalid binary boolean");
        }

        if (isReading())
        {
            value = encoded != 0;
        }
    }

    void string(std::string& value) override 
    {
        std::uint64_t count = isReading() ? 0 : value.size();
        integer(count);
        checkString(count);

        if (isReading()) 
        {
            std::string temporary;
            std::array<char, 4096> buffer{};

            while (count) 
            {
                const auto chunk = static_cast<std::size_t>(count < buffer.size() ? count : buffer.size());
                bytes(buffer.data(), chunk);
                temporary.append(buffer.data(), chunk);
                count -= chunk;
            }

            value = std::move(temporary);
        }

        else
        {
            bytes(value.data(), value.size());
        }
    }

    std::istream* input_ = nullptr;
    std::ostream* output_ = nullptr;
};
}
