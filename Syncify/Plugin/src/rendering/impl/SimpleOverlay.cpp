#include "pch.h"
#include "SimpleOverlay.h"

void SimpleOverlay::RenderOverlay(const char* title, const char* artist, float progress, float duration)
{
	float titleSizeX = ImGui::CalcTextSize(std::string("Now Playing: " + std::string(title)).c_str()).x;
	float artistSizeX = ImGui::CalcTextSize(std::string("Artist: " + std::string(artist)).c_str()).x;

	float finalSize = std::max(titleSizeX, artistSizeX) + 20;
	finalSize = std::max(finalSize, 175.f);

	ImGui::SetWindowSize({ finalSize, 50 });

	ImGui::Text("Now Playing: %s", title);
	ImGui::Text("Artist: %s", artist);
}