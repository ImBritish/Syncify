#include "pch.h"
#include "CompactOverlay.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <string>

namespace
{
	std::string FormatTimeShort(int totalSeconds)
	{
		const int clamped = std::max(0, totalSeconds);
		const int hours = clamped / 3600;
		const int minutes = (clamped % 3600) / 60;
		const int seconds = clamped % 60;

		if (hours > 0)
			return std::format("{}:{:02d}:{:02d}", hours, minutes, seconds);

		return std::format("{}:{:02d}", minutes, seconds);
	}
}

void CompactOverlay::RenderOverlay(const char* title, const char* artist, float progress, float duration, void* albumCoverTexture)
{
	const float scale = std::max(0.5f, Settings::GlobalScale);
	const float padding = Settings::Padding * scale;
	const float albumPadding = std::max(0.0f, Settings::AlbumCoverPadding * scale);
	const float timeTextOffsetY = Settings::TimeTextOffsetY * scale;
	const float barHeight = std::max(1.0f, Settings::ProgressBarHeight * scale);

	ImGui::SetWindowSize({ Settings::SizeX * scale, Settings::SizeY * scale });

	ImVec2 MinBounds = ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);
	ImVec2 MaxBounds = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImGuiStyle& style = ImGui::GetStyle();
	const ImDrawListFlags oldDrawFlags = drawList->Flags;
	const float oldCircleSegError = style.CircleSegmentMaxError;
	drawList->Flags |= ImDrawListFlags_AntiAliasedFill;
	drawList->Flags |= ImDrawListFlags_AntiAliasedLines;
	style.CircleSegmentMaxError = std::min(oldCircleSegError, 0.60f);

	ImFont* titleFont = Font::FontLarge ? Font::FontLarge : ImGui::GetFont();
	ImFont* artistFont = Font::FontRegular ? Font::FontRegular : ImGui::GetFont();
	const float titleFontSize = titleFont->FontSize * scale;
	const float artistFontSize = artistFont->FontSize * scale;

	auto toColor = [](const float color[3], int alpha)
		{
			const int r = static_cast<int>(std::clamp(color[0], 0.0f, 1.0f) * 255.0f);
			const int g = static_cast<int>(std::clamp(color[1], 0.0f, 1.0f) * 255.0f);
			const int b = static_cast<int>(std::clamp(color[2], 0.0f, 1.0f) * 255.0f);
			return IM_COL32(r, g, b, std::clamp(alpha, 0, 255));
		};

	auto calcWidth = [](ImFont* font, float size, const char* text)
		{
			return font->CalcTextSizeA(size, FLT_MAX, -1.0f, text).x;
		};

	const bool showAlbumCover = Settings::CompactShowAlbumCover && albumCoverTexture != nullptr;
	const float yPos = MaxBounds.y - barHeight - padding;
	const float overlayHeight = MaxBounds.y - MinBounds.y;

	float coverRegionSize = 0.0f;
	float coverSize = 0.0f;
	float coverMinX = 0.0f;
	float coverMinY = 0.0f;

	if (showAlbumCover)
	{
		coverRegionSize = std::max(0.0f, overlayHeight - (albumPadding * 2.0f));
		coverSize = coverRegionSize * std::clamp(Settings::AlbumCoverScale, 0.1f, 1.0f);
		const float centerOffset = (coverRegionSize - coverSize) * 0.5f;
		coverMinX = MinBounds.x + albumPadding + centerOffset;
		coverMinY = MinBounds.y + albumPadding + centerOffset;
	}

	const float textBaseX = showAlbumCover ? MinBounds.x + albumPadding + coverRegionSize + padding : MinBounds.x + padding;
	const float availableWidth = std::max(0.0f, MaxBounds.x - textBaseX - padding);
	const ImVec2 titleTextPos = ImVec2(textBaseX + Settings::TitleXOffset * scale, MinBounds.y + Settings::TitleYOffset * scale);
	const ImVec2 artistTextPos = ImVec2(textBaseX + Settings::AuthorXOffset * scale, MinBounds.y + Settings::AuthorYOffset * scale);

	const float titleSizeX = calcWidth(titleFont, titleFontSize, title);
	const float artistSizeX = calcWidth(artistFont, artistFontSize, artist);
	const float titleOverflow = titleSizeX - availableWidth;
	const float artistOverflow = artistSizeX - availableWidth;

	const float cornerInset = 1.0f;
	const ImVec2 panelMin = ImVec2(MinBounds.x + cornerInset, MinBounds.y + cornerInset);
	const ImVec2 panelMax = ImVec2(MaxBounds.x - cornerInset, MaxBounds.y - cornerInset);
	const float panelW = std::max(0.0f, panelMax.x - panelMin.x);
	const float panelH = std::max(0.0f, panelMax.y - panelMin.y);
	const float panelRoundLimit = std::max(0.0f, (std::min(panelW, panelH) * 0.5f) - 1.0f);
	const float panelRounding = std::clamp(Settings::BackgroundRounding * scale, 0.0f, panelRoundLimit);

	drawList->AddRectFilled(
		panelMin,
		panelMax,
		toColor(Settings::BackgroundColor, Settings::BackgroundAlpha),
		panelRounding
	);

	if (showAlbumCover)
	{
		drawList->AddImageRounded(
			static_cast<ImTextureID>(albumCoverTexture),
			ImVec2(coverMinX, coverMinY),
			ImVec2(coverMinX + coverSize, coverMinY + coverSize),
			ImVec2(0, 0),
			ImVec2(1, 1),
			IM_COL32(255, 255, 255, std::clamp(Settings::AlbumCoverAlpha, 0, 255)),
			Settings::AlbumCoverRounding * scale
		);
	}

	const ImVec2 textClipMin(textBaseX, MinBounds.y);
	const ImVec2 textClipMax(MaxBounds.x - padding, yPos - 1.0f);
	drawList->PushClipRect(textClipMin, textClipMax, true);

	auto drawScrollingText = [&](const char* text, const ImVec2& basePos, ImFont* font, float fontSize, float overflow, ImU32 color)
		{
			if (overflow <= 0.0f)
			{
				drawList->AddText(font, fontSize, basePos, color, text);
				return;
			}

			const float waitTime = std::max(0.0f, Settings::AnimationWaitTime);
			const float speed = std::max(1.0f, Settings::AnimationSpeed * scale);
			const float oneWay = overflow / speed;
			const float cycleTime = oneWay * 2.0f + waitTime * 2.0f;
			const float t = std::fmod(static_cast<float>(ImGui::GetTime()), std::max(0.01f, cycleTime));

			float offset = 0.0f;
			if (t < waitTime)
			{
				offset = 0.0f;
			}
			else if (t < waitTime + oneWay)
			{
				offset = (t - waitTime) * speed;
			}
			else if (t < waitTime + oneWay + waitTime)
			{
				offset = overflow;
			}
			else
			{
				offset = overflow - (t - (waitTime + oneWay + waitTime)) * speed;
			}

			drawList->AddText(font, fontSize, ImVec2(basePos.x - offset, basePos.y), color, text);
		};

	drawScrollingText(title, titleTextPos, titleFont, titleFontSize, titleOverflow, toColor(Settings::TitleColor, Settings::TitleAlpha));
	drawScrollingText(artist, artistTextPos, artistFont, artistFontSize, artistOverflow, toColor(Settings::ArtistColor, Settings::ArtistAlpha));
	drawList->PopClipRect();

	static float animationProgress = 0.0f;
	const float barStartX = textBaseX;
	const float barEndX = std::max(barStartX, MaxBounds.x - padding);
	const float barWidth = std::max(0.0f, barEndX - barStartX);
	const float targetProgress = std::clamp((duration > 0.0f) ? progress / duration : 0.0f, 0.0f, 1.0f);

	const float deltaTime = ImGui::GetIO().DeltaTime;
	const float progressAnimSpeed = std::max(0.01f, Settings::ProgressAnimationSpeed);
	animationProgress = ImLerp(animationProgress, targetProgress, 1.0f - std::exp(-progressAnimSpeed * deltaTime));

	drawList->AddRectFilled(
		ImVec2(barStartX, yPos),
		ImVec2(barEndX, yPos + barHeight),
		toColor(Settings::DurationBarBackgroundColor, Settings::ProgressBarBackgroundAlpha),
		Settings::DurationBarRounding * scale
	);

	drawList->AddRectFilled(
		ImVec2(barStartX, yPos),
		ImVec2(barStartX + barWidth * animationProgress, yPos + barHeight),
		toColor(Settings::DurationBarColor, Settings::ProgressBarAlpha),
		Settings::DurationBarRounding * scale
	);

	if (Settings::ShowTimeText)
	{
		const int totalSec = std::max(0, static_cast<int>(duration / 1000.0f));
		const int progressSec = std::clamp(static_cast<int>(progress / 1000.0f), 0, totalSec);
		const int remainingSec = std::max(0, totalSec - progressSec);

		std::string timeStr;
		if (Settings::OnlyShowTimeLeft)
		{
			timeStr = std::format("-{}", FormatTimeShort(remainingSec));
		}
		else
		{
			if (Settings::ShowElapsedTime)
				timeStr = FormatTimeShort(progressSec);

			if (Settings::ShowTotalDuration)
			{
				if (!timeStr.empty())
					timeStr += " / ";
				timeStr += FormatTimeShort(totalSec);
			}
		}

		if (!timeStr.empty())
		{
			const float timeWidth = calcWidth(artistFont, artistFontSize, timeStr.c_str());
			const ImVec2 textPos = ImVec2(barEndX - timeWidth, yPos - timeTextOffsetY);
			drawList->AddText(artistFont, artistFontSize, textPos, toColor(Settings::TimeColor, Settings::TimeAlpha), timeStr.c_str());
		}
	}

	style.CircleSegmentMaxError = oldCircleSegError;
	drawList->Flags = oldDrawFlags;
}
