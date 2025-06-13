#pragma once

#include <Plugin/external/bakkes/gui/GuiBase.h>
#include <bakkesmod/plugin/bakkesmodplugin.h>
#include <bakkesmod/plugin/pluginwindow.h>
#include <bakkesmod/plugin/PluginSettingsWindow.h>

#include "version.h"

#include "spotify/SpotifyAPI.h"
#include "enum/impl/DisplayMode.h"
#include "enum/impl/SizeMode.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

enum DisplayModeEnum
{
	Simple, Compact, Extended
};

class Syncify: public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase, public PluginWindowBase
{
public: // Overrides
	void onLoad() override;
	void onUnload() override;
	void RenderSettings() override;
	void RenderWindow() override;
	void Render() override;
private:
	bool bIsInGame() { return gameWrapper->IsInGame() || gameWrapper->IsInOnlineGame() || gameWrapper->IsInFreeplay(); }
	bool bShowControls() { return gameWrapper->IsCursorVisible() == 2; }
	bool bNotPlaying() { return *this->m_SpotifyApi->GetTitle() == "Not Playing" && *this->m_SpotifyApi->GetArtist() == "Not Playing"; }

	ImVec2 CalcTextSize(const char* text, ImFont* font = nullptr);
public:
	void RenderCanvas(CanvasWrapper& canvas);

	void SaveData();
	void LoadData();
private:
	std::shared_ptr<SpotifyAPI> m_SpotifyApi;
private:
	bool ShowOverlay = true, HideWhenNotPlaying = true;
	DisplayMode CurrentDisplayMode = DisplayMode::Compact;
	SizeMode CurrentSizeMode = SizeMode::Dynamic;
private:
	ImFont* FontLarge{};
	ImFont* FontRegular{};
private:
	float g_AnimSpeed = 40.0f, g_AnimWaitTime = 3.0f, g_Padding = 5.0f;
private:
	float ProgressBarColor[3] = { 0.f, 0.78f, 0.f };
	float BackgroundRounding = 0.f, ProgressBarRounding = 0.f;
};