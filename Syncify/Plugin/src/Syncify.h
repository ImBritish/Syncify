#pragma once

#include <Plugin/external/bakkes/gui/GuiBase.h>
#include <bakkesmod/plugin/bakkesmodplugin.h>
#include <bakkesmod/wrappers/ImageWrapper.h>

#include "version.h"

#include "spotify/SpotifyAPI.h"

// Status Implementation
#include "hidden/StatusImpl.h"

#include "rendering/impl/CompactOverlay.h"

#include <memory>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH);

class Syncify final : public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase, public PluginWindowBase
{
#ifdef SYNCIFY_STATUSIMPL
public:
	std::shared_ptr<StatusImpl> status;
#endif
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
public:
	void RenderCanvas(CanvasWrapper& canvas);

	void SaveData();
	void LoadData();
private:
	const char* GetDisplayModeName(uint8_t displayMode);
	void LoadDataV2(const nlohmann::json& data);
	void LoadDataLegacy(const nlohmann::json& data);
	void UpdateAlbumCoverTexture();
private:
	std::shared_ptr<SpotifyAPI> m_SpotifyApi;
	std::unordered_map<uint8_t, std::unique_ptr<Overlay>> OverlayInstances{};
	Overlay* CurrentDisplayMode{};

	std::unique_ptr<ImageWrapper> m_AlbumCoverImage;
	std::string m_AlbumCoverUrl{};
	std::wstring m_PendingAlbumCoverPath{};
	std::string m_PendingAlbumCoverUrl{};
	std::mutex m_AlbumCoverMutex;
	std::atomic<bool> m_AlbumCoverDownloadInFlight{ false };
	std::atomic<bool> m_HasPendingAlbumCover{ false };
};
