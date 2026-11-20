#include "AssetBrowser.h"
#include "ArthurBackend.h"
#include "BaseObject.h"
#include "Assets/AssetManager.h"
#include "Serialization/ArchiveFile.h"
#include "ImGuiApplication.h"
#include "imgui.h"


struct test : OOT::BaseObject
{
    IMPL_RTTI(test)
    int value = 0;
    void Serialize(oot::serialization::Archive& ar) override { ar.serialize(value); }
    void Deserialize(oot::serialization::Archive& ar) override { ar.deserialize(value); }
};

struct test2 : OOT::BaseObject
{
    IMPL_RTTI(test2)
    std::string name;
   
    
    void Serialize(oot::serialization::Archive& ar) override
    {
	    ar.serialize(name);
       
    }
    void Deserialize(oot::serialization::Archive& ar) override
    {
	    ar.deserialize(name);
    }
};

struct test3 : test, test2
{
    IMPL_RTTI(test3)
    std::map<std::string, std::vector<int>> values;
    void Serialize(oot::serialization::Archive& ar) override
    {
        test::Serialize(ar);
        test2::Serialize(ar);
        ar.serialize(values);
    }
    void Deserialize(oot::serialization::Archive& ar) override
    {
        test::Deserialize(ar);
        test2::Deserialize(ar);
        ar.deserialize(values);
    }
};

int RunAssetBrowser(unsigned frameLimit)
{
    using namespace oot::assets;
    int backend = 0; // Noe or Arthur; switching rebuilds the matching manager.
    const auto root = std::filesystem::absolute("archives");
    auto directory = root;
    auto registryPath = directory / "registry.json";
    auto bootstrap = [&](int choice, const std::filesystem::path& folder)
    {
        if (std::filesystem::exists(folder / "registry.json")) return;
        AssetRegistry initial;
        for (bool json : {false, true})
        {
            const std::string filename = json ? "imgui-demo.json" : (choice == 0 ? "imgui-demo.bin" : "imgui-demo.txt");
            const auto path = folder / "assets" / filename;
            if (!std::filesystem::exists(path))
            {
                if (choice == 0)
                {
                    test3 demo; demo.value = 99; demo.name = "Noe example";
                    demo.values = {{"coordinates", {4, 5, 6}}};
                    if (json) oot::serialization::SaveJson(path, demo);
                    else oot::serialization::SaveBinary(path, demo);
                }
                else sample::arthur::Save(path, {73, "Arthur example", {{"coordinates", {1, 2, 3}}}}, json);
            }
            initial.Upsert({json ? 1002u : 1001u, json ? "Exemple JSON" : "Exemple binaire", OOT::TypeInfo::Name<test3>(), filename,
                json ? AssetFormat::Json : AssetFormat::Binary, std::filesystem::file_size(path), {choice == 0 ? "noe" : "arthur", "example"}});
        }
        initial.Save(folder / "registry.json");
    };
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
        const auto nextDirectory = backend == 0 ? root : root / "arthur";
        const auto nextPath = nextDirectory / "registry.json";
        bootstrap(backend, nextDirectory);
        auto nextRegistry = std::make_unique<AssetRegistry>();
        nextRegistry->Load(nextPath);
        auto nextManager = std::make_unique<AssetManager>(*nextRegistry, nextDirectory / "assets");
        if (backend == 0) nextManager->RegisterArchiveType<test3>();
        else nextManager->RegisterLoader<test3>(OOT::TypeInfo::Name<test3>(), [](const AssetMetadata& metadata, const auto& path)
        {
            auto data = sample::arthur::Load(path, metadata.format == AssetFormat::Json);
            auto result = std::make_shared<test3>();
            result->value = data.value; result->name = std::move(data.name); result->values = std::move(data.values);
            return result;
        });
        auto nextRows = nextRegistry->Snapshot();
        loaded.clear(); pending.clear();
        manager = std::move(nextManager); registry = std::move(nextRegistry); rows = std::move(nextRows);
        directory = nextDirectory; registryPath = nextPath;
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
        ImGui::BeginDisabled(!pending.empty());
        ImGui::SetNextItemWidth(240);
        const int previousBackend = backend;
        if (ImGui::Combo("Implementation", &backend, "Noe\0Arthur\0"))
        {
            try { reload(); }
            catch (const std::exception& e) { backend = previousBackend; error = e.what(); }
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("%s", backend == 0 ? "Archive commune / JSON et binaire portable" : "ArchiveStrategy + OOT::Serialize / JSON et binaire natif");
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
                auto exportAsset = [&](bool json)
                {
                    const auto file = directory / "exports" / (std::to_string(selected) + (json ? ".json" : (backend == 0 ? ".bin" : ".txt")));
                    if (backend == 0)
                    {
                        if (json) oot::serialization::SaveJson(file, *it->second);
                        else oot::serialization::SaveBinary(file, *it->second);
                    }
                    else sample::arthur::Save(file, {it->second->value, it->second->name, it->second->values}, json);
                    notice = "Sauvegarde : " + file.string();
                };
                if (ImGui::Button("Sauvegarder JSON")) attempt([&] { exportAsset(true); });
                ImGui::SameLine();
                if (ImGui::Button("Sauvegarder binaire")) attempt([&] { exportAsset(false); });
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

