#include "pch.h"
#include "Syncify.h"

#include "bakkesmod/wrappers/GuiManagerWrapper.h"
#include <bakkesmod/wrappers/http/HttpWrapper.h>

#include <algorithm>
#include <fstream>
#include <filesystem>

BAKKESMOD_PLUGIN(Syncify, "Syncify", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

namespace
{
	constexpr int kConfigVersion = 2;

	template <typename T>
	bool TryRead(const nlohmann::json& node, const char* key, T& out)
	{
		if (!node.contains(key))
			return false;

		try
		{
			out = node.at(key).get<T>();
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void TryReadColor(const nlohmann::json& node, const char* key, float color[3])
	{
		if (!node.contains(key))
			return;

		const auto& colorNode = node.at(key);

		float r = color[0], g = color[1], b = color[2];

		if (TryRead(colorNode, "R", r)) color[0] = r;
		if (TryRead(colorNode, "G", g)) color[1] = g;
		if (TryRead(colorNode, "B", b)) color[2] = b;
	}

	void ClampVisualSettings()
	{
		Settings::GlobalScale = std::clamp(Settings::GlobalScale, 0.50f, 2.50f);
		Settings::SizeX = std::clamp(Settings::SizeX, 175.0f, 600.0f);
		Settings::SizeY = std::clamp(Settings::SizeY, 60.0f, 220.0f);
		Settings::Opacity = std::clamp(Settings::Opacity, 0, 255);

		Settings::Padding = std::clamp(Settings::Padding, 0.0f, 24.0f);
		Settings::AlbumCoverPadding = std::clamp(Settings::AlbumCoverPadding, 0.0f, 24.0f);
		Settings::AlbumCoverScale = std::clamp(Settings::AlbumCoverScale, 0.1f, 1.0f);
		Settings::TitleXOffset = std::clamp(Settings::TitleXOffset, -120.0f, 120.0f);
		Settings::TitleYOffset = std::clamp(Settings::TitleYOffset, -20.0f, 120.0f);
		Settings::AuthorXOffset = std::clamp(Settings::AuthorXOffset, -120.0f, 120.0f);
		Settings::AuthorYOffset = std::clamp(Settings::AuthorYOffset, -20.0f, 140.0f);
		Settings::TimeTextOffsetY = std::clamp(Settings::TimeTextOffsetY, 8.0f, 40.0f);

		Settings::BackgroundRounding = std::clamp(Settings::BackgroundRounding, 0.0f, 24.0f);
		Settings::BorderThickness = std::clamp(Settings::BorderThickness, 0.0f, 6.0f);
		Settings::AlbumCoverRounding = std::clamp(Settings::AlbumCoverRounding, 0.0f, 24.0f);
		Settings::ProgressBarHeight = std::clamp(Settings::ProgressBarHeight, 2.0f, 20.0f);
		Settings::DurationBarRounding = std::clamp(Settings::DurationBarRounding, 0.0f, 20.0f);

		Settings::AnimationSpeed = std::clamp(Settings::AnimationSpeed, 5.0f, 180.0f);
		Settings::AnimationWaitTime = std::clamp(Settings::AnimationWaitTime, 0.0f, 8.0f);
		Settings::ProgressAnimationSpeed = std::clamp(Settings::ProgressAnimationSpeed, 0.5f, 30.0f);

		Settings::BackgroundAlpha = std::clamp(Settings::BackgroundAlpha, 0, 255);
		Settings::BorderAlpha = std::clamp(Settings::BorderAlpha, 0, 255);
		Settings::AlbumCoverAlpha = std::clamp(Settings::AlbumCoverAlpha, 0, 255);
		Settings::TitleAlpha = std::clamp(Settings::TitleAlpha, 0, 255);
		Settings::ArtistAlpha = std::clamp(Settings::ArtistAlpha, 0, 255);
		Settings::TimeAlpha = std::clamp(Settings::TimeAlpha, 0, 255);
		Settings::ProgressBarAlpha = std::clamp(Settings::ProgressBarAlpha, 0, 255);
		Settings::ProgressBarBackgroundAlpha = std::clamp(Settings::ProgressBarBackgroundAlpha, 0, 255);
	}
}

void Syncify::onLoad()
{
	_globalCvarManager = cvarManager;

	this->m_SpotifyApi = std::make_shared<SpotifyAPI>();

	this->LoadData();
	this->m_SpotifyApi->SetOnTokensChanged([this]()
		{
			this->SaveData();
		});

	this->OverlayInstances.emplace(Compact, std::make_unique<CompactOverlay>());

	auto it = this->OverlayInstances.find(Settings::CurrentDisplayMode);

	if (it != this->OverlayInstances.end())
		this->CurrentDisplayMode = it->second.get();
	else
		this->CurrentDisplayMode = this->OverlayInstances.at(Compact).get();

	if (this->m_SpotifyApi->IsAccessTokenExpired())
		this->m_SpotifyApi->RefreshAccessToken(nullptr);

#ifdef SYNCIFY_STATUSIMPL
	status = std::make_shared<StatusImpl>(gameWrapper, m_SpotifyApi);
	status->ApplyStatus();
#endif

	auto gui = gameWrapper->GetGUIManager();

	if (auto [codeLarge, fontLarge] = gui.LoadFont("FontLarge", "../../font.ttf", 18, nullptr, Font::Ranges); codeLarge == 2)
		Font::FontLarge = fontLarge;
	else
		Log::Error("Failed to load FontLarge");

	if (auto [codeRegular, fontRegular] = gui.LoadFont("FontRegular", "../../font.ttf", 14, nullptr, Font::Ranges); codeRegular == 2)
		Font::FontRegular = fontRegular;
	else
		Log::Error("Failed to load FontRegular");

	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas)
		{
			this->RenderCanvas(canvas);
		}
	);
}

void Syncify::onUnload()
{
	if (this->m_SpotifyApi)
		this->m_SpotifyApi->SetOnTokensChanged(nullptr);

	this->m_AlbumCoverImage.reset();
	this->m_AlbumCoverUrl.clear();
	this->m_AlbumCoverDownloadInFlight = false;
	this->m_HasPendingAlbumCover = false;

	this->SaveData();

	this->m_SpotifyApi->ForceServerClose();
}

void Syncify::RenderSettings()
{
	auto sectionHeader = [](const char* title)
		{
			ImGui::Spacing();
			ImGui::Text("%s", title);
			ImGui::Separator();
		};

	auto checkboxWithTooltip = [](const char* label, bool* value, const char* tooltip)
		{
			const bool changed = ImGui::Checkbox(label, value);
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("%s", tooltip);
				ImGui::EndTooltip();
			}
			return changed;
		};

	auto colorEditWithAlpha = [](const char* label, float color[3], int* alpha)
		{
			float rgba[4] = {
				std::clamp(color[0], 0.0f, 1.0f),
				std::clamp(color[1], 0.0f, 1.0f),
				std::clamp(color[2], 0.0f, 1.0f),
				std::clamp(static_cast<float>(*alpha) / 255.0f, 0.0f, 1.0f)
			};

			if (!ImGui::ColorEdit4(label, rgba, ImGuiColorEditFlags_NoInputs))
				return false;

			color[0] = std::clamp(rgba[0], 0.0f, 1.0f);
			color[1] = std::clamp(rgba[1], 0.0f, 1.0f);
			color[2] = std::clamp(rgba[2], 0.0f, 1.0f);
			*alpha = std::clamp(static_cast<int>(rgba[3] * 255.0f + 0.5f), 0, 255);
			return true;
		};

	auto alphaOnlyEdit4 = [](const char* label, int* alpha)
		{
			float rgba[4] = {
				1.0f,
				1.0f,
				1.0f,
				std::clamp(static_cast<float>(*alpha) / 255.0f, 0.0f, 1.0f)
			};

			if (!ImGui::ColorEdit4(label, rgba, ImGuiColorEditFlags_NoInputs))
				return false;

			*alpha = std::clamp(static_cast<int>(rgba[3] * 255.0f + 0.5f), 0, 255);
			return true;
		};

	if (!this->m_SpotifyApi->IsAuthenticated())
	{
		float titleWidth = ImGui::GetCurrentWindow()->Size.x / 2 - ImGui::CalcTextSize("Authentication").x / 2;

		ImGui::SetCursorPosX(titleWidth);
		ImGui::Text("Authentication");

		ImGui::Separator();

		ImGui::InputText("Client Id", this->m_SpotifyApi->GetClientId());
		ImGui::InputText("Client Secret", this->m_SpotifyApi->GetClientSecret(), ImGuiInputTextFlags_Password);

		if (ImGui::Button("Authenticate"))
		{
			this->m_SpotifyApi->Authenticate();
			this->SaveData();
		}

		return;
	}
	bool changed = false;

	sectionHeader("Behavior");
	if (checkboxWithTooltip("Show Overlay", &Settings::ShowOverlay, "Toggles the in-game overlay window on or off."))
	{
		changed = true;
		gameWrapper->Execute([this](GameWrapper* gw)
			{
				if (Settings::ShowOverlay)
				{
					cvarManager->executeCommand("openmenu " + GetMenuName());
				}
				else
				{
					cvarManager->executeCommand("closemenu " + GetMenuName());
				}
			}
		);
	}
	changed |= checkboxWithTooltip("Hide When Not Playing", &Settings::HideWhenNotPlaying, "Automatically hides the overlay while Spotify is not actively playing a track.");
	changed |= checkboxWithTooltip("Display Custom Status", this->m_SpotifyApi->UseCustomStatus(), "Updates your online status with your current track.");

	sectionHeader("Global Scale");
	changed |= ImGui::SliderFloat("Scale##global_scale", &Settings::GlobalScale, 0.50f, 2.50f, "%.2f");

	sectionHeader("Background");
	changed |= ImGui::SliderFloat("Width", &Settings::SizeX, 175.f, 600.f, "%.0f");
	changed |= ImGui::SliderFloat("Height", &Settings::SizeY, 60.f, 220.f, "%.0f");
	changed |= ImGui::SliderFloat("Background Rounding", &Settings::BackgroundRounding, 0.f, 24.f, "%.1f");
	changed |= colorEditWithAlpha("Background Color", Settings::BackgroundColor, &Settings::BackgroundAlpha);

	sectionHeader("Album Cover");
	changed |= checkboxWithTooltip("Show Album Cover", &Settings::CompactShowAlbumCover, "Renders the track album cover on the left side.");
	changed |= ImGui::SliderFloat("Padding", &Settings::AlbumCoverPadding, 0.f, 24.f, "%.1f");
	changed |= ImGui::SliderFloat("Scale##album_scale", &Settings::AlbumCoverScale, 0.10f, 1.00f, "%.2f");
	changed |= ImGui::SliderFloat("Rounding", &Settings::AlbumCoverRounding, 0.f, 24.f, "%.1f");
	changed |= ImGui::SliderInt("Opacity##album_opacity", &Settings::AlbumCoverAlpha, 0, 255);

	sectionHeader("Title/Author");
	changed |= colorEditWithAlpha("Title Color", Settings::TitleColor, &Settings::TitleAlpha);
	changed |= colorEditWithAlpha("Author Color", Settings::ArtistColor, &Settings::ArtistAlpha);
	changed |= ImGui::SliderFloat("Title X Offset", &Settings::TitleXOffset, -120.f, 120.f, "%.1f");
	changed |= ImGui::SliderFloat("Title Y Offset", &Settings::TitleYOffset, -20.f, 120.f, "%.1f");
	changed |= ImGui::SliderFloat("Author X Offset", &Settings::AuthorXOffset, -120.f, 120.f, "%.1f");
	changed |= ImGui::SliderFloat("Author Y Offset", &Settings::AuthorYOffset, -20.f, 140.f, "%.1f");
	changed |= ImGui::SliderFloat("Text Scroll Speed", &Settings::AnimationSpeed, 5.f, 180.f, "%.0f");
	changed |= ImGui::SliderFloat("Text Wait Time", &Settings::AnimationWaitTime, 0.f, 8.f, "%.1f");

	sectionHeader("Time Text");
	changed |= checkboxWithTooltip("Show Time Text", &Settings::ShowTimeText, "Renders time text near the progress bar.");
	changed |= colorEditWithAlpha("Text Color", Settings::TimeColor, &Settings::TimeAlpha);
	changed |= ImGui::SliderFloat("Y Offset", &Settings::TimeTextOffsetY, 8.f, 40.f, "%.1f");
	changed |= checkboxWithTooltip("Show Elapsed Time", &Settings::ShowElapsedTime, "Shows the elapsed playback time.");
	changed |= checkboxWithTooltip("Show Total Duration", &Settings::ShowTotalDuration, "Shows the track total duration.");
	changed |= checkboxWithTooltip("Only Show Time Left", &Settings::OnlyShowTimeLeft, "Shows only remaining time (e.g. -1:23).");

	sectionHeader("Progress Bar");
	changed |= colorEditWithAlpha("Progress Color", Settings::DurationBarColor, &Settings::ProgressBarAlpha);
	changed |= colorEditWithAlpha("Progress BG Color", Settings::DurationBarBackgroundColor, &Settings::ProgressBarBackgroundAlpha);
	changed |= ImGui::SliderFloat("Progress Height", &Settings::ProgressBarHeight, 2.f, 20.f, "%.1f");
	changed |= ImGui::SliderFloat("Progress Rounding", &Settings::DurationBarRounding, 0.f, 20.f, "%.1f");
	changed |= ImGui::SliderFloat("Progress Smoothness", &Settings::ProgressAnimationSpeed, 0.5f, 30.f, "%.1f");

	sectionHeader("Account");
	if (ImGui::Button("Disconnect Spotify"))
	{
		this->m_SpotifyApi->Disconnect();
		this->SaveData();
		return;
	}

	ImGui::SameLine();
	ImGui::TextColored({ 0.15f, 0.15f, 0.15f, 1.0f }, "(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextColored({ 1.0f, 1.0f, 1.0f, 1.f }, "Unlink Syncify with Spotify to re-enter app info.");
		ImGui::EndTooltip();
	}

	if (changed)
	{
		ClampVisualSettings();
		this->SaveData();
	}
}

