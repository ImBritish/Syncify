#pragma once
#include "../Overlay.h"

class CompactOverlay : public Overlay
{
public:
	void RenderOverlay(const char* title, const char* artist, float progress, float duration) override;
};
