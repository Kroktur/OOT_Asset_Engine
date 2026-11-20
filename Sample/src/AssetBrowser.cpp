// SPOILER - The Legend of Zelda: Ocarina of Time : Saria devient la Sage de la Foret.
#include "AssetBrowser.h"
#include "ArthurBackend.h"
#include "ObjectEditor.h"
#include "BaseObject.h"
#include "Assets/AssetManager.h"
#include "ImGuiApplication.h"
#include "imgui.h"
#include <algorithm>
#include <array>

int RunAssetBrowser(unsigned frameLimit)
{
    using namespace oot::assets;
    // The collection owns the objects; selection and editor access them as BaseObject*.
    std::vector<std::shared_ptr<OOT::BaseObject>> objects;
    int selected = -1, newType = 0, backend = 0;
    bool focusObjects = false;
    AssetId selectedAsset = 0;
    std::array<char, 128> assetName{};
    const auto root = std::filesystem::absolute("archives/objects");
    std::filesystem::path directory;
    std::unique_ptr<AssetRegistry> registry;
    std::unique_ptr<AssetManager> manager;
    std::vector<AssetMetadata> rows;
    std::unordered_map<AssetId, AssetFuture<OOT::BaseObject>> pending;
    std::string error, notice = "Creez un objet, selectionnez-le et modifiez ses proprietes.";
    ImGuiTextFilter filter;

    auto makeManager = [&](const AssetRegistry& source, const std::filesystem::path& folder, int choice)
    {
        auto result = std::make_unique<AssetManager>(source, folder / "assets");
        for (const auto* type : sample::objects::Types)
            result->RegisterLoader<OOT::BaseObject>(type, [choice](const AssetMetadata& metadata, const auto& path)
            {
                auto object = sample::objects::Load(path, metadata.format == AssetFormat::Json, choice);
                if (OOT::TypeInfo::Name(*object) != metadata.type)
                    throw std::runtime_error("Object type differs from registry metadata");
                return object;
            });
        return result;
    };
    auto reload = [&]
    {
        const auto folder = root / (backend == 0 ? "noe" : "arthur");
        auto nextRegistry = std::make_unique<AssetRegistry>();
        if (std::filesystem::exists(folder / "registry.json")) nextRegistry->Load(folder / "registry.json");
        auto nextManager = makeManager(*nextRegistry, folder, backend);
        auto nextRows = nextRegistry->Snapshot();
        manager = std::move(nextManager); registry = std::move(nextRegistry);
        directory = folder; rows = std::move(nextRows);
        selectedAsset = rows.empty() ? 0 : rows.front().id;
        notice = "Registre charge. Les objets en memoire sont conserves.";
    };
    auto attempt = [&](auto action)
    {
        try { action(); error.clear(); }
        catch (const std::exception& e) { error = e.what(); }
    };
    auto save = [&](bool json)
    {
        auto& object = *objects.at(selected);
        AssetId id = 1;
        for (const auto& row : rows)
        {
            if (row.id == std::numeric_limits<AssetId>::max()) throw std::runtime_error("Asset ID limit reached");
            id = std::max(id, row.id + 1);
        }
        std::string filename;
        std::filesystem::path file;
        do
        {
            filename = std::to_string(id) + (json ? ".json" : (backend == 0 ? ".bin" : ".txt"));
            file = directory / "assets" / filename;
            if (!std::filesystem::exists(file)) break;
            if (++id == 0) throw std::runtime_error("Asset ID limit reached");
        } while (true);
        sample::objects::Save(file, object, json, backend);
        // Publish a new registry snapshot only after the file was saved and verified.
        auto nextRegistry = std::make_unique<AssetRegistry>();
        for (const auto& row : rows) nextRegistry->Upsert(row);
        const std::string type = OOT::TypeInfo::Name(object);
        nextRegistry->Upsert({id, assetName[0] ? std::string(assetName.data()) : type + " " + std::to_string(id),
            type, filename, json ? AssetFormat::Json : AssetFormat::Binary,
            std::filesystem::file_size(file), {"object", backend == 0 ? "noe" : "arthur"}});
        auto nextManager = makeManager(*nextRegistry, directory, backend);
        auto nextRows = nextRegistry->Snapshot();
        nextRegistry->Save(directory / "registry.json");
        manager = std::move(nextManager); registry = std::move(nextRegistry); rows = std::move(nextRows);
        selectedAsset = id;
        notice = "Sauvegarde et ajout au registre : " + file.string();
    };
    attempt(reload);
    return RunImGuiApplication([&]
    {
        for (auto it = pending.begin(); it != pending.end();)
        {
            if (!it->second.ready()) { ++it; continue; }
            attempt([&]
            {
                auto object = it->second.get();
                auto found = std::find(objects.begin(), objects.end(), object);
                if (found == objects.end())
                {
                    objects.push_back(std::move(object));
                    selected = static_cast<int>(objects.size()) - 1;
                }
                else selected = static_cast<int>(found - objects.begin());
                focusObjects = true;
                notice = "Objet charge, CRC valide. Selectionnez-le pour le modifier.";
            });
            it = pending.erase(it);
        }
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("OOT Asset Engine", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("OBJETS / ARCHIVES");
        ImGui::BeginDisabled(!pending.empty());
        const int previous = backend;
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("Systeme d'archive", &backend, "Noe\0Arthur\0"))
        {
            try { reload(); error.clear(); }
            catch (const std::exception& e) { backend = previous; error = e.what(); }
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Les objets restent en memoire jusqu'a leur suppression. Chaque sauvegarde cree un nouvel asset.");
        const float width = ImGui::GetContentRegionAvail().x;
        if (ImGui::BeginTabBar("Workspace"))
        {
        if (ImGui::BeginTabItem("Objets en memoire", nullptr, focusObjects ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
        {
        focusObjects = false;
        ImGui::BeginChild("Objects", ImVec2(width * 0.30f, -105), ImGuiChildFlags_Borders);
        ImGui::TextWrapped("Objets en memoire : %zu", objects.size());
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##newtype", &newType, sample::objects::Types, 3);
        if (ImGui::Button("Creer un objet")) attempt([&]
        {
            objects.push_back(sample::objects::Create(sample::objects::Types[newType]));
            selected = static_cast<int>(objects.size()) - 1;
        });
        ImGui::Separator();
        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            const std::string label = std::to_string(i + 1) + " - " + OOT::TypeInfo::Name(*objects[i]);
            if (ImGui::Selectable(label.c_str(), selected == static_cast<int>(i))) selected = static_cast<int>(i);
            ImGui::PopID();
        }
        ImGui::EndChild(); ImGui::SameLine();
        ImGui::BeginChild("Editor", ImVec2(0, -105), ImGuiChildFlags_Borders);
        if (selected >= 0 && selected < static_cast<int>(objects.size()))
        {
            ImGui::InputText("Nom de l'asset", assetName.data(), assetName.size());
            ImGui::BeginDisabled(!pending.empty() || !registry);
            if (ImGui::Button("Sauvegarder JSON")) attempt([&] { save(true); });
            if (ImGui::Button("Sauvegarder binaire")) attempt([&] { save(false); });
            ImGui::EndDisabled();
            if (ImGui::Button("Retirer de la liste"))
            {
                objects.erase(objects.begin() + selected);
                selected = objects.empty() ? -1 : std::min(selected, static_cast<int>(objects.size()) - 1);
                if (manager) manager->CollectUnused();
            }
            ImGui::Separator();
            if (selected >= 0) DrawObjectEditor(objects[selected].get());
        }
        else ImGui::TextWrapped("Creez un objet ou chargez un asset du registre.");
        ImGui::EndChild();
        ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Registre / chargement"))
        {
        ImGui::BeginChild("Registry", ImVec2(0, -105), ImGuiChildFlags_Borders);
        ImGui::Text("Registre : %zu assets", rows.size());
        ImGui::BeginDisabled(!pending.empty());
        if (ImGui::Button("Actualiser")) attempt(reload);
        ImGui::SameLine();
        if (ImGui::Button("Vider le cache") && manager) manager->ClearCache();
        ImGui::EndDisabled();
        filter.Draw("Filtrer", -1);
        for (const auto& row : rows)
        {
            if (!filter.PassFilter((row.name + " " + row.type + " " + row.path).c_str())) continue;
            ImGui::PushID(std::to_string(row.id).c_str());
            if (ImGui::Selectable("##asset", selectedAsset == row.id)) selectedAsset = row.id;
            ImGui::SameLine(); ImGui::TextUnformatted(row.name.c_str());
            ImGui::PopID();
        }
        ImGui::Separator();
        if (const auto metadata = registry ? registry->Find(selectedAsset) : std::optional<AssetMetadata>{})
        {
            ImGui::TextWrapped("Type : %s", metadata->type.c_str());
            ImGui::TextWrapped("Fichier : %s", metadata->path.c_str());
            ImGui::Text("Taille : %llu octets", static_cast<unsigned long long>(metadata->fileBytes));
            ImGui::TextUnformatted(metadata->format == AssetFormat::Json ? "Format : JSON" : "Format : binaire");
            const auto state = manager->State(selectedAsset);
            ImGui::Text("Etat : %s", state == AssetState::Loading ? "Chargement" : state == AssetState::Ready ? "En cache" : state == AssetState::Failed ? "Erreur" : "Non charge");
            ImGui::BeginDisabled(pending.contains(selectedAsset));
            if (ImGui::Button("Charger async")) attempt([&]
            {
                pending.emplace(selectedAsset, manager->LoadAsync<OOT::BaseObject>(selectedAsset));
            });
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        }
        ImGui::Separator();
        ImGui::TextWrapped("%s", error.empty() ? notice.c_str() : error.c_str());
        ImGui::TextWrapped("Dossier : %s", directory.string().c_str());
        ImGui::End();
    }, frameLimit);
}
