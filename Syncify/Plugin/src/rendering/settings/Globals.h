#pragma once

#include <external/imgui/imgui.h>

enum DisplayMode : uint8_t
{
	Simple = 0, Compact = 1, Extended = 2
};

namespace Settings 
{
	inline float AnimationSpeed = 40.f;
	inline float AnimationWaitTime = 3.0f;

	inline float Padding = 5.0f;

	inline float DurationBarColor[3]{ 0.f, 0.78f, 0.f };

	inline float BackgroundRounding = 0.0f;
	inline float DurationBarRounding = 0.0f;

	inline bool ShowOverlay = true;
	inline bool HideWhenNotPlaying = true;

	inline bool CustomOnlineStatus = true;

	inline uint8_t CurrentDisplayMode = DisplayMode::Compact;
}

namespace Font
{
	inline ImFont* FontLarge{};
	inline ImFont* FontRegular{};
}