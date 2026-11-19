#include <iostream>
#include <sstream>
#include "Serialization/BinaryArchive.h"
#include "Serialization/TextArchive.h"
#include "Serialization/ArchiveFile.h"
#include "Assets/AssetManager.h"

#include "Factory.h"
#include "RTTI.h"
#include "KTR/Hash/KTR_Hasher.h"
#include "KTR/Containers/KTR_FlatHashMap.h"
#include "filesystem"
#include "fstream"
#include "RTTI.h"
#include <array>
#include <map>
#include <unordered_map>

#include "BaseObject.h"
#include "Serialize.h"


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
private:
	int t;
};
struct test4 : OOT::BaseObject
{
	IMPL_RTTI(test4)
    IMPL_RTTI(test)
    int value = 0;
    void Serialize(Archive& ar) override { ar.serialize(value); }
    void Deserialize(Archive& ar) override { ar.deserialize(value); }
};

struct test2 : OOT::BaseObject
{
	
	test2()
	{
		t = new test4{};
	}
	~test2()
	{
		delete t;
	}
	void Read(Archive& ar) override
	{
		delete t;
		t = OOT::Serialize<OOT::BaseObject*>::Read(ar);
		toto = OOT::Serialize<int>::Read(ar);
		std::cout << OOT::TypeInfo::Name(*this);
		std::cout << OOT::TypeInfo::Name(*t);
		std::cout << toto;
	}
	void Write(Archive& ar) override
	{
		OOT::Serialize<OOT::BaseObject*>::Write(ar, t);
		OOT::Serialize<int>::Write(ar,toto);

	}
	IMPL_RTTI(test2)
private:
	int toto = 5;
	OOT::BaseObject* t;
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

    const auto setup = [](Archive& archive) { archive.registerType<test2, test3>(); };
    std::unique_ptr<test2> restored;
    if constexpr (std::is_same_v<ArchiveType, oot::serialization::BinaryArchive>)
    {
        oot::serialization::SaveBinary("archives/example.bin", original, setup);
        oot::serialization::LoadBinary("archives/example.bin", restored, setup);
    }

    else
    {
        oot::serialization::SaveJson("archives/example.json", original, setup);
        oot::serialization::LoadJson("archives/example.json", restored, setup);
    }
    const auto* derived = dynamic_cast<test3*>(restored.get());

    if (!derived || derived->value != 42 || derived->name != "Asset example" || derived->values.at("coordinates") != std::vector<int>{1, 2, 3})
    {
        throw std::runtime_error("Archive round-trip failed");
    }

    std::cout << format << ": " << OOT::TypeInfo::Name(*restored) << ", value=" << derived->value << ", name=" << derived->name << '\n';
}

void assetExample()
{
    using namespace oot::assets;
    test3 source;
    source.value = 99;
    source.name = "Registered asset";
    source.values = {{"coordinates", {4, 5, 6}}};
    oot::serialization::SaveBinary("archives/assets/sample.bin", source);

    AssetRegistry registry;
    registry.Upsert({1001, source.name, OOT::TypeInfo::Name<test3>(), "sample.bin", AssetFormat::Binary, std::filesystem::file_size("archives/assets/sample.bin"), {"example", "test"}});
    registry.Save("archives/registry.json");

    AssetRegistry restoredRegistry;
    restoredRegistry.Load("archives/registry.json");
    const auto metadata = restoredRegistry.FindTag("example");
    std::cout << "Registry: " << metadata.at(0).name << " (" << metadata.at(0).fileBytes << " bytes)\n";

    AssetManager manager(restoredRegistry, "archives/assets");
    manager.RegisterArchiveType<test3>();
    {
        auto first = manager.LoadAsync<test3>(1001);
        auto second = manager.LoadAsync<test3>(1001);
        auto asset = first.get();
        if (asset != second.get() || asset->value != 99 || asset->values != source.values)
        {
            throw std::runtime_error("Async asset round-trip failed");
        }

        std::cout << "Async asset: " << asset->name << ", shared instance\n";
    }
    std::cout << "Unused resources released: " << manager.CollectUnused() << '\n';
}

#ifdef OOT_WITH_IMGUI
#include "imgui.h"
#include "ImGuiApplication.h"

