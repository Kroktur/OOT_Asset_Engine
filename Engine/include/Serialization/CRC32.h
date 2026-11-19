#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace oot::serialization
{
class CRC32
{
public:
    void update(std::string_view bytes) noexcept
    {
        for (unsigned char byte : bytes)
        {
            state_ = (state_ >> 8) ^ table_[(state_ ^ byte) & 255];
        }
    }

    std::uint32_t value() const noexcept
    {
        return state_ ^ 0xFFFFFFFFu;
    }

    void reset() noexcept
    {
        state_ = 0xFFFFFFFFu;
    }

    static std::uint32_t Compute(std::string_view bytes) noexcept
    {
        CRC32 crc;
        crc.update(bytes);
        return crc.value();
    }

private:
    inline static constexpr auto table_ = []
    {
        std::array<std::uint32_t, 256> result{};
        for (std::uint32_t i = 0; i < result.size(); ++i)
        {
            auto value = i;
            for (unsigned bit = 0; bit < 8; ++bit)
            {
                value = (value >> 1) ^ ((value & 1) ? 0xEDB88320u : 0u);
            }

            result[i] = value;
        }

        return result;
    }();

    std::uint32_t state_ = 0xFFFFFFFFu;
};
}