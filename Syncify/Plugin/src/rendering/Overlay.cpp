#include "pch.h"
#include "Overlay.h"

ImVec2 Overlay::CalcTextSize(const char* text, ImFont* font) {
	if (font == nullptr)
		return ImGui::CalcTextSize(text);

	const float font_size = font->FontSize;

	ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, -1.0F, text, nullptr, nullptr);

	text_size.x = IM_FLOOR(text_size.x + 1.f);

	return text_size;
}