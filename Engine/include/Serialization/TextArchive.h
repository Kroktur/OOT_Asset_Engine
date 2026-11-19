#pragma once
#include "Archive.h"
#include <json.hpp>
#include <cmath>
#include <istream>
#include <ostream>
#include <set>

namespace oot::serialization
{
class TextArchive final : public Archive
{
public:
    explicit TextArchive(std::istream& input, ArchiveLimits limits = {})
        : Archive(Mode::Read, limits)
    {
        readDocument(input);
    }

    explicit TextArchive(std::ostream& output, ArchiveLimits limits = {})
        : Archive(Mode::Write, limits), output_(&output)
    {
        write("{\n  \"format\": \"OOT-JSON\",\n  \"version\": 1,\n  \"values\": [");
    }

    void finish() override
    {
        if (finished_)
        {
            return;
        }

        if (isReading())
        {
            if (cursor_ != values_.size())
            {
                throw ArchiveError("Unused JSON archive fields");
            }
        }

        else
        {
            write("\n  ]\n}\n");
        }
        finished_ = true;
    }

private:
    using Json = nlohmann::json;

    void checkOpen() const
    {
        if (finished_)
        {
            throw ArchiveError("JSON archive is already finished");
        }
    }

    void write(const std::string& text)
    {
        consume(text.size());
        try 
        {
            output_->write(text.data(), static_cast<std::streamsize>(text.size()));
        }

        catch (const std::ios_base::failure&)
        {
            throw ArchiveError("JSON archive I/O failure");
        }

        if (!*output_)
        {
            throw ArchiveError("JSON archive I/O failure");
        }
    }

    void append(const Json& value)
    {
        checkOpen();
        std::string encoded;
        try
        {
            encoded = value.dump();
        }

        catch (const Json::exception& error)
        {
            throw ArchiveError(error.what());
        }

        write((first_ ? "\n    " : ",\n    ") + encoded);
        first_ = false;
    }

    const Json& next()
    {
        checkOpen();
        if (cursor_ >= values_.size())
        {
            throw ArchiveError("Missing JSON archive field");
        }

        return values_[cursor_++];
    }

    void readDocument(std::istream& input)
    {
        std::string source;
        for (;;)
        {
            int c;
            try
            {
                c = input.get();
            }

            catch (const std::ios_base::failure&)
            {
                if (input.eof() && !input.bad())
                {
                    break;
                }

                throw ArchiveError("JSON archive I/O failure");
            }

            if (c == std::char_traits<char>::eof())
            {
                if (input.bad() || !input.eof())
                {
                    throw ArchiveError("JSON archive I/O failure");
                }

                break;
            }
            consume(1);
            source += static_cast<char>(c);
        }

        std::set<std::string> keys;
        try
        {
            auto document = Json::parse(source, [&keys](int depth, Json::parse_event_t event, Json& parsed)
            {
                if (depth > 2 || (depth >= 2 && (event == Json::parse_event_t::array_start || event == Json::parse_event_t::object_start)))
                {
                    throw ArchiveError("Unexpected nested JSON value");
                }

                if (event == Json::parse_event_t::key && !keys.insert(parsed.get<std::string>()).second)
                {
                    throw ArchiveError("Duplicate JSON field");
                }

                return true;
            });

            if (!document.is_object() || document.size() != 3
                || !document.contains("format") || document["format"] != "OOT-JSON"
                || !document.contains("version") || !document["version"].is_number_integer()
                || document["version"] != 1 || !document.contains("values") || !document["values"].is_array())
            {
                throw ArchiveError("Invalid JSON archive format or version");
            }

            for (const auto& value : document["values"])
            {
                if (value.is_null() || value.is_structured())
                {
                    throw ArchiveError("Invalid JSON archive value");
                }

                if (value.is_string())
                {
                    checkString(value.get_ref<const std::string&>().size());
                }
            }
            values_ = std::move(document["values"]);
        }
        catch (const Json::exception& error)
        {
            throw ArchiveError(error.what());
        }
    }

    void integer(std::uint64_t& value) override
    {
        if (isWriting())
        {
            append(value);
            return;
        }

        const auto& token = next();
        if (token.is_number_unsigned())
        {
            value = token.get<std::uint64_t>();
        }

        else if (token.is_number_integer() && token.get<std::int64_t>() >= 0)
        {
            value = static_cast<std::uint64_t>(token.get<std::int64_t>());
        }

        else
        {
            throw ArchiveError("Expected an unsigned JSON integer");
        }
    }

    void signedInteger(std::int64_t& value) override
    {
        if (isWriting())
        {
            append(value);
            return;
        }

        const auto& token = next();
        if (token.is_number_unsigned())
        {
            const auto number = token.get<std::uint64_t>();
            if (number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            {
                throw ArchiveError("Signed JSON integer out of range");
            }
            value = static_cast<std::int64_t>(number);
        }

        else if (token.is_number_integer())
        {
            value = token.get<std::int64_t>();
        }

        else
        {
            throw ArchiveError("Expected a signed JSON integer");
        }
    }

    void real(double& value) override
    {
        if (isWriting())
        {
            if (!std::isfinite(value))
            {
                throw ArchiveError("Non-finite numbers are unsupported");
            }

            append(value);
            return;
        }

        const auto& token = next();
        if (!token.is_number())
        {
            throw ArchiveError("Expected a JSON number");
        }

        const double number = token.get<double>();
        if (!std::isfinite(number))
        {
            throw ArchiveError("Non-finite numbers are unsupported");
        }
        value = number;
    }

    void boolean(bool& value) override
    {
        if (isWriting())
        {
            append(value);
            return;
        }

        const auto& token = next();
        if (!token.is_boolean())
        {
            throw ArchiveError("Expected a JSON boolean");
        }

        value = token.get<bool>();
    }

    void string(std::string& value) override
    {
        if (isWriting())
        {
            checkString(value.size()); 
            append(value); 
            return;
        }

        const auto& token = next();
        if (!token.is_string())
        {
            throw ArchiveError("Expected a JSON string");
        }

        value = token.get<std::string>();
    }

    std::ostream* output_ = nullptr;
    Json values_ = Json::array();
    std::size_t cursor_ = 0;
    bool first_ = true;
    bool finished_ = false;
};
}