void Syncify::Render()
{
	if (this->m_SpotifyApi && !this->m_SpotifyApi->IsAuthenticated())
	{
		if (!ImGui::Begin(menuTitle_.c_str(), &isWindowOpen_, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
		{
			ImGui::End();
			return;
		}

		ImGui::SetWindowSize({ 155, 50 });

		ImGui::Text("Authentication Required");
		ImGui::Text("F2 -> Plugins -> Syncify");

		ImGui::End();
		return;
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

	WindowFlags += ImGuiWindowFlags_NoBackground;

	if (!ImGui::Begin(menuTitle_.c_str(), &isWindowOpen_, WindowFlags))
	{
		ImGui::End();
		return;
	}

	this->RenderWindow();

	ImGui::End();

	if (!isWindowOpen_)
	{
		_globalCvarManager->executeCommand("togglemenu " + GetMenuName());
	}
}

void Syncify::RenderWindow()
{
	if (!Settings::ShowOverlay || !isWindowOpen_ || !gameWrapper)
		return;

	if (!this->m_SpotifyApi)
		return;

	if (!Font::FontLarge)
	{
		auto gui = gameWrapper->GetGUIManager();
		Font::FontLarge = gui.GetFont("FontLarge");
	}

	if (!Font::FontRegular)
	{
		auto gui = gameWrapper->GetGUIManager();
		Font::FontRegular = gui.GetFont("FontRegular");
	}

	std::string Title = *this->m_SpotifyApi->GetTitle(), Artist = *this->m_SpotifyApi->GetArtist();
	this->UpdateAlbumCoverTexture();

	void* albumCoverTexture = nullptr;
	if (Settings::CompactShowAlbumCover && this->m_AlbumCoverImage && this->m_AlbumCoverImage->IsLoadedForImGui())
	{
		albumCoverTexture = this->m_AlbumCoverImage->GetImGuiTex();
	}

	if (this->CurrentDisplayMode)
		this->CurrentDisplayMode->RenderOverlay(Title.c_str(), Artist.c_str(), this->m_SpotifyApi->GetProgress(), this->m_SpotifyApi->GetDuration(), albumCoverTexture);
}

void Syncify::UpdateAlbumCoverTexture()
{
	if (!this->m_SpotifyApi || !Settings::CompactShowAlbumCover)
	{
		this->m_AlbumCoverImage.reset();
		this->m_AlbumCoverUrl.clear();
		this->m_AlbumCoverDownloadInFlight = false;
		this->m_HasPendingAlbumCover = false;
		return;
	}

	if (this->m_HasPendingAlbumCover.exchange(false))
	{
		std::wstring pendingPath;
		std::string pendingUrl;
		{
			std::lock_guard<std::mutex> lock(this->m_AlbumCoverMutex);
			pendingPath = this->m_PendingAlbumCoverPath;
			pendingUrl = this->m_PendingAlbumCoverUrl;
			this->m_PendingAlbumCoverPath.clear();
			this->m_PendingAlbumCoverUrl.clear();
		}

		if (!pendingPath.empty() && pendingUrl == this->m_AlbumCoverUrl)
		{
			this->m_AlbumCoverImage = std::make_unique<ImageWrapper>(pendingPath, false, true);
			this->m_AlbumCoverImage->LoadForImGui([](bool) {});
		}
	}

	const std::string latestUrl = this->m_SpotifyApi->GetAlbumCoverUrl();

	if (latestUrl.empty())
	{
		this->m_AlbumCoverImage.reset();
		this->m_AlbumCoverUrl.clear();
		return;
	}

	if (latestUrl != this->m_AlbumCoverUrl)
	{
		this->m_AlbumCoverUrl = latestUrl;
		this->m_AlbumCoverImage.reset();
		this->m_HasPendingAlbumCover = false;
	}
	else if (this->m_AlbumCoverImage || this->m_AlbumCoverDownloadInFlight)
	{
		return;
	}

	if (this->m_AlbumCoverDownloadInFlight.exchange(true))
		return;

	const auto cacheDir = gameWrapper->GetDataFolder() / "Syncify" / "cache";
	if (!exists(cacheDir))
		create_directories(cacheDir);

	const auto hashed = std::to_string(std::hash<std::string>{}(latestUrl));
	const auto coverPath = cacheDir / ("cover_" + hashed + ".jpg");

	if (exists(coverPath))
	{
		this->m_AlbumCoverImage = std::make_unique<ImageWrapper>(coverPath.wstring(), false, true);
		this->m_AlbumCoverImage->LoadForImGui([](bool) {});
		this->m_AlbumCoverDownloadInFlight = false;
		return;
	}

	CurlRequest request;
	request.url = latestUrl;

	HttpWrapper::SendCurlRequest(request, coverPath.wstring(), [this, latestUrl](int code, const std::wstring& outputPath)
		{
			if (code != 200)
			{
				Log::Warning("Failed to download album cover. HTTP {}", code);
				this->m_AlbumCoverDownloadInFlight = false;
				return;
			}

			{
				std::lock_guard<std::mutex> lock(this->m_AlbumCoverMutex);
				this->m_PendingAlbumCoverUrl = latestUrl;
				this->m_PendingAlbumCoverPath = outputPath;
			}
			this->m_HasPendingAlbumCover = true;
			this->m_AlbumCoverDownloadInFlight = false;
		}
	);
}

void Syncify::RenderCanvas(CanvasWrapper& canvas)
{
	if (!this->m_SpotifyApi || !this->m_SpotifyApi->IsAuthenticated())
		return;

	static auto lastTime = std::chrono::high_resolution_clock::now();
	auto now = std::chrono::high_resolution_clock::now();

	auto duration = duration_cast<std::chrono::milliseconds>(now - lastTime).count();

	if (duration >= 1000)
	{
		this->m_SpotifyApi->FetchMediaData();
		lastTime = now;
	}

	if (!this->bIsInGame())
	{
		if (this->isWindowOpen_)
			cvarManager->executeCommand("closemenu " + GetMenuName());

		return;
	}

	if (!this->isWindowOpen_ && Settings::ShowOverlay) // Overlay Is Closed But Should Be Open
	{
		if (this->bNotPlaying() && Settings::HideWhenNotPlaying) // If there is no song playing and we should hide when not playing -> Keep the menu Closed
		{
			return;
		}

		cvarManager->executeCommand("openmenu " + GetMenuName()); // Open the window and render Overlay
		return;
	}

	if (this->isWindowOpen_ && Settings::HideWhenNotPlaying && this->bNotPlaying()) // Window is open but needs closing
	{
		cvarManager->executeCommand("closemenu " + GetMenuName());
	}
}

void Syncify::SaveData()
{
	if (!this->m_SpotifyApi || !gameWrapper)
		return;

	std::filesystem::path latestSavePath = gameWrapper->GetDataFolder() / "Syncify" / "LatestSave.json";

	if (!exists(latestSavePath.parent_path()))
	{
		create_directories(latestSavePath.parent_path());
	}

	nlohmann::json j;
	j["ConfigVersion"] = kConfigVersion;

	j["Auth"]["ClientId"] = *this->m_SpotifyApi->GetClientId();
	j["Auth"]["ClientSecret"] = *this->m_SpotifyApi->GetClientSecret();
	j["Auth"]["AccessToken"] = *this->m_SpotifyApi->GetAccessToken();
	j["Auth"]["RefreshToken"] = *this->m_SpotifyApi->GetRefreshToken();
	j["Auth"]["TokenExpiryUnix"] = this->m_SpotifyApi->GetTokenExpiryUnix();

	j["Overlay"]["ShowOverlay"] = Settings::ShowOverlay;
	j["Overlay"]["HideWhenNotPlaying"] = Settings::HideWhenNotPlaying;
	j["Overlay"]["DisplayMode"] = Settings::CurrentDisplayMode;
	j["Overlay"]["OverlayWidth"] = Settings::SizeX;
	j["Overlay"]["OverlayHeight"] = Settings::SizeY;
	j["Overlay"]["OverlayOpacity"] = Settings::BackgroundAlpha; // legacy fallback
	j["Overlay"]["GlobalScale"] = Settings::GlobalScale;
	j["Overlay"]["CompactShowAlbumCover"] = Settings::CompactShowAlbumCover;
	j["Overlay"]["ShowTimeText"] = Settings::ShowTimeText;
	j["Overlay"]["ShowElapsedTime"] = Settings::ShowElapsedTime;
	j["Overlay"]["ShowTotalDuration"] = Settings::ShowTotalDuration;
	j["Overlay"]["OnlyShowTimeLeft"] = Settings::OnlyShowTimeLeft;

	j["Features"]["CustomOnlineStatus"] = *this->m_SpotifyApi->UseCustomStatus();

	j["Style"]["Background"]["Rounding"] = Settings::BackgroundRounding;
	j["Style"]["Background"]["Alpha"] = Settings::BackgroundAlpha;
	j["Style"]["Background"]["Color"]["R"] = Settings::BackgroundColor[0];
	j["Style"]["Background"]["Color"]["G"] = Settings::BackgroundColor[1];
	j["Style"]["Background"]["Color"]["B"] = Settings::BackgroundColor[2];

	j["Style"]["AlbumCover"]["Padding"] = Settings::AlbumCoverPadding;
	j["Style"]["AlbumCover"]["Scale"] = Settings::AlbumCoverScale;
	j["Style"]["AlbumCover"]["Rounding"] = Settings::AlbumCoverRounding;
	j["Style"]["AlbumCover"]["Alpha"] = Settings::AlbumCoverAlpha;

	j["Style"]["TitleAuthor"]["TitleColor"]["R"] = Settings::TitleColor[0];
	j["Style"]["TitleAuthor"]["TitleColor"]["G"] = Settings::TitleColor[1];
	j["Style"]["TitleAuthor"]["TitleColor"]["B"] = Settings::TitleColor[2];
	j["Style"]["TitleAuthor"]["AuthorColor"]["R"] = Settings::ArtistColor[0];
	j["Style"]["TitleAuthor"]["AuthorColor"]["G"] = Settings::ArtistColor[1];
	j["Style"]["TitleAuthor"]["AuthorColor"]["B"] = Settings::ArtistColor[2];
	j["Style"]["TitleAuthor"]["TitleAlpha"] = Settings::TitleAlpha;
	j["Style"]["TitleAuthor"]["AuthorAlpha"] = Settings::ArtistAlpha;
	j["Style"]["TitleAuthor"]["TitleXOffset"] = Settings::TitleXOffset;
	j["Style"]["TitleAuthor"]["TitleYOffset"] = Settings::TitleYOffset;
	j["Style"]["TitleAuthor"]["AuthorXOffset"] = Settings::AuthorXOffset;
	j["Style"]["TitleAuthor"]["AuthorYOffset"] = Settings::AuthorYOffset;
	j["Style"]["TitleAuthor"]["ScrollSpeed"] = Settings::AnimationSpeed;
	j["Style"]["TitleAuthor"]["WaitTime"] = Settings::AnimationWaitTime;

	j["Style"]["TimeText"]["Color"]["R"] = Settings::TimeColor[0];
	j["Style"]["TimeText"]["Color"]["G"] = Settings::TimeColor[1];
	j["Style"]["TimeText"]["Color"]["B"] = Settings::TimeColor[2];
	j["Style"]["TimeText"]["Alpha"] = Settings::TimeAlpha;
	j["Style"]["TimeText"]["OffsetY"] = Settings::TimeTextOffsetY;

	j["Style"]["ProgressBar"]["Color"]["R"] = Settings::DurationBarColor[0];
	j["Style"]["ProgressBar"]["Color"]["G"] = Settings::DurationBarColor[1];
	j["Style"]["ProgressBar"]["Color"]["B"] = Settings::DurationBarColor[2];
	j["Style"]["ProgressBar"]["BackgroundColor"]["R"] = Settings::DurationBarBackgroundColor[0];
	j["Style"]["ProgressBar"]["BackgroundColor"]["G"] = Settings::DurationBarBackgroundColor[1];
	j["Style"]["ProgressBar"]["BackgroundColor"]["B"] = Settings::DurationBarBackgroundColor[2];
	j["Style"]["ProgressBar"]["Height"] = Settings::ProgressBarHeight;
	j["Style"]["ProgressBar"]["Rounding"] = Settings::DurationBarRounding;
	j["Style"]["ProgressBar"]["Alpha"] = Settings::ProgressBarAlpha;
	j["Style"]["ProgressBar"]["BackgroundAlpha"] = Settings::ProgressBarBackgroundAlpha;
	j["Style"]["ProgressBar"]["Smoothness"] = Settings::ProgressAnimationSpeed;

	j["Style"]["Padding"] = Settings::Padding;

	std::ofstream outFile(latestSavePath);

	if (outFile.is_open())
	{
		outFile << j.dump(4);
		outFile.close();
		Log::Info("Saved Settings!");
	}
	else
	{
		Log::Error("Failed to open file for saving: {}", latestSavePath.string());
	}
}

void Syncify::LoadData()
{
	std::filesystem::path latestSavePath = gameWrapper->GetDataFolder() / "Syncify" / "LatestSave.json";

	if (!exists(latestSavePath))
		return;

	std::ifstream inFile(latestSavePath);
	const nlohmann::json data = nlohmann::json::parse(inFile, nullptr, false);
	if (data.is_discarded() || data.is_null())
	{
		Log::Error("Failed to parse latest save file.");
		return;
	}

	int configVersion = 1;
	TryRead(data, "ConfigVersion", configVersion);

	if (configVersion >= 2)
		this->LoadDataV2(data);
	else
		this->LoadDataLegacy(data);
}

void Syncify::LoadDataV2(const nlohmann::json& data)
{
	int legacyOpacity = Settings::Opacity;
	bool legacyOpacityRead = false;
	bool loadedFeatureAlpha = false;

	if (data.contains("Auth"))
	{
		const auto& auth = data.at("Auth");
		std::string clientId, clientSecret, accessToken, refreshToken;
		long long tokenExpiry = 0;

		if (TryRead(auth, "ClientId", clientId)) this->m_SpotifyApi->SetClientId(clientId);
		if (TryRead(auth, "ClientSecret", clientSecret)) this->m_SpotifyApi->SetClientSecret(clientSecret);
		if (TryRead(auth, "AccessToken", accessToken)) this->m_SpotifyApi->SetAccessToken(accessToken);
		if (TryRead(auth, "RefreshToken", refreshToken)) this->m_SpotifyApi->SetRefreshToken(refreshToken);
		if (TryRead(auth, "TokenExpiryUnix", tokenExpiry)) this->m_SpotifyApi->SetTokenExpiryUnix(tokenExpiry);
	}

	if (data.contains("Overlay"))
	{
		const auto& overlay = data.at("Overlay");
		TryRead(overlay, "ShowOverlay", Settings::ShowOverlay);
		TryRead(overlay, "HideWhenNotPlaying", Settings::HideWhenNotPlaying);
		TryRead(overlay, "DisplayMode", Settings::CurrentDisplayMode);
		TryRead(overlay, "OverlayWidth", Settings::SizeX);
		TryRead(overlay, "OverlayHeight", Settings::SizeY);
		if (TryRead(overlay, "OverlayOpacity", legacyOpacity))
		{
			legacyOpacityRead = true;
			Settings::Opacity = legacyOpacity;
		}
		TryRead(overlay, "GlobalScale", Settings::GlobalScale);
		TryRead(overlay, "CompactShowAlbumCover", Settings::CompactShowAlbumCover);
		TryRead(overlay, "ShowTimeText", Settings::ShowTimeText);
		TryRead(overlay, "ShowElapsedTime", Settings::ShowElapsedTime);
		TryRead(overlay, "ShowTotalDuration", Settings::ShowTotalDuration);
		TryRead(overlay, "OnlyShowTimeLeft", Settings::OnlyShowTimeLeft);
	}

	if (data.contains("Features"))
	{
		const auto& features = data.at("Features");
		bool customStatus = false;
		if (TryRead(features, "CustomOnlineStatus", customStatus))
			this->m_SpotifyApi->SetCustomStatusEnabled(customStatus);
	}

	if (data.contains("Style"))
	{
		const auto& style = data.at("Style");
		TryRead(style, "Padding", Settings::Padding);

		if (style.contains("Background"))
		{
			const auto& background = style.at("Background");
			TryRead(background, "Rounding", Settings::BackgroundRounding);
			TryReadColor(background, "Color", Settings::BackgroundColor);
			if (TryRead(background, "Alpha", Settings::BackgroundAlpha)) loadedFeatureAlpha = true;

			if (background.contains("Border"))
			{
				const auto& border = background.at("Border");
				TryRead(border, "Enabled", Settings::BorderEnabled);
				TryRead(border, "Thickness", Settings::BorderThickness);
				TryReadColor(border, "Color", Settings::BorderColor);
				if (TryRead(border, "Alpha", Settings::BorderAlpha)) loadedFeatureAlpha = true;
			}
		}
		else
		{
			TryRead(style, "BackgroundRounding", Settings::BackgroundRounding);
			TryRead(style, "BorderThickness", Settings::BorderThickness);
			TryReadColor(style, "BackgroundColor", Settings::BackgroundColor);
			TryReadColor(style, "BorderColor", Settings::BorderColor);
		}

		if (style.contains("AlbumCover"))
		{
			const auto& albumCover = style.at("AlbumCover");
			TryRead(albumCover, "Padding", Settings::AlbumCoverPadding);
			TryRead(albumCover, "Scale", Settings::AlbumCoverScale);
			TryRead(albumCover, "Rounding", Settings::AlbumCoverRounding);
			if (TryRead(albumCover, "Alpha", Settings::AlbumCoverAlpha)) loadedFeatureAlpha = true;

			float legacySize = 42.0f;
			if (TryRead(albumCover, "Size", legacySize))
			{
				Settings::AlbumCoverScale = std::clamp(legacySize / 42.0f, 0.1f, 1.0f);
			}
		}

		if (style.contains("TitleAuthor"))
		{
			const auto& text = style.at("TitleAuthor");
			TryReadColor(text, "TitleColor", Settings::TitleColor);
			TryReadColor(text, "AuthorColor", Settings::ArtistColor);
			TryRead(text, "TitleXOffset", Settings::TitleXOffset);
			TryRead(text, "TitleYOffset", Settings::TitleYOffset);
			TryRead(text, "AuthorXOffset", Settings::AuthorXOffset);
			TryRead(text, "AuthorYOffset", Settings::AuthorYOffset);
			TryRead(text, "ScrollSpeed", Settings::AnimationSpeed);
			TryRead(text, "WaitTime", Settings::AnimationWaitTime);
			if (TryRead(text, "TitleAlpha", Settings::TitleAlpha)) loadedFeatureAlpha = true;
			if (TryRead(text, "AuthorAlpha", Settings::ArtistAlpha)) loadedFeatureAlpha = true;
		}
		else
		{
			TryReadColor(style, "TitleColor", Settings::TitleColor);
			TryReadColor(style, "ArtistColor", Settings::ArtistColor);
		}

		if (style.contains("TimeText"))
		{
			const auto& timeText = style.at("TimeText");
			TryReadColor(timeText, "Color", Settings::TimeColor);
			TryRead(timeText, "OffsetY", Settings::TimeTextOffsetY);
			if (TryRead(timeText, "Alpha", Settings::TimeAlpha)) loadedFeatureAlpha = true;
		}
		else
		{
			TryReadColor(style, "TimeColor", Settings::TimeColor);
			TryRead(style, "TimeTextOffsetY", Settings::TimeTextOffsetY);
		}

		if (style.contains("Animation"))
		{
			const auto& animation = style.at("Animation");
			TryRead(animation, "Speed", Settings::AnimationSpeed);
			TryRead(animation, "WaitTime", Settings::AnimationWaitTime);
			TryRead(animation, "ProgressSmoothness", Settings::ProgressAnimationSpeed);
		}

		if (style.contains("ProgressBar"))
		{
			const auto& progressBar = style.at("ProgressBar");
			TryRead(progressBar, "Height", Settings::ProgressBarHeight);
			TryRead(progressBar, "Rounding", Settings::DurationBarRounding);
			TryRead(progressBar, "Smoothness", Settings::ProgressAnimationSpeed);
			if (TryRead(progressBar, "Alpha", Settings::ProgressBarAlpha)) loadedFeatureAlpha = true;
			if (TryRead(progressBar, "BackgroundAlpha", Settings::ProgressBarBackgroundAlpha)) loadedFeatureAlpha = true;
			if (progressBar.contains("Color"))
				TryReadColor(progressBar, "Color", Settings::DurationBarColor);
			if (progressBar.contains("BackgroundColor"))
				TryReadColor(progressBar, "BackgroundColor", Settings::DurationBarBackgroundColor);
		}
	}

	if (legacyOpacityRead && !loadedFeatureAlpha)
	{
		const int fallbackAlpha = std::clamp(legacyOpacity, 0, 255);
		Settings::BackgroundAlpha = fallbackAlpha;
		Settings::BorderAlpha = fallbackAlpha;
		Settings::AlbumCoverAlpha = fallbackAlpha;
		Settings::TitleAlpha = fallbackAlpha;
		Settings::ArtistAlpha = fallbackAlpha;
		Settings::TimeAlpha = fallbackAlpha;
		Settings::ProgressBarAlpha = fallbackAlpha;
		Settings::ProgressBarBackgroundAlpha = fallbackAlpha;
	}

	ClampVisualSettings();
}

void Syncify::LoadDataLegacy(const nlohmann::json& data)
{
	std::string clientId, clientSecret, accessToken, refreshToken;
	long long tokenExpiry = 0;
	int legacyOpacity = Settings::Opacity;
	bool legacyOpacityRead = false;

	if (TryRead(data, "ClientId", clientId)) this->m_SpotifyApi->SetClientId(clientId);
	if (TryRead(data, "ClientSecret", clientSecret)) this->m_SpotifyApi->SetClientSecret(clientSecret);
	if (TryRead(data, "AccessToken", accessToken)) this->m_SpotifyApi->SetAccessToken(accessToken);
	if (TryRead(data, "RefreshToken", refreshToken)) this->m_SpotifyApi->SetRefreshToken(refreshToken);
	if (TryRead(data, "TokenExpiry", tokenExpiry)) this->m_SpotifyApi->SetTokenExpiryUnix(tokenExpiry);

	if (data.contains("Options"))
	{
		const auto& options = data.at("Options");
		TryRead(options, "ShowOverlay", Settings::ShowOverlay);
		TryRead(options, "HideWhenNotPlaying", Settings::HideWhenNotPlaying);
		TryRead(options, "OverlayWidth", Settings::SizeX);
		TryRead(options, "OverlayHeight", Settings::SizeY);
		if (TryRead(options, "OverlayOpacity", legacyOpacity))
		{
			legacyOpacityRead = true;
			Settings::Opacity = legacyOpacity;
		}
		TryRead(options, "CompactShowAlbumCover", Settings::CompactShowAlbumCover);

		bool customStatus = false;
		if (TryRead(options, "ListeningOLS", customStatus))
			this->m_SpotifyApi->SetCustomStatusEnabled(customStatus);
	}

	if (data.contains("Style"))
	{
		const auto& style = data.at("Style");
		TryRead(style, "DisplayMode", Settings::CurrentDisplayMode);
		TryRead(style, "Rounding", Settings::BackgroundRounding);

		if (style.contains("Background"))
		{
			const auto& background = style.at("Background");
			if (background.contains("Color"))
				TryReadColor(background, "Color", Settings::BackgroundColor);
		}

		if (style.contains("ProgressBar"))
		{
			const auto& progressBar = style.at("ProgressBar");
			TryRead(progressBar, "Rounding", Settings::DurationBarRounding);
			if (progressBar.contains("Color"))
				TryReadColor(progressBar, "Color", Settings::DurationBarColor);
		}
	}

	if (legacyOpacityRead)
	{
		const int fallbackAlpha = std::clamp(legacyOpacity, 0, 255);
		Settings::BackgroundAlpha = fallbackAlpha;
		Settings::BorderAlpha = fallbackAlpha;
		Settings::AlbumCoverAlpha = fallbackAlpha;
		Settings::TitleAlpha = fallbackAlpha;
		Settings::ArtistAlpha = fallbackAlpha;
		Settings::TimeAlpha = fallbackAlpha;
		Settings::ProgressBarAlpha = fallbackAlpha;
		Settings::ProgressBarBackgroundAlpha = fallbackAlpha;
	}

	ClampVisualSettings();
}

const char* Syncify::GetDisplayModeName(uint8_t mode)
{
	switch (mode)
	{
	case Compact:
		return "Compact";
	default:
		return "Unknown";
	}
}
