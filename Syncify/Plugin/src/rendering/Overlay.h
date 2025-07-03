#pragma once

#include "settings/Globals.h"

class Overlay
{
public:
	virtual void RenderOverlay(const char* title, const char* artist, float progress, float duration) {}
public:
	ImVec2 CalcTextSize(const char* text, ImFont* font = nullptr);
};