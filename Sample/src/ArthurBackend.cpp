#include "ArthurBackend.h"
#include "Serialization/ArchiveFile.h"
#include "BaseObject.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <set>

namespace sample::arthur
{
namespace
{
    constexpr const char* Marker = "OOT-Arthur-1";
    // Keep Arthur's native API and polymorphic factory dispatch.
    struct ArthurAsset : OOT::BaseObject
    {
        IMPL_RTTI(ArthurAsset)
        AssetData data;
        void Write(::Archive& archive) override
        {
            OOT::Serialize<int>::Write(archive, data.value);
            OOT::Serialize<std::string>::Write(archive, data.name);
            OOT::Serialize<decltype(data.values)>::Write(archive, data.values);
        }
        void Read(::Archive& archive) override
        {
            data.value = OOT::Serialize<int>::Read(archive);
            data.name = OOT::Serialize<std::string>::Read(archive);
            data.values = OOT::Serialize<decltype(data.values)>::Read(archive);
        }
    };

    // Arthur's reader allocates from file counts and uses asserts. Check the
    // demo schema before invoking it, without changing his implementation.
    class Validator
    {
    public:
        Validator(const std::string& bytes, bool json) : bytes_(bytes), json_(json)
        {
            if (json_)
            {
                values_ = nlohmann::json::parse(bytes, [](int depth, auto event, auto&)
                {
                    if (depth > 1 || (depth == 1 && (event == nlohmann::json::parse_event_t::array_start
                        || event == nlohmann::json::parse_event_t::object_start)))
                        throw oot::serialization::ArchiveError("Invalid Arthur JSON structure");
                    return true;
                });
                if (!values_.is_array()) Fail();
            }
        }
        void Check()
        {
            if (String() != Marker || Number<OOT::rtti_id_type>() != OOT::TypeInfo::Id<ArthurAsset>()) Fail();
            Number<int>(); String();
            std::set<std::string> keys;
            const auto count = Count();
            for (std::size_t i = 0; i < count; ++i)
            {
                if (!keys.insert(String()).second) Fail();
                const auto size = Count();
                for (std::size_t j = 0; j < size; ++j) Number<int>();
            }
            if (cursor_ != (json_ ? values_.size() : bytes_.size())) Fail();
        }
    private:
        [[noreturn]] static void Fail() { throw oot::serialization::ArchiveError("Invalid Arthur asset or incompatible backend"); }
        template<class T> T Number()
        {
            if (!json_)
            {
                if (bytes_.size() - cursor_ < sizeof(T)) Fail();
                T value;
                std::memcpy(&value, bytes_.data() + cursor_, sizeof(T));
                cursor_ += sizeof(T);
                return value;
            }
            if (cursor_ >= values_.size()) Fail();
            const auto& token = values_[cursor_++];
            if (token.is_number_unsigned())
            {
                const auto value = token.get<std::uint64_t>();
                if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) Fail();
                return static_cast<T>(value);
            }
            if (!token.is_number_integer()) Fail();
            const auto value = token.get<std::int64_t>();
            if constexpr (std::is_unsigned_v<T>)
            {
                if (value < 0 || static_cast<std::uint64_t>(value) > std::numeric_limits<T>::max()) Fail();
            }
            else if (value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max()) Fail();
            return static_cast<T>(value);
        }
        std::size_t Count()
        {
            const auto value = Number<std::size_t>();
            if (value > 1'000'000 - elements_) Fail();
            elements_ += value;
            return value;
        }
        std::string String()
        {
            const auto size = Number<std::size_t>();
            if (size > 16 * 1024 * 1024) Fail();
            if (json_)
            {
                if (cursor_ >= values_.size() || !values_[cursor_].is_string()) Fail();
                auto text = values_[cursor_++].get<std::string>();
                if (text.size() != size) Fail();
                return text;
            }
            if (size > bytes_.size() - cursor_) Fail();
            auto text = bytes_.substr(cursor_, size);
            cursor_ += size;
            return text;
        }
        const std::string& bytes_;
        bool json_;
        nlohmann::json values_;
        std::size_t cursor_ = 0, elements_ = 0;
    };

    struct StagingFile
    {
        std::filesystem::path directory, file;
        explicit StagingFile(bool json)
        {
            static std::atomic<unsigned> sequence{0};
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            for (int i = 0; i < 128; ++i)
            {
                directory = std::filesystem::temp_directory_path() /
                    ("oot-arthur-" + std::to_string(stamp) + "-" + std::to_string(sequence++));
                if (std::filesystem::create_directory(directory))
                { file = directory / (json ? "payload.json" : "payload.txt"); return; }
            }
            throw oot::serialization::ArchiveError("Cannot create Arthur staging directory");
        }
        ~StagingFile()
        {
            std::error_code ignored;
            std::filesystem::remove(file, ignored);
            std::filesystem::remove(directory, ignored);
        }
    };
    std::mutex factoryMutex;
}

void Save(const std::filesystem::path& path, AssetData data, bool json)
{
    StagingFile staging(json);
    {
        std::lock_guard lock(factoryMutex);
        ::Archive archive;
        archive.OpenFile(staging.file);
        archive.ClearFile();
        OOT::Serialize<std::string>::Write(archive, Marker);
        auto derived = std::make_unique<ArthurAsset>();
        derived->data = std::move(data);
        std::unique_ptr<OOT::BaseObject> object = std::move(derived);
        OOT::Serialize<decltype(object)>::Write(archive, object);
        archive.CloseFile();
    }
    if (std::filesystem::file_size(staging.file) > oot::serialization::ArchiveLimits{}.maxArchiveBytes)
        throw oot::serialization::ArchiveError("Arthur asset exceeds size limit");
    std::ifstream input(staging.file, std::ios::binary);
    if (!input) throw oot::serialization::ArchiveError("Cannot read Arthur staging file");
    const std::string payload{std::istreambuf_iterator<char>(input), {}};
    Validator(payload, json).Check();
    // Reuse integrity and atomic replacement without changing Arthur's payload.
    oot::serialization::detail::SaveFile(path, payload, json, {});
}

AssetData Load(const std::filesystem::path& path, bool json)
{
    const auto payload = oot::serialization::detail::ReadFile(path, json, {});
    Validator(payload, json).Check();
    StagingFile staging(json);
    {
        std::ofstream output(staging.file, std::ios::binary);
        output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        output.close();
        if (!output) throw oot::serialization::ArchiveError("Cannot prepare Arthur archive");
    }
    std::lock_guard lock(factoryMutex);
    ::Archive archive;
    archive.OpenFile(staging.file);
    OOT::Serialize<std::string>::Read(archive);
    auto object = OOT::Serialize<std::unique_ptr<OOT::BaseObject>>::Read(archive);
    // CloseFile rewrites JSON in Arthur's implementation: only the staging copy
    // is opened, so loading never modifies the user's original file.
    archive.CloseFile();
    return dynamic_cast<ArthurAsset&>(*object).data;
}

void VerifyRoundTrips(const std::filesystem::path& directory)
{
    const AssetData original{73, "Arthur example", {{"coordinates", {1, 2, 3}}, {"empty", {}}}};
    for (bool json : {false, true})
    {
        const auto path = directory / (json ? "arthur.json" : "arthur.txt");
        Save(path, original, json);
        if (Load(path, json) != original) throw std::runtime_error("Arthur round-trip failed");
    }
}
}
