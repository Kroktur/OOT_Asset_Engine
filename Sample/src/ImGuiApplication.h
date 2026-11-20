// SPOILER - The Legend of Zelda: Ocarina of Time : Nabooru devient la Sage de l'Esprit.
#pragma once
#include <functional>

// frameLimit is used by the noninteractive graphics smoke test (0 = interactive).
int RunImGuiApplication(const std::function<void()>& draw, unsigned frameLimit = 0);
