#include "pch.h"
#include "Syncify.h"

#include "bakkesmod/wrappers/GuiManagerWrapper.h"

#include <fstream>

BAKKESMOD_PLUGIN(Syncify, "Syncify", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void Syncify::onLoad()
{
	_globalCvarManager = cvarManager;

	this->m_SpotifyApi = std::make_shared<SpotifyAPI>();

	this->LoadData();

	this->OverlayInstances.emplace(std::make_pair(DisplayMode::Simple, std::make_unique<SimpleOverlay>()));
	this->OverlayInstances.emplace(std::make_pair(DisplayMode::Compact, std::make_unique<CompactOverlay>()));

	this->CurrentDisplayMode = this->OverlayInstances.at(Settings::CurrentDisplayMode).get();

	this->m_SpotifyApi->RefreshAccessToken(nullptr);

#ifdef SYNCIFY_STATUSIMPL
	status = std::make_shared<StatusImpl>(gameWrapper, m_SpotifyApi);
	status->ApplyStatus();
#endif

	auto gui = gameWrapper->GetGUIManager();

	auto [codeLarge, fontLarge] = gui.LoadFont("FontLarge", "../../font.ttf", 18);
	auto [codeRegular, fontRegular] = gui.LoadFont("FontRegular", "../../font.ttf", 14);

	if (codeLarge == 2)
		Font::FontLarge = fontLarge;

	if (codeRegular == 2)
		Font::FontRegular = fontRegular;

	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas)
		{
			this->RenderCanvas(canvas);
		}
	);
}

void Syncify::onUnload()
{
	this->SaveData();

	this->m_SpotifyApi->ForceServerClose();
}

void Syncify::RenderSettings()
{
	if (!this->m_SpotifyApi->IsAuthenticated())
	{
		float titleWidth = (ImGui::GetCurrentWindow()->Size.x / 2) - (ImGui::CalcTextSize("Authentication").x / 2);

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

	float settingsWidth = (ImGui::GetCurrentWindow()->Size.x / 2) - (ImGui::CalcTextSize("Settings").x / 2);

	ImGui::SetCursorPosX(settingsWidth);
	ImGui::Text("Settings");

	ImGui::Separator();

	if (ImGui::Checkbox("Show Overlay", &Settings::ShowOverlay))
	{
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

	ImGui::Checkbox("Hide When 'Not Playing'", &Settings::HideWhenNotPlaying);

	ImGui::Checkbox("Display Custom Status", this->m_SpotifyApi->UseCustomStatus());
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.f }, "This feature is experimental and can cause game crashes.");
		ImGui::EndTooltip();
	}

	ImGui::SliderFloat("Overlay Width", &Settings::SizeX, 175.f, 400.f);
	ImGui::SliderFloat("Overlay Height", &Settings::SizeY, 70.f, 125.f);
	ImGui::SliderInt("Overlay Opacity", &Settings::Opacity, 0, 255);

	ImGui::Separator();

	float modeTitleWidth = (ImGui::GetCurrentWindow()->Size.x / 2) - (ImGui::CalcTextSize("Style").x / 2);

	ImGui::SetCursorPosX(modeTitleWidth);
	ImGui::Text("Style");

	ImGui::Separator();

	if (ImGui::BeginCombo("Type", this->GetDisplayModeName(Settings::CurrentDisplayMode)))
	{
		for (uint8_t modeIndex = 0; modeIndex < this->OverlayInstances.size(); ++modeIndex)
		{
			if (modeIndex == DisplayMode::Extended) // Don't display extended since i dosent render anything yet
				continue;

			bool Selected = modeIndex == Settings::CurrentDisplayMode;

			if (ImGui::Selectable(this->GetDisplayModeName(modeIndex), Selected))
			{
				Settings::CurrentDisplayMode = modeIndex;
				this->CurrentDisplayMode = this->OverlayInstances.at(modeIndex).get();
			}
		}

		ImGui::EndCombo();
	}

	if (Settings::CurrentDisplayMode != DisplayMode::Simple)
	{
		ImGui::Separator();

		ImGui::SliderFloat("Background Rounding", &Settings::BackgroundRounding, 0.f, 14.f, "%.1f");

		ImGui::Separator();

		ImGui::ColorEdit3("ProgressBar Color", Settings::DurationBarColor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs);
		ImGui::SliderFloat("ProgressBar Rounding", &Settings::DurationBarRounding, 0.f, 10.f, "%.1f");
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

	if (Settings::CurrentDisplayMode != DisplayMode::Simple)
	{
		WindowFlags += ImGuiWindowFlags_NoBackground;
	}

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

	bool ShowControls = gameWrapper->IsCursorVisible() == 2;

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

	if (this->CurrentDisplayMode)
		this->CurrentDisplayMode->RenderOverlay(Title.c_str(), Artist.c_str(), this->m_SpotifyApi->GetProgress(), this->m_SpotifyApi->GetDuration());
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
		return;
	}
}

