#include <iostream>
#include <sstream>
#include "Serialization/BinaryArchive.h"
#include "Serialization/TextArchive.h"

using oot::serialization::Archive;

namespace OOT
{
    struct BaseObject : OOT::BaseRtti
    {
    public:
        BaseObject() = default;
        IMPL_RTTI(BaseObject)
        virtual void Serialize(Archive&) {}
        virtual void Deserialize(Archive&) {}
    };
}

struct test : OOT::BaseObject
{
    IMPL_RTTI(test)
    int value = 0;
    void Serialize(Archive& ar) override { ar.serialize(value); }
    void Deserialize(Archive& ar) override { ar.deserialize(value); }
};

struct test2 : OOT::BaseObject
{
    IMPL_RTTI(test2)
    std::string name;
    void Serialize(Archive& ar) override { ar.serialize(name); }
    void Deserialize(Archive& ar) override { ar.deserialize(name); }
};

struct test3 : test, test2
{
    IMPL_RTTI(test3)
    std::map<std::string, std::vector<int>> values;
    void Serialize(Archive& ar) override
    {
        test::Serialize(ar);
        test2::Serialize(ar);
        ar.serialize(values);
    }
    void Deserialize(Archive& ar) override
    {
        test::Deserialize(ar);
        test2::Deserialize(ar);
        ar.deserialize(values);
    }
};

template<class ArchiveType> void example(const char* format)
{
    auto object = std::make_unique<test3>();
    object->value = 42;
    object->name = "Asset example";
    object->values = {{"coordinates", {1, 2, 3}}};
    std::unique_ptr<test2> original = std::move(object);

    std::ostringstream output;
    ArchiveType writer(output);
    writer.template registerType<test2, test3>();
    writer.serialize(original);

    std::istringstream input(output.str());
    ArchiveType reader(input);
    reader.template registerType<test2, test3>();
    std::unique_ptr<test2> restored;
    reader.deserialize(restored);
    const auto* derived = dynamic_cast<test3*>(restored.get());

    if (!derived || derived->value != 42 || derived->name != "Asset example" || derived->values.at("coordinates") != std::vector<int>{1, 2, 3})
    {
        throw std::runtime_error("Archive round-trip failed");
    }

    std::cout << format << ": " << OOT::TypeInfo::Name(*restored) << ", value=" << derived->value << ", name=" << derived->name << '\n';
}

int main()
{
    try
    {
        OOT::ObjectData data = OOT::GetDataRegistry().Create(OOT::TypeInfo::Id<test3>());
        OOT::ObjectData data2 = OOT::GetDataRegistry().Create(OOT::TypeInfo::Id<test2>());

        std::unique_ptr<OOT::BaseObject> create2 = OOT::GetRTTIFactoryId().Create<std::unique_ptr<test2>>(data2.id);
        auto concrete = OOT::GetRTTIFactoryId().Create<std::unique_ptr<test3>>(data.id);
        OOT::BaseObject* create = static_cast<test2*>(concrete.get());

        std::cout << OOT::TypeInfo::Name(*create) << '\n';
        std::cout << OOT::TypeInfo::Name(*create2) << '\n';

        example<oot::serialization::BinaryArchive>("Binary");
        example<oot::serialization::TextArchive>("Text");
    }

    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
