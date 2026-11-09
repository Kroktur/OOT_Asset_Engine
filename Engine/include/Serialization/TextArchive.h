#pragma once
#include "Archive.h"
#include <charconv>
#include <cmath>
#include <istream>
#include <ostream>

namespace oot::serialization 
{

class TextArchive final : public Archive 
{
public:
    explicit TextArchive(std::istream& input, ArchiveLimits limits = {})
        : Archive(Mode::Read, limits), input_(&input) { header(); }

    explicit TextArchive(std::ostream& output, ArchiveLimits limits = {})
        : Archive(Mode::Write, limits), output_(&output) { header(); }

private:
    char get() 
    {
        consume(1);
        char c{};

        try
        {
            input_->get(c);
        }

        catch (const std::ios_base::failure&)
        {
            throw ArchiveError("Text archive I/O failure");
        }

        if (!*input_)
        {
            throw ArchiveError("Truncated text archive");
        }

        return c;
    }

    void put(char c) 
    {
        consume(1);
        try 
        { 
            output_->put(c);
        }

        catch (const std::ios_base::failure&) 
        {
            throw ArchiveError("Text archive I/O failure");
        }

        if (!*output_)
        {
            throw ArchiveError("Text archive I/O failure");
        }
    }

    void write(const std::string& value)
    {
        for (char c : value)
        {
            put(c);
        }
    }

    void expect(char c)
    {
        if (get() != c)
        {
            throw ArchiveError("Malformed text archive");
        }
    }

    std::string line() 
    {
        std::string result;

        for (;;) 
        {
            char c = get();

            if (c == '\n')
            {
                return result;
            }

            if (result.size() >= 128)
            {
                throw ArchiveError("Numeric token too long");
            }

            result += c;
        }
    }

    void header()
    {
        if (isReading())
        {
            if (line() != "OOT-TEXT 1")
            {
                throw ArchiveError("Invalid text archive signature or version");
            }
        }

        else
        {
            write("OOT-TEXT 1\n");
        }
    }

    template<class T> void number(char tag, T& value) 
    {
        if (isReading()) 
        {
            expect(tag); expect(' ');
            const auto token = line();
            T temporary{};
            const auto result = std::from_chars(token.data(), token.data() + token.size(), temporary);

            if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
            {
                throw ArchiveError("Invalid number in text archive");
            }

            if constexpr (std::is_floating_point_v<T>)
            {
                if (!std::isfinite(temporary))
                {
                    throw ArchiveError("Non-finite numbers are unsupported");
                }
            }

            value = temporary;
        }
        
        else
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                if (!std::isfinite(value))
                {
                    throw ArchiveError("Non-finite numbers are unsupported");
                }
            }

            std::array<char, 128> buffer{};
            const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);

            if (result.ec != std::errc{})
            {
                throw ArchiveError("Cannot format number");
            }

            put(tag); put(' ');

            for (auto p = buffer.data(); p != result.ptr; ++p)
            {
                put(*p);
            }

            put('\n');
        }
    }

    void integer(std::uint64_t& value) override
    {
        number('u', value);
    }

    void signedInteger(std::int64_t& value) override
    {
        number('i', value);
    }

    void real(double& value) override 
    {
        number('f', value);
    }

    void boolean(bool& value) override
    {
        unsigned temporary = value ? 1 : 0;
        number('b', temporary);

        if (temporary > 1)
        {
            throw ArchiveError("Invalid text boolean");
        }

        if (isReading())
        {
            value = temporary != 0;
        }
    }

    static unsigned hex(char c) 
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }

        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }

        throw ArchiveError("Invalid string escape");
    }

    void string(std::string& value) override 
    {
        if (isReading()) 
        {
            expect('s');
            expect(' ');
            expect('"');

            std::string temporary;

            for (;;) 
            {
                char c = get();

                if (c == '"')
                {
                    break;
                }

                if (c == '\\') 
                {
                    c = get();

                    if (c == 'n')
                    {
                        c = '\n';
                    }

                    else if (c == 'r')
                    {
                        c = '\r';
                    }

                    else if (c == 't')
                    {
                        c = '\t';
                    }

                    else if (c == 'x')
                    { 
                        const unsigned high = hex(get()); c = static_cast<char>((high << 4) | hex(get()));
                    }

                    else if (c != '\\' && c != '"')
                    {
                        throw ArchiveError("Unknown string escape");
                    }
                }
                
                else if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) >= 127)
                {
                    throw ArchiveError("Unescaped byte in text string");
                }

                checkString(temporary.size() + 1);
                temporary += c;
            }

            expect('\n');
            value = std::move(temporary);
        }
        
        else 
        {
            checkString(value.size());
            write("s \"");
            constexpr char digits[] = "0123456789abcdef";

            for (unsigned char c : value)
            {
                if (c == '"' || c == '\\')
                {
                    put('\\'); put(static_cast<char>(c));
                }

                else if (c == '\n')
                {
                    write("\\n");
                }

                else if (c == '\r')
                {
                    write("\\r");
                }

                else if (c == '\t') {
                    write("\\t");
                }

                else if (c < 32 || c >= 127)
                {
                    write("\\x"); put(digits[c >> 4]); put(digits[c & 15]);
                }

                else
                {
                    put(static_cast<char>(c));
                }
            }

            write("\"\n");
        }
    }

    std::istream* input_ = nullptr;
    std::ostream* output_ = nullptr;
};
}
