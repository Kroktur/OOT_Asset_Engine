// SPOILER - The Legend of Zelda: Ocarina of Time : Ruto devient la Sage de l'Eau.
#include "ObjectEditor.h"
#include "Object.h"
#include "ArthurBackend.h"
#include "imgui.h"
#include <array>

namespace
{
void Edit(OOT::BaseObject* object, unsigned depth)
{
    if (!object) { ImGui::TextUnformatted("Aucun objet."); return; }
    ImGui::PushID(object);
    ImGui::TextUnformatted(OOT::TypeInfo::Name(*object).c_str());
    if (auto* value = dynamic_cast<IntegralObject*>(object))
    {
        ImGui::InputInt("Entier", &value->Value());
        ImGui::InputFloat("Flottant", &value->FloatValue());
    }
    else if (auto* value = dynamic_cast<MapObject*>(object))
    {
        // Draft key is shared, but only the selected object's map is modified.
        static std::array<char, 256> key{};
        ImGui::InputText("Nouvelle cle", key.data(), key.size());
        const bool duplicate = value->Entries().contains(key.data());
        ImGui::BeginDisabled(!key[0] || duplicate);
        if (ImGui::Button("Ajouter une entree"))
        {
            value->Entries().emplace(key.data(), std::pair{0, 0});
            key[0] = '\0';
        }
        ImGui::EndDisabled();
        if (duplicate) ImGui::TextUnformatted("Cette cle existe deja.");
        for (auto it = value->Entries().begin(); it != value->Entries().end();)
        {
            ImGui::PushID(it->first.c_str());
            ImGui::Separator();
            ImGui::TextUnformatted(it->first.c_str());
            ImGui::InputInt("Premier", &it->second.first);
            ImGui::InputInt("Second", &it->second.second);
            const bool remove = ImGui::Button("Supprimer l'entree");
            ImGui::PopID();
            if (remove) it = value->Entries().erase(it);
            else ++it;
        }
    }
    else if (auto* value = dynamic_cast<PolyMorphObject*>(object))
    {
        auto* child = value->GetObject();
        const std::string type = child ? OOT::TypeInfo::Name(*child) : "Aucun";
        if (ImGui::BeginCombo("Type contenu", type.c_str()))
        {
            for (const auto* name : sample::objects::Types)
            {
                ImGui::BeginDisabled(depth >= 16 && std::string_view(name) == "PolyMorphObject");
                if (ImGui::Selectable(name, type == name) && type != name)
                    value->SetObject(sample::objects::Create(name));
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        ImGui::TextWrapped("Changer le type remplace le contenu actuel.");
        if (depth < 32) Edit(value->GetObject(), depth + 1);
        else ImGui::TextUnformatted("Limite d'imbrication atteinte.");
    }
    else ImGui::TextUnformatted("Type non pris en charge.");
    ImGui::PopID();
}
}
void DrawObjectEditor(OOT::BaseObject* object) { Edit(object, 0); }
