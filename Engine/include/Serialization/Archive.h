#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace oot::serialization 
{

class ArchiveError : public std::runtime_error 
{
public:
    using std::runtime_error::runtime_error;
};

struct ArchiveLimits 
{
    std::uint64_t maxStringBytes = 16 * 1024 * 1024;
    std::uint64_t maxContainerElements = 1'000'000;
    std::uint64_t maxArchiveBytes = 64 * 1024 * 1024;
    std::uint32_t maxDepth = 64;
};

class Archive 
{
public:
    enum class Mode 
    {
        Read,
        Write
    };

    virtual ~Archive() = default;

    Archive(const Archive&) = delete;

    Archive& operator=(const Archive&) = delete;

    bool isReading() const noexcept 
    {
        return mode_ == Mode::Read;
    }

    bool isWriting() const noexcept 
    {
        return !isReading();
    }

    template<class... T> void operator()(T&... values) 
    {
        (process(values), ...);
    }

    template<class T> Archive& operator&(T& value) 
    {
        process(value); return *this;
    }

protected:
    explicit Archive(Mode mode, ArchiveLimits limits) : mode_(mode), limits_(limits) {}

    virtual void integer(std::uint64_t&) = 0;
    virtual void signedInteger(std::int64_t&) = 0;
    virtual void real(double&) = 0;
    virtual void boolean(bool&) = 0;
    virtual void string(std::string&) = 0;

    const ArchiveLimits& limits() const noexcept 
    {
        return limits_;
    }

    void consume(std::uint64_t bytes) 
    {
        if (bytes > limits_.maxArchiveBytes - bytes_)
        {
            throw ArchiveError("Archive byte limit exceeded");
        }

        bytes_ += bytes;
    }

    void checkString(std::uint64_t size) const 
    {
        if (size > limits_.maxStringBytes || size > std::string{}.max_size())
        {
            throw ArchiveError("String size limit exceeded");
        }
    }

private:
    struct DepthGuard 
    {
        Archive& archive;

        explicit DepthGuard(Archive& a) : archive(a) 
        {
            if (a.depth_ >= a.limits_.maxDepth)
            {
                throw ArchiveError("Archive nesting limit exceeded");
            }

            ++a.depth_;
        }

        ~DepthGuard() { --archive.depth_; }
    };

    template<class T> void process(T& value) 
    {
        static_assert(!std::is_const_v<T>, "serialize requires mutable fields");

        if constexpr (std::is_same_v<T, bool>)
        {
            boolean(value);
        }

        else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) 
        {
            static_assert(sizeof(T) <= sizeof(std::int64_t));
            std::int64_t temporary = isReading() ? 0 : value;
            signedInteger(temporary);

            if (temporary < std::numeric_limits<T>::min() || temporary > std::numeric_limits<T>::max())
            {
                throw ArchiveError("Signed integer out of range");
            }

            if (isReading())
            {
                value = static_cast<T>(temporary);
            }
        }

        else if constexpr (std::is_integral_v<T>) 
        {
            static_assert(sizeof(T) <= sizeof(std::uint64_t));
            std::uint64_t temporary = isReading() ? 0 : value;
            integer(temporary);

            if (temporary > std::numeric_limits<T>::max())
            {
                throw ArchiveError("Unsigned integer out of range");
            }

            if (isReading())
            {
                value = static_cast<T>(temporary);
            }
        }

        else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) 
        {
            double temporary = isReading() ? 0 : value;
            real(temporary);

            if constexpr (std::is_same_v<T, float>) 
            {
                if (temporary > std::numeric_limits<float>::max() || temporary < -std::numeric_limits<float>::max())
                {
                    throw ArchiveError("Float out of range");
                }
            }

            if (isReading())
            {
                value = static_cast<T>(temporary);
            }
        }
        
        else if constexpr (std::is_enum_v<T>)
        {
            auto temporary = isReading() ? std::underlying_type_t<T>{} : static_cast<std::underlying_type_t<T>>(value);
            process(temporary);

            if (isReading())
            {
                value = static_cast<T>(temporary);
            }
        }
        
        else if constexpr (std::is_same_v<T, std::string>)
        {
            string(value);
        }

        else 
        {
            DepthGuard guard(*this);
            value.serialize(*this);
        }
    }

    template<class T, class Allocator> void process(std::vector<T, Allocator>& value) 
    {
        DepthGuard guard(*this);
        std::uint64_t count = isReading() ? 0 : value.size();
        integer(count);

        if (count > limits_.maxContainerElements || count > value.max_size())
        {
            throw ArchiveError("Container size limit exceeded");
        }

        if (isReading())
        {
            std::vector<T, Allocator> temporary(value.get_allocator());
            for (std::uint64_t i = 0; i < count; ++i) 
            {
                T element{};
                process(element);
                temporary.push_back(std::move(element));
            }

            value = std::move(temporary);
        }
        
        else
        {
            for (std::size_t i = 0; i < value.size(); ++i) 
            {
                if constexpr (std::is_same_v<T, bool>)
                {
                    bool element = value[i]; process(element);
                }

                else
                {
                    process(value[i]);
                }
            }
        }
    }

    template<class T, std::size_t N> void process(std::array<T, N>& value) 
    {
        DepthGuard guard(*this);
        if (N > limits_.maxContainerElements)
        {
            throw ArchiveError("Container size limit exceeded");
        }

        for (auto& element : value)
        {
            process(element);
        }
    }

    template<class T> void process(std::optional<T>& value)
    {
        DepthGuard guard(*this);
        bool present = value.has_value();
        boolean(present);

        if (isReading())
        {
            if (present)
            {
                T temporary{}; process(temporary); value = std::move(temporary);
            }

            else
            {
                value.reset();
            }
        }
        
        else if (present)
        {
            process(*value);
        }
    }

    Mode mode_;
    ArchiveLimits limits_;
    std::uint64_t bytes_ = 0;
    std::uint32_t depth_ = 0;
};
}
