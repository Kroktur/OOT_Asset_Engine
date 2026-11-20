// SPOILER - The Legend of Zelda: Ocarina of Time : Ganondorf devient Ganon lors du combat final.
#include "ArthurBackend.h"
#include "Serialization/ArchiveFile.h"
#include "Object.h"
#include <cmath>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <set>

namespace sample::objects
{
namespace
{
    constexpr const char* Marker = "OOT-Arthur-Objects-1";
    // Arthur's reader allocates from file counts and uses asserts. Check the
    // Object.h schema before invoking it, without changing his implementation.
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
            if (String() != Marker) Fail();
            Object(0);
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
        void Object(unsigned depth)
        {
            if (depth >= 32) Fail();
            const auto id = Number<OOT::rtti_id_type>();
            if (id == OOT::TypeInfo::Id<IntegralObject>())
            {
                Number<int>();
                float value;
                if (json_)
                {
                    if (cursor_ >= values_.size() || !values_[cursor_].is_number()) Fail();
                    const double number = values_[cursor_++].get<double>();
                    if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max()) Fail();
                    value = static_cast<float>(number);
                }
                else
                {
                    if (bytes_.size() - cursor_ < sizeof(float)) Fail();
                    std::memcpy(&value, bytes_.data() + cursor_, sizeof(float));
                    cursor_ += sizeof(float);
                }
                if (!std::isfinite(value)) Fail();
            }
            else if (id == OOT::TypeInfo::Id<MapObject>())
            {
                std::set<std::string> keys;
                const auto count = Count();
                for (std::size_t i = 0; i < count; ++i)
                {
                    if (!keys.insert(String()).second) Fail();
                    Number<int>(); Number<int>();
                }
            }
            else if (id == OOT::TypeInfo::Id<PolyMorphObject>()) Object(depth + 1);
            else Fail();
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

void SaveArthur(const std::filesystem::path& path, OOT::BaseObject& value, bool json)
{
    StagingFile staging(json);
    {
        std::lock_guard lock(factoryMutex);
        ::Archive archive;
        archive.OpenFile(staging.file);
        archive.ClearFile();
        OOT::Serialize<std::string>::Write(archive, Marker);
        auto* object = &value;
        OOT::Serialize<OOT::BaseObject*>::Write(archive, object);
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

std::unique_ptr<OOT::BaseObject> LoadArthur(const std::filesystem::path& path, bool json)
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
    return object;
}


namespace
{
    void Setup(oot::serialization::Archive& archive)
    {
        archive.registerType<OOT::BaseObject, MapObject>();
        archive.registerType<OOT::BaseObject, IntegralObject>();
        archive.registerType<OOT::BaseObject, PolyMorphObject>();
    }
    template<class Format> void SaveNoe(const std::filesystem::path& path, OOT::BaseObject& object, bool json)
    {
        std::ostringstream output(std::ios::binary);
        Format archive(output);
        Setup(archive);
        std::string type = OOT::TypeInfo::Name(object);
        archive.serialize(type);
        object.Serialize(archive);
        archive.finish();
        oot::serialization::detail::SaveFile(path, output.str(), json, {});
    }
    template<class Format> std::unique_ptr<OOT::BaseObject> LoadNoe(const std::filesystem::path& path, bool json)
    {
        std::istringstream input(oot::serialization::detail::ReadFile(path, json, {}), std::ios::binary);
        Format archive(input);
        Setup(archive);
        std::string type;
        archive.deserialize(type);
        auto object = Create(type);
        object->Deserialize(archive);
        archive.finish();
        if (input.bad() || (!input.eof() && input.peek() != std::char_traits<char>::eof()))
            throw oot::serialization::ArchiveError("Trailing object data");
        return object;
    }
    void ValidateObject(OOT::BaseObject& object, unsigned depth = 0)
    {
        if (depth >= 32) throw std::runtime_error("Object nesting limit exceeded");
        if (auto* value = dynamic_cast<IntegralObject*>(&object))
        {
            if (!std::isfinite(value->FloatValue())) throw std::runtime_error("Float must be finite");
        }
        else if (auto* value = dynamic_cast<MapObject*>(&object))
        {
            if (value->Entries().size() > 1'000'000) throw std::runtime_error("Too many map entries");
        }
        else if (auto* value = dynamic_cast<PolyMorphObject*>(&object))
        {
            if (!value->GetObject()) throw std::runtime_error("Missing child object");
            ValidateObject(*value->GetObject(), depth + 1);
        }
        else throw std::runtime_error("Unsupported object type");
    }
}
std::unique_ptr<OOT::BaseObject> Create(std::string_view type)
{
    if (type == "MapObject") return std::make_unique<MapObject>();
    if (type == "IntegralObject") return std::make_unique<IntegralObject>();
    if (type == "PolyMorphObject") return std::make_unique<PolyMorphObject>();
    throw oot::serialization::ArchiveError("Unsupported object type");
}
void Save(const std::filesystem::path& path, OOT::BaseObject& object, bool json, int backend)
{
    ValidateObject(object);
    if (backend == 1) SaveArthur(path, object, json);
    else
    {
        std::lock_guard lock(factoryMutex);
        if (json) SaveNoe<oot::serialization::TextArchive>(path, object, true);
        else SaveNoe<oot::serialization::BinaryArchive>(path, object, false);
    }
}
std::shared_ptr<OOT::BaseObject> Load(const std::filesystem::path& path, bool json, int backend)
{
    std::unique_ptr<OOT::BaseObject> result;
    if (backend == 1) result = LoadArthur(path, json);
    else
    {
        std::lock_guard lock(factoryMutex);
        result = json ? LoadNoe<oot::serialization::TextArchive>(path, true)
                      : LoadNoe<oot::serialization::BinaryArchive>(path, false);
    }
    if (!result) throw std::runtime_error("Missing object");
    ValidateObject(*result);
    return result;
}
}