void Syncify::SaveData()
{
	std::filesystem::path latestSavePath = gameWrapper->GetDataFolder() / "Syncify" / "LatestSave.json";

	if (!std::filesystem::exists(latestSavePath.parent_path()))
	{
		std::filesystem::create_directories(latestSavePath.parent_path());
	}

	nlohmann::json j;

	j["ClientId"] = *this->m_SpotifyApi->GetClientId();
	j["ClientSecret"] = *this->m_SpotifyApi->GetClientSecret();
	j["AccessToken"] = *this->m_SpotifyApi->GetAccessToken();
	j["RefreshToken"] = *this->m_SpotifyApi->GetRefreshToken();

	j["Options"]["ShowOverlay"] = Settings::ShowOverlay;
	j["Options"]["HideWhenNotPlaying"] = Settings::HideWhenNotPlaying;
	j["Options"]["ListeningOLS"] = *this->m_SpotifyApi->UseCustomStatus();

	j["Options"]["OverlayWidth"] = Settings::SizeX;
	j["Options"]["OverlayHeight"] = Settings::SizeY;
	j["Options"]["OverlayOpacity"] = Settings::Opacity;

	j["Style"]["DisplayMode"] = Settings::CurrentDisplayMode;
	//j["Style"]["SizeMode"] = Settings::CurrentSize;

	j["Style"]["Rounding"] = Settings::BackgroundRounding;

	j["Style"]["ProgressBar"]["Color"]["R"] = Settings::DurationBarColor[0];
	j["Style"]["ProgressBar"]["Color"]["G"] = Settings::DurationBarColor[1];
	j["Style"]["ProgressBar"]["Color"]["B"] = Settings::DurationBarColor[2];

	j["Style"]["ProgressBar"]["Rounding"] = Settings::DurationBarRounding;

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

	nlohmann::json saveData;

	if (!std::filesystem::exists(latestSavePath))
		return;

	std::ifstream inFile(latestSavePath);

	nlohmann::json data = nlohmann::json::parse(inFile, nullptr, true);

	if (data.is_null())
	{
		Log::Error("Failed to parse latest save file.");
		return;
	}

	if (data.contains("ClientId"))
		this->m_SpotifyApi->SetClientId(data["ClientId"]);

	if (data.contains("ClientSecret"))
		this->m_SpotifyApi->SetClientSecret(data["ClientSecret"]);

	if (data.contains("AccessToken"))
		this->m_SpotifyApi->SetAccessToken(data["AccessToken"]);

	if (data.contains("RefreshToken"))
		this->m_SpotifyApi->SetRefreshToken(data["RefreshToken"]);

	if (data.contains("Options"))
	{
		nlohmann::json options = data["Options"];

		if (options.contains("ShowOverlay"))
			Settings::ShowOverlay = options["ShowOverlay"];

		if (options.contains("HideWhenNotPlaying"))
			Settings::HideWhenNotPlaying = options["HideWhenNotPlaying"];

		if (options.contains("ListeningOLS"))
			this->m_SpotifyApi->SetCustomStatusEnabled(options["ListeningOLS"]);

		if (options.contains("OverlayWidth"))
			Settings::SizeX = options["OverlayWidth"];

		if (options.contains("OverlayHeight"))
			Settings::SizeY = options["OverlayHeight"];

		if (options.contains("OverlayOpacity"))
			Settings::Opacity = options["OverlayOpacity"];
	}

	if (data.contains("Style"))
	{
		nlohmann::json style = data["Style"];

		if (style.contains("Rounding"))
			Settings::BackgroundRounding = style["Rounding"];

		if (style.contains("DisplayMode"))
		{
			Settings::CurrentDisplayMode = style["DisplayMode"];
		}

		//if (style.contains("SizeMode"))
		//{
		//	for (const SizeMode* mode : SizeMode::values())
		//	{
		//		if (mode->ordinal() == (int)style["SizeMode"])
		//		{
		//			this->CurrentSizeMode = *mode;
		//			break;
		//		}
		//	}
		//}

		if (style.contains("ProgressBar"))
		{
			nlohmann::json progressBar = style["ProgressBar"];

			if (progressBar.contains("Rounding"))
				Settings::DurationBarRounding = progressBar["Rounding"];

			if (progressBar.contains("Color"))
			{
				nlohmann::json pbColor = progressBar["Color"];

				Settings::DurationBarColor[0] = pbColor["R"];
				Settings::DurationBarColor[1] = pbColor["G"];
				Settings::DurationBarColor[2] = pbColor["B"];
			}
		}
	}

	inFile.close();
}

const char* Syncify::GetDisplayModeName(uint8_t mode)
{
	switch (mode)
	{
	case Simple:
		return "Simple";
	case Compact:
		return "Compact";
	case Extended:
		return "Extended";
	default:
		return "Unknown";
	}
}