int graphicalExample(unsigned frameLimit)
{
    using namespace oot::assets;
    const auto directory = std::filesystem::absolute("archives");
    const auto registryPath = directory / "registry.json";
    if (!std::filesystem::exists(registryPath))
    {
        test3 demo;
        demo.value = 99;
        demo.name = "Registered asset";
        demo.values = {{"coordinates", {4, 5, 6}}};
        // Never overwrite an existing resource when bootstrapping the demo.
        const auto assetPath = directory / "assets" / "imgui-demo.bin";
        if (!std::filesystem::exists(assetPath)) oot::serialization::SaveBinary(assetPath, demo);
        AssetRegistry initial;
        initial.Upsert({1001, demo.name, OOT::TypeInfo::Name<test3>(), "imgui-demo.bin",
            AssetFormat::Binary, std::filesystem::file_size(assetPath), {"example"}});
        initial.Save(registryPath);
    }


int main(int argc, char* argv[])
{
	Archive archive;
	std::filesystem::path path = argv[0];
	path = path.parent_path().parent_path().parent_path().parent_path().parent_path() / "Resources";
	archive.OpenFile(path / "test_1.json");
	//archive.ClearFile();
	//OOT::Serialize<float>::Write(archive, 5.53f);
	//std::cout << OOT::Serialize<float>::Read(archive) << std::endl;
	using t = std::vector<
		std::map<
		std::string,
		std::vector<std::pair<int, float>>
		>
	>;
	t test;/* = {
	{
		{
			"Player",
			{
				{100, 10.5f},
				{200, 20.5f}
			}
		},
		{
			"Enemy",
			{
				{50, 5.5f},+
				{75, 7.5f}
			}
		}
	}
	};*/
	//OOT::Serialize<t>::Write(archive, test);
	test = OOT::Serialize<t>::Read(archive);
	archive.CloseFile();
	//delete test;
    std::unique_ptr<AssetRegistry> registry;
    std::unique_ptr<AssetManager> manager;
    std::vector<AssetMetadata> rows;
    std::unordered_map<AssetId, AssetFuture<test3>> pending;
    std::unordered_map<AssetId, std::shared_ptr<test3>> loaded;
    std::string error, notice = "Selectionnez un asset pour consulter ses informations.";
    AssetId selected = 0;
    ImGuiTextFilter filter;
    auto reload = [&]
    {
        auto nextRegistry = std::make_unique<AssetRegistry>();
        nextRegistry->Load(registryPath);
        auto nextManager = std::make_unique<AssetManager>(*nextRegistry, directory / "assets");
        nextManager->RegisterArchiveType<test3>();
        auto nextRows = nextRegistry->Snapshot();
        loaded.clear(); pending.clear();
        manager = std::move(nextManager); registry = std::move(nextRegistry); rows = std::move(nextRows);
        selected = rows.empty() ? 0 : rows.front().id;
        error.clear(); notice = "Registre charge. Aucun fichier asset charge automatiquement.";
    };
    auto attempt = [&](auto action)
    {
        try { action(); error.clear(); }
        catch (const std::exception& e) { error = e.what(); }
    };
    attempt(reload);
    auto stateLabel = [](AssetState state)
    {
        switch (state)
        {
        case AssetState::Loading: return "Chargement...";
        case AssetState::Ready: return "En cache";
        case AssetState::Failed: return "Erreur";
        default: return "Non charge";
        }
    };
    return RunImGuiApplication([&]
    {
        for (auto it = pending.begin(); it != pending.end();)
        {
            if (!it->second.ready()) { ++it; continue; }
            try { loaded[it->first] = it->second.get(); notice = "Asset charge et CRC valide."; }
            catch (const std::exception& e) { error = e.what(); }
            it = pending.erase(it);
        }
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("OOT Asset Engine", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.f, 1.f), "OOT / ASSET ENGINE");
        ImGui::TextUnformatted("Registre et ressources");
        ImGui::TextDisabled("%zu assets   |   %zu demandes en cours   |   %.1f FPS", rows.size(), pending.size(), ImGui::GetIO().Framerate);
        ImGui::Separator();
        ImGui::BeginDisabled(!pending.empty());
        if (ImGui::Button("Recharger le registre")) attempt(reload);
        ImGui::SameLine();
        if (ImGui::Button("Vider le cache") && manager)
        { loaded.clear(); manager->ClearCache(); notice = "Cache libere."; }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Collecter les ressources inutilisees") && manager)
            notice = std::to_string(manager->CollectUnused()) + " ressource(s) liberee(s).";
        ImGui::Spacing();
        filter.Draw("Filtrer nom / type / chemin / tag", 300);
        ImGui::Spacing();
        const float listWidth = ImGui::GetContentRegionAvail().x * 0.58f;
        ImGui::BeginChild("List", ImVec2(listWidth, -100), ImGuiChildFlags_Borders);
        if (ImGui::BeginTable("Assets", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("Nom"); ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Format"); ImGui::TableSetupColumn("Etat"); ImGui::TableHeadersRow();
            for (const auto& row : rows)
            {
                std::string search = row.name + " " + row.type + " " + row.path;
                for (const auto& tag : row.tags) search += " " + tag;
                if (!filter.PassFilter(search.c_str())) continue;
                ImGui::PushID(std::to_string(row.id).c_str());
                ImGui::TableNextRow(); ImGui::TableNextColumn();
                // Hidden ID avoids treating metadata as an ImGui widget label.
                if (ImGui::Selectable("##asset", selected == row.id, ImGuiSelectableFlags_SpanAllColumns)) selected = row.id;
                ImGui::SameLine(); ImGui::TextUnformatted(row.name.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(row.type.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(row.format == AssetFormat::Json ? "JSON" : "Binaire");
                ImGui::TableNextColumn(); ImGui::TextUnformatted(stateLabel(manager->State(row.id)));
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (rows.empty()) ImGui::TextWrapped("Aucun asset. Chargez un registre valide pour commencer.");
        ImGui::EndChild(); ImGui::SameLine();
        ImGui::BeginChild("Details", ImVec2(0, -100), ImGuiChildFlags_Borders);
        const auto metadata = registry ? registry->Find(selected) : std::optional<AssetMetadata>{};
        if (metadata)
        {
            ImGui::TextUnformatted(metadata->name.c_str()); ImGui::Separator();
            ImGui::Text("ID : %llu", static_cast<unsigned long long>(metadata->id));
            ImGui::TextWrapped("Type : %s", metadata->type.c_str());
            ImGui::TextWrapped("Chemin : %s", metadata->path.c_str());
            ImGui::Text("Fichier : %llu octets", static_cast<unsigned long long>(metadata->fileBytes));
            for (const auto& tag : metadata->tags) ImGui::BulletText("%s", tag.c_str());
            const bool supported = metadata->type == OOT::TypeInfo::Name<test3>();
            ImGui::BeginDisabled(!supported || pending.contains(selected));
            if (ImGui::Button("Charger async")) attempt([&]
            {
                pending.emplace(selected, manager->LoadAsync<test3>(selected));
                notice = "Chargement demande...";
            });
            ImGui::SameLine();
            if (ImGui::Button("Liberer"))
            { loaded.erase(selected); manager->Unload(selected); notice = "Reference et cache liberes."; }
            ImGui::EndDisabled();
            if (!supported) ImGui::TextWrapped("Aucun chargeur enregistre dans cet exemple pour ce type.");
            ImGui::Spacing(); ImGui::Separator();
            if (auto it = loaded.find(selected); it != loaded.end())
            {
                ImGui::TextUnformatted("Contenu charge");
                ImGui::Text("Valeur : %d", it->second->value);
                ImGui::TextWrapped("Nom : %s", it->second->name.c_str());
                for (const auto& [key, values] : it->second->values)
                {
                    ImGui::TextUnformatted(key.c_str());
                    for (int value : values) { ImGui::SameLine(); ImGui::Text("%d", value); }
                }
            }
            else ImGui::TextWrapped("Les metadonnees sont consultables sans charger le contenu.");
        }
        else ImGui::TextUnformatted("Aucune selection.");
        ImGui::EndChild();
        ImGui::Separator();
        if (!error.empty()) ImGui::TextWrapped("Erreur : %s", error.c_str());
        else ImGui::TextWrapped("%s", notice.c_str());
        ImGui::TextWrapped("Registre : %s", registryPath.string().c_str());
        ImGui::End();
    }, frameLimit);
}
#endif

int main(int argc, char** argv)
{
    try
    {
#ifdef OOT_WITH_IMGUI
        if (argc < 2 || std::string_view(argv[1]) != "--console")
            return graphicalExample(argc > 1 && std::string_view(argv[1]) == "--smoke-test" ? 5 : 0);
#endif
        OOT::ObjectData data = OOT::GetDataRegistry().Create(OOT::TypeInfo::Id<test3>());
        OOT::ObjectData data2 = OOT::GetDataRegistry().Create(OOT::TypeInfo::Id<test2>());

        std::unique_ptr<OOT::BaseObject> create2 = OOT::GetRTTIFactoryId().Create<std::unique_ptr<test2>>(data2.id);
        auto concrete = OOT::GetRTTIFactoryId().Create<std::unique_ptr<test3>>(data.id);
        OOT::BaseObject* create = static_cast<test2*>(concrete.get());

        std::cout << OOT::TypeInfo::Name(*create) << '\n';
        std::cout << OOT::TypeInfo::Name(*create2) << '\n';

	delete create;
	return 0;
}
        example<oot::serialization::BinaryArchive>("Binary");
        example<oot::serialization::TextArchive>("JSON");
        assetExample();
    }

    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
