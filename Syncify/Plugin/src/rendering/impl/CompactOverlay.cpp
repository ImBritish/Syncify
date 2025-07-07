#include "pch.h"
#include "CompactOverlay.h"

void CompactOverlay::RenderOverlay(const char* title, const char* artist, float progress, float duration)
{
	ImVec2 MinBounds = ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);
	ImVec2 MaxBounds = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y);

	float titleSizeX = this->CalcTextSize(title, Font::FontLarge).x;

	float artistSizeX = this->CalcTextSize(artist, Font::FontRegular).x;

	//float finalSize = std::min(225.f, std::max(std::max((float)titleSizeX, (float)artistSizeX) + 15, 175.f));
	float finalSize = Settings::SizeX;

	ImGui::SetWindowSize({ finalSize, Settings::SizeY });

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	float availableWidth = MaxBounds.x - MinBounds.x - 10.0f;
	ImVec2 titleTextPos = ImVec2(MinBounds.x + 5.0f, MinBounds.y + 8.0f);
	ImVec2 artistTextPos = ImVec2(MinBounds.x + 5.0f, MinBounds.y + 30.0f);
	float titleOverflow = titleSizeX - availableWidth;
	float artistOverflow = artistSizeX - availableWidth;

	ImGui::GetBackgroundDrawList()->AddRectFilled(
		MinBounds, MaxBounds, ImColor(35, 35, 35, Settings::Opacity), Settings::BackgroundRounding
	);

	if (Font::FontLarge != nullptr)
		ImGui::PushFont(Font::FontLarge);

	if (titleOverflow <= 0.0f)
	{
		drawList->AddText(titleTextPos, IM_COL32(255, 255, 255, Settings::Opacity), title);
	}
	else
	{
		float t = ImGui::GetTime();

		float scrollDistance = titleOverflow * 2.0f;

		float cycleTime = scrollDistance / Settings::AnimationSpeed + 2.0f * Settings::AnimationWaitTime;

		float cyclePos = fmod(t, cycleTime);

		float offset = 0.0f;
		if (cyclePos < Settings::AnimationWaitTime)
		{
			offset = 0.0f;
		}
		else if (cyclePos < Settings::AnimationWaitTime + (titleOverflow / Settings::AnimationSpeed))
		{
			float scrollT = cyclePos - Settings::AnimationWaitTime;
			offset = scrollT * Settings::AnimationSpeed;
		}
		else if (cyclePos < Settings::AnimationWaitTime + (titleOverflow / Settings::AnimationSpeed) + Settings::AnimationWaitTime)
		{
			offset = titleOverflow;
		}
		else
		{
			float scrollT = cyclePos - Settings::AnimationWaitTime - (titleOverflow / Settings::AnimationSpeed) - Settings::AnimationWaitTime;
			offset = titleOverflow - scrollT * Settings::AnimationSpeed;
		}

		ImVec2 scrollPos = ImVec2(titleTextPos.x - offset, titleTextPos.y);
		drawList->AddText(scrollPos, IM_COL32(255, 255, 255, Settings::Opacity), title);
	}

	if (Font::FontLarge != nullptr)
		ImGui::PopFont();

	if (Font::FontRegular != nullptr)
		ImGui::PushFont(Font::FontRegular);

	if (artistOverflow <= 0) {
		drawList->AddText(
			ImVec2(MinBounds.x + 5, MinBounds.y + 30), ImColor(255, 255, 255, Settings::Opacity), artist
		);
	}
	else {
		float tA = ImGui::GetTime();

		float scrollDistanceA = artistOverflow * 2.0f;

		float cycleTimeA = scrollDistanceA / Settings::AnimationSpeed + 2.0f * Settings::AnimationWaitTime;

		float cyclePosA = fmod(tA, cycleTimeA);

		float offsetA = 0.0f;
		if (cyclePosA < Settings::AnimationWaitTime)
		{
			offsetA = 0.0f;
		}
		else if (cyclePosA < Settings::AnimationWaitTime + (artistOverflow / Settings::AnimationSpeed))
		{
			float scrollT = cyclePosA - Settings::AnimationWaitTime;
			offsetA = scrollT * Settings::AnimationSpeed;
		}
		else if (cyclePosA < Settings::AnimationWaitTime + (artistOverflow / Settings::AnimationSpeed) + Settings::AnimationWaitTime)
		{
			offsetA = artistOverflow;
		}
		else
		{
			float scrollA = cyclePosA - Settings::AnimationWaitTime - (artistOverflow / Settings::AnimationSpeed) - Settings::AnimationWaitTime;
			offsetA = artistOverflow - scrollA * Settings::AnimationSpeed;
		}

		ImVec2 scrollPosA = ImVec2(artistTextPos.x - offsetA, artistTextPos.y);
		drawList->AddText(scrollPosA, IM_COL32(255, 255, 255, Settings::Opacity), artist);
	}

	if (Font::FontRegular != nullptr)
		ImGui::PopFont();

	drawList->AddRectFilled(
		ImVec2(MinBounds.x + 5, MaxBounds.y - 10), ImVec2(MaxBounds.x - 5, MaxBounds.y - 5), ImColor(55, 55, 55, Settings::Opacity), Settings::DurationBarRounding
	);

	ImVec2 min = MinBounds;
	ImVec2 max = MaxBounds;

	static float animationProgress = 0.0f;

	float barHeight = 5.0f;
	float yPos = max.y - barHeight - Settings::Padding;

	float barStartX = min.x + Settings::Padding;
	float barEndX = max.x - Settings::Padding;

	float barWidth = barEndX - barStartX;

	float targetProgress = (duration > 0.0f) ? progress / duration : 0.0f;

	float progressFraction = (duration > 0.0f) ? progress / duration : 0.0f;
	progressFraction = std::clamp(progressFraction, 0.0f, 1.0f);

	float deltaTime = ImGui::GetIO().DeltaTime;
	float animationSpeed = 10.0f;

	animationProgress = ImLerp(animationProgress, targetProgress, 1.0f - std::exp(-animationSpeed * deltaTime));

	drawList->AddRectFilled(
		ImVec2(barStartX, yPos),
		ImVec2(barStartX + barWidth * animationProgress, yPos + barHeight),
		ImColor(Settings::DurationBarColor[0], Settings::DurationBarColor[1], Settings::DurationBarColor[2], Settings::Opacity / 255.f), Settings::DurationBarRounding
	);

	int totalSec = static_cast<int>(duration / 1000.0f);
	int progressSec = static_cast<int>(progress / 1000.0f);

	std::string timeStr = std::format("{:01d}:{:02d} / {:01d}:{:02d}",
		progressSec / 60, progressSec % 60,
		totalSec / 60, totalSec % 60
	);

	ImVec2 textPos = ImVec2(barEndX - this->CalcTextSize(timeStr.c_str()).x, yPos - 16);

	drawList->AddText(textPos, IM_COL32(255, 255, 255, std::clamp(Settings::Opacity, 0, 180)), timeStr.c_str());
}