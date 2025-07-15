#include "pch.h"
#include "CompactOverlay.h"

void CompactOverlay::RenderOverlay(const char* title, const char* artist, float progress, float duration)
{
    // Cache window position and size calculations
    const auto windowPos = ImGui::GetWindowPos();
    const auto windowSize = ImGui::GetWindowSize();
    const auto MinBounds = windowPos;
    const ImVec2 MaxBounds = {windowPos.x + windowSize.x, windowPos.y + windowSize.y};

    // Set window size once
    ImGui::SetWindowSize({Settings::SizeX, Settings::SizeY});

    // Calculate text sizes once
    const auto titleSizeX = Font::FontLarge ? this->CalcTextSize(title, Font::FontLarge).x : 0.0f;
    const auto artistSizeX = Font::FontRegular ? this->CalcTextSize(artist, Font::FontRegular).x : 0.0f;

    // Get draw list once
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Calculate common positions and values
    constexpr auto horizontalPadding = 8.0f;
    const auto availableWidth = MaxBounds.x - MinBounds.x - 2 * horizontalPadding;

    // Adjusted text positions
    const auto titleYPos = MinBounds.y + 10.0f;
    const auto artistYPos = titleYPos + 24.0f;
    const ImVec2 titleTextPos = {MinBounds.x + horizontalPadding, titleYPos};
    const ImVec2 artistTextPos = {MinBounds.x + horizontalPadding, artistYPos};

    // Draw background with subtle shadow
    drawList->AddRectFilled(
        MinBounds, MaxBounds,
        ImColor(35, 35, 35, Settings::Opacity),
        Settings::BackgroundRounding
    );

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

    // Progress bar settings
    constexpr float barHeight = 4.0f;
    constexpr float barVerticalPadding = 12.0f;
    constexpr float barRounding = 2.0f;
    const float yPos = MaxBounds.y - barHeight - barVerticalPadding;
    const float barStartX = MinBounds.x + horizontalPadding;
    const float barEndX = MaxBounds.x - horizontalPadding;

    // Format time string first to calculate its size
    const int totalSec = static_cast<int>(duration / 1000.0f);
    const int progressSec = static_cast<int>(progress / 1000.0f);
    const std::string timeStr = std::format("{:01d}:{:02d} / {:01d}:{:02d}", progressSec / 60, progressSec % 60, totalSec / 60, totalSec % 60);

    // Calculate time text size and adjust bar width
    const float timeTextWidth = this->CalcTextSize(timeStr.c_str()).x;
    constexpr float timeTextPadding = 8.0f; // Increased padding for better spacing
    const float adjustedBarWidth = barEndX - barStartX - timeTextWidth - timeTextPadding;

    // Draw progress bar background (using adjusted width)
    drawList->AddRectFilled(
        {barStartX, yPos},
        {barStartX + adjustedBarWidth, yPos + barHeight},
        ImColor(45, 45, 45, Settings::Opacity - 30),
        barRounding
    );

    // Animate progress
    static float animationProgress = 0.0f;
    const float targetProgress = duration > 0.0f ? std::clamp(progress / duration, 0.0f, 1.0f) : 0.0f;
    const float deltaTime = ImGui::GetIO().DeltaTime;
    constexpr float animationSpeed = 10.0f;
    animationProgress = ImLerp(animationProgress, targetProgress, 1.0f - std::exp(-animationSpeed * deltaTime));

    // Draw progress bar fill (using adjusted width)
    ImU32 progressColor = ImColor(
        static_cast<int>(Settings::DurationBarColor[0] * 255),
        static_cast<int>(Settings::DurationBarColor[1] * 255),
        static_cast<int>(Settings::DurationBarColor[2] * 255),
        std::clamp(Settings::Opacity - 50, 0, 180)
    );
    drawList->AddRectFilled(
        {barStartX, yPos},
        {barStartX + adjustedBarWidth * animationProgress, yPos + barHeight},
        progressColor,
        barRounding
    );

    // Draw time text right-aligned
    const float textYOffset = (barHeight - ImGui::GetFontSize()) * 0.5f;
    const ImVec2 textPos = {
        barStartX + adjustedBarWidth + timeTextPadding,
        yPos + textYOffset
    };
    drawList->AddText(
        textPos,
        IM_COL32(200, 200, 200, std::clamp(Settings::Opacity - 30, 0, 255)),
        timeStr.c_str()
    );

    // Draw subtle border
    drawList->AddRect(
        MinBounds, MaxBounds,
        ImColor(60, 60, 60, Settings::Opacity / 2),
        Settings::BackgroundRounding,
        0, 1.0f
    );
}