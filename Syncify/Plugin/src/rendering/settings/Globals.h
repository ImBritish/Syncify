#pragma once

#include <external/imgui/imgui.h>

enum DisplayMode : uint8_t
{
	Compact = 0
};

namespace Settings 
{
	inline float AnimationSpeed = 40.f;
	inline float AnimationWaitTime = 3.0f;
	inline float ProgressAnimationSpeed = 10.0f;

	inline float Padding = 5.0f;
	inline float AlbumCoverPadding = 4.0f;
	inline float AlbumCoverScale = 1.0f;
	inline float TitleXOffset = 0.0f;
	inline float TitleYOffset = 8.0f;
	inline float AuthorXOffset = 0.0f;
	inline float AuthorYOffset = 30.0f;
	inline float TimeTextOffsetY = 16.0f;
	inline float ProgressBarHeight = 5.0f;

	inline float BackgroundColor[3]{ 0.14f, 0.14f, 0.14f };
	inline float DurationBarColor[3]{ 0.f, 0.78f, 0.f };
	inline float DurationBarBackgroundColor[3]{ 0.22f, 0.22f, 0.22f };
	inline float TitleColor[3]{ 1.0f, 1.0f, 1.0f };
	inline float ArtistColor[3]{ 0.90f, 0.90f, 0.90f };
	inline float TimeColor[3]{ 1.0f, 1.0f, 1.0f };
	inline float BorderColor[3]{ 0.0f, 0.0f, 0.0f };

	inline float BackgroundRounding = 0.0f;
	inline float DurationBarRounding = 0.0f;
	inline float BorderThickness = 0.0f;
	inline float AlbumCoverRounding = 4.0f;

	inline int BackgroundAlpha = 255;
	inline bool BorderEnabled = true;
	inline int BorderAlpha = 255;
	inline int AlbumCoverAlpha = 255;
	inline int TitleAlpha = 255;
	inline int ArtistAlpha = 255;
	inline int TimeAlpha = 255;
	inline int ProgressBarAlpha = 255;
	inline int ProgressBarBackgroundAlpha = 255;

	inline bool ShowOverlay = true;
	inline bool HideWhenNotPlaying = true;
	inline bool CompactShowAlbumCover = false;
	inline bool ShowTimeText = true;
	inline bool ShowElapsedTime = true;
	inline bool ShowTotalDuration = true;
	inline bool OnlyShowTimeLeft = false;

	inline bool CustomOnlineStatus = true;

	inline uint8_t CurrentDisplayMode = DisplayMode::Compact;

	inline float SizeX = 225.f, SizeY = 70.f;
	inline float GlobalScale = 1.0f;
	inline int Opacity = 255; // Legacy value kept for backward compatibility.
}

namespace Font
{
	inline ImFont* FontLarge{};
	inline ImFont* FontRegular{};

    const ImWchar Ranges[] = {
        // Basic Latin + Latin-1 Supplement
        0x0020, 0x00FF,

        // Extended Latin
        0x0100, 0x017F, // Latin Extended-A
        0x0180, 0x024F, // Latin Extended-B

        // IPA Extensions
        0x0250, 0x02AF,

        // Cyrillic
        0x0400, 0x04FF, // Cyrillic
        0x0500, 0x052F, // Cyrillic Supplement
        0x2DE0, 0x2DFF, // Cyrillic Extended-A
        0xA640, 0xA69F, // Cyrillic Extended-B

        // Greek and Coptic
        0x0370, 0x03FF,

        // Arabic
        0x0600, 0x06FF,
        0x0750, 0x077F, // Arabic Supplement
        0x08A0, 0x08FF, // Arabic Extended-A
        0xFB50, 0xFDFF, // Arabic Presentation Forms-A
        0xFE70, 0xFEFF, // Arabic Presentation Forms-B

        // Hebrew
        0x0590, 0x05FF,

        // CJK
        0x3000, 0x30FF, // CJK Symbols and Punctuation + Hiragana + Katakana
        0x31F0, 0x31FF, // Katakana Phonetic Extensions
        0x3200, 0x32FF, // Enclosed CJK Letters and Months
        0x3400, 0x4DBF, // CJK Unified Ideographs Extension A
        0x4E00, 0x9FFF, // CJK Unified Ideographs
        0xF900, 0xFAFF, // CJK Compatibility Ideographs

        // Hangul
        0xAC00, 0xD7AF, // Hangul Syllables
        0x1100, 0x11FF, // Hangul Jamo
        0x3130, 0x318F, // Hangul Compatibility Jamo

        // Indic scripts
        0x0900, 0x097F, // Devanagari
        0x0980, 0x09FF, // Bengali
        0x0A00, 0x0A7F, // Gurmukhi
        0x0A80, 0x0AFF, // Gujarati
        0x0B00, 0x0B7F, // Oriya
        0x0B80, 0x0BFF, // Tamil
        0x0C00, 0x0C7F, // Telugu
        0x0C80, 0x0CFF, // Kannada
        0x0D00, 0x0D7F, // Malayalam

        // Thai
        0x0E00, 0x0E7F,

        // Vietnamese
        0x1EA0, 0x1EFF,

        // Symbols and Punctuation
        0x2000, 0x206F, // General Punctuation
        0x2070, 0x209F, // Superscripts and Subscripts
        0x20A0, 0x20CF, // Currency Symbols
        0x2100, 0x214F, // Letterlike Symbols
        0x2150, 0x218F, // Number Forms
        0x2190, 0x21FF, // Arrows
        0x2200, 0x22FF, // Mathematical Operators
        0x2300, 0x23FF, // Miscellaneous Technical
        0x2400, 0x243F, // Control Pictures
        0x2440, 0x245F, // Optical Character Recognition
        0x2460, 0x24FF, // Enclosed Alphanumerics
        0x2500, 0x257F, // Box Drawing
        0x2580, 0x259F, // Block Elements
        0x25A0, 0x25FF, // Geometric Shapes
        0x2600, 0x26FF, // Miscellaneous Symbols
        0x2700, 0x27BF, // Dingbats
        0x27C0, 0x27EF, // Miscellaneous Mathematical Symbols-A
        0x27F0, 0x27FF, // Supplemental Arrows-A
        0x2800, 0x28FF, // Braille Patterns

        0 // Terminator
    };
}
