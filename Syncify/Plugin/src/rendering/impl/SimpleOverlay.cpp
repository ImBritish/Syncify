#include "pch.h"
#include "SimpleOverlay.h"

void SimpleOverlay::RenderOverlay(const char* title, const char* artist, float progress, float duration)
{
	// Cache window position and size calculations
	const auto windowPos = ImGui::GetWindowPos();
	const auto windowSize = ImGui::GetWindowSize();

	const auto MinBounds = windowPos;
	const ImVec2 MaxBounds = {windowPos.x + windowSize.x, windowPos.y + windowSize.y};

	// Set window size once
	ImGui::SetWindowSize({Settings::SizeX, Settings::SizeY - 10.f});

	// Calculate text sizes once
	const auto titleSizeX = Font::FontLarge ? this->CalcTextSize(std::string("Now Playing: " + std::string(title)).c_str(), Font::FontLarge).x : 0.0f;
	const auto artistSizeX = Font::FontRegular ? this->CalcTextSize(std::string("Artist: " + std::string(artist)).c_str(), Font::FontRegular).x : 0.0f;

	// Get draw list once
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Calculate common positions and values
	constexpr auto horizontalPadding = 8.0f; // Increased from 5.0f for better spacing
	const auto availableWidth = MaxBounds.x - MinBounds.x - 2 * horizontalPadding;

	// Adjusted text positions with better spacing
	const auto titleYPos = MinBounds.y + 10.0f; // Increased from 8.0f
	const auto artistYPos = titleYPos + 24.0f; // Increased from 30.0f (22px gap instead of 22)
	const ImVec2 titleTextPos = {MinBounds.x + horizontalPadding, titleYPos};
	const ImVec2 artistTextPos = {MinBounds.x + horizontalPadding, artistYPos};

	// Make a simple overlay with a solid background
	drawList->AddRectFilled(
		MinBounds, MaxBounds,
		ImColor(35, 35, 35, Settings::Opacity),
		Settings::BackgroundRounding
	);

	// Helper function for scrolling text
	auto renderScrollingText = [drawList](const char* text, const ImVec2& pos, float textWidth, float availableWidth, float opacity, ImFont* font)
	{
		const auto overflow = textWidth - availableWidth;

		if (overflow <= 0.0f)
		{
			if (font) ImGui::PushFont(font);
			drawList->AddText(pos, IM_COL32(255, 255, 255, opacity), text);
			if (font) ImGui::PopFont();
			return;
		}

		const auto t = ImGui::GetTime();
		const auto scrollDistance = overflow * 2.0f;
		const auto cycleTime = scrollDistance / Settings::AnimationSpeed + 2.0f * Settings::AnimationWaitTime;
		const auto cyclePos = fmod(t, cycleTime);

		float offset;
		if (cyclePos < Settings::AnimationWaitTime)
		{
			offset = 0.0f;
		}
		else if (cyclePos < Settings::AnimationWaitTime + overflow / Settings::AnimationSpeed)
		{
			offset = (cyclePos - Settings::AnimationWaitTime) * Settings::AnimationSpeed;
		}
		else if (cyclePos < Settings::AnimationWaitTime + overflow / Settings::AnimationSpeed +
			Settings::AnimationWaitTime)
		{
			offset = overflow;
		}
		else
		{
			offset = overflow - (cyclePos - Settings::AnimationWaitTime - overflow / Settings::AnimationSpeed -
				Settings::AnimationWaitTime) * Settings::AnimationSpeed;
		}

		if (font) ImGui::PushFont(font);
		drawList->AddText({pos.x - offset, pos.y}, IM_COL32(255, 255, 255, opacity), text);
		if (font) ImGui::PopFont();
	};

	// Render title with larger font and better spacing
	if (Font::FontLarge) ImGui::PushFont(Font::FontLarge);
	renderScrollingText(title, titleTextPos, titleSizeX, availableWidth, Settings::Opacity, Font::FontLarge);
	if (Font::FontLarge) ImGui::PopFont();

	// Render artist with smaller font and adjusted spacing
	if (Font::FontRegular) ImGui::PushFont(Font::FontRegular);
	renderScrollingText(artist, artistTextPos, artistSizeX, availableWidth, Settings::Opacity - 20, Font::FontRegular);
	if (Font::FontRegular) ImGui::PopFont();
}