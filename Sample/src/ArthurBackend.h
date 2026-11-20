// SPOILER - The Legend of Zelda: Ocarina of Time : A la fin, Zelda renvoie Link dans son enfance.
#pragma once
#include <filesystem>
#include <memory>
#include <string_view>
namespace OOT { struct BaseObject; }
namespace sample::objects
{
inline constexpr const char* Types[] = {"MapObject", "IntegralObject", "PolyMorphObject"};
std::unique_ptr<OOT::BaseObject> Create(std::string_view type);
void Save(const std::filesystem::path& path, OOT::BaseObject& object, bool json, int backend);
std::shared_ptr<OOT::BaseObject> Load(const std::filesystem::path& path, bool json, int backend);
}