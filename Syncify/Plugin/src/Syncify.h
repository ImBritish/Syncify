#pragma once

#include <Plugin/external/bakkes/gui/GuiBase.h>
#include <bakkesmod/plugin/bakkesmodplugin.h>
#include <bakkesmod/plugin/pluginwindow.h>
#include <bakkesmod/plugin/PluginSettingsWindow.h>

#include "version.h"

#include "spotify/SpotifyAPI.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class Syncify: public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase, public PluginWindowBase
{
public: // Overrides
	void onLoad() override;
	void onUnload() override;
	void RenderSettings() override;
	void RenderWindow() override;
	void Render() override;
public:
	void RenderCanvas(CanvasWrapper& canvas);

	void SaveData();
	void LoadData();
private:
	std::shared_ptr<SpotifyAPI> m_SpotifyApi;
private:
	bool DisplayOverlay = true, HideWhenNotPlaying = true;
};