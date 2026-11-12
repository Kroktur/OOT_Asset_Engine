#include <iostream>
#include <sstream>
#include "Factory.h"
#include "RTTI.h"
#include "Serialization/BinaryArchive.h"
#include "Serialization/TextArchive.h"

using oot::serialization::Archive;

struct test : OOT::BaseRtti
{
    IMPL_RTTI(test)
    int value = 0;
    void serialize(Archive& ar) { ar.serialize(value); }
    void deserialize(Archive& ar) { ar.deserialize(value); }
};

struct test2 : OOT::BaseRtti
{
    IMPL_RTTI(test2)
    std::string name;
    void serialize(Archive& ar) { ar.serialize(name); }
    void deserialize(Archive& ar) { ar.deserialize(name); }
};

struct test3 : test, test2
{
    IMPL_RTTI(test3)
    std::map<std::string, std::vector<int>> values;
    void serialize(Archive& ar)
    {
        test::serialize(ar);
        test2::serialize(ar);
        ar.serialize(values);
    }
    void deserialize(Archive& ar)
    {
        test::deserialize(ar);
        test2::deserialize(ar);
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
    if (!derived || derived->value != 42 || derived->name != "Asset example"
        || derived->values.at("coordinates") != std::vector<int>{1, 2, 3})
        throw std::runtime_error("Archive round-trip failed");
    std::cout << format << ": " << OOT::TypeInfo::Name(*restored)
              << ", value=" << derived->value << ", name=" << derived->name << '\n';
}

int main()
{
    try
    {
        // The factory is global: register each concrete type only once.
        OOT::Factory::Register<test3>(OOT::TypeInfo::Id<test3>());
        OOT::Factory::Register<int>(0);
        auto number = OOT::Factory::Create<int, std::unique_ptr<int>>(0);
        example<oot::serialization::BinaryArchive>("Binary");
        example<oot::serialization::TextArchive>("Text");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
