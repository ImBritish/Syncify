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

	this->m_SpotifyApi->RefreshAccessToken(nullptr);

	auto gui = gameWrapper->GetGUIManager();

	auto [codeLarge, fontLarge] = gui.LoadFont("FontLarge", "../../font.ttf", 18);
	auto [codeRegular, fontRegular] = gui.LoadFont("FontRegular", "../../font.ttf", 14);

	if (codeLarge == 2)
		this->FontLarge = fontLarge;

	if (codeRegular == 2)
		this->FontRegular = fontRegular;

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
		ImGui::BeginChild("##xx Authentication", ImVec2(300, 110), true);
		{
			float titleWidth = (300 / 2) - (ImGui::CalcTextSize("Authentication").x / 2);

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
		}
		ImGui::EndChild();

		return;
	}

	ImGui::BeginChild("##xx Settings", ImVec2(450, 220), true);
	{
		float titleWidth = (250 / 2) - (ImGui::CalcTextSize("Settings").x / 2);

		ImGui::SetCursorPosX(titleWidth);
		ImGui::Text("Settings");

		ImGui::Separator();

		if (ImGui::Checkbox("Show Overlay", &ShowOverlay))
		{
			gameWrapper->Execute([this](GameWrapper* gw)
				{
					if (ShowOverlay)
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

		ImGui::Checkbox("Hide When 'Not Playing'", &this->HideWhenNotPlaying);

		ImGui::Separator();

		float modeTitleWidth = (250 / 2) - (ImGui::CalcTextSize("Style").x / 2);

		ImGui::SetCursorPosX(titleWidth);
		ImGui::Text("Style");

		ImGui::Separator();

		if (ImGui::BeginCombo("Type", this->CurrentDisplayMode.name().c_str()))
		{
			for (const DisplayMode* displayMode : this->CurrentDisplayMode.values())
			{
				if (displayMode->ordinal() == DisplayModeEnum::Extended)
					continue;

				bool Selected = displayMode->ordinal() == this->CurrentDisplayMode.ordinal();

				if (ImGui::Selectable(displayMode->name().c_str(), Selected))
				{
					this->CurrentDisplayMode = *displayMode;
				}
			}

			ImGui::EndCombo();
		}

		if (this->CurrentDisplayMode.ordinal() != DisplayModeEnum::Simple)
		{
			ImGui::Separator();

			ImGui::SliderFloat("Background Rounding", &this->BackgroundRounding, 0.f, 14.f, "%.1f");

			ImGui::Separator();

			ImGui::ColorEdit3("ProgressBar Color", this->ProgressBarColor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs);
			ImGui::SliderFloat("ProgressBar Rounding", &this->ProgressBarRounding, 0.f, 10.f, "%.1f");
		}
	}
	ImGui::EndChild();
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

	if (this->CurrentDisplayMode != DisplayMode::Simple)
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
	if (!ShowOverlay || !isWindowOpen_ || !gameWrapper)
		return;

	if (!this->m_SpotifyApi)
		return;

	bool ShowControls = gameWrapper->IsCursorVisible() == 2;

	if (!this->FontLarge)
	{
		auto gui = gameWrapper->GetGUIManager();
		this->FontLarge = gui.GetFont("FontLarge");
	}

	if (!this->FontRegular)
	{
		auto gui = gameWrapper->GetGUIManager();
		this->FontRegular = gui.GetFont("FontRegular");
	}

	std::string Title = *this->m_SpotifyApi->GetTitle(), Artist = *this->m_SpotifyApi->GetArtist();

	switch (this->CurrentDisplayMode.ordinal())
	{
		case DisplayModeEnum::Simple:
		{
			float titleSizeX = ImGui::CalcTextSize(std::string(("Now Playing: ") + Title).c_str()).x;
			float artistSizeX = ImGui::CalcTextSize(std::string(("Artist: ") + Artist).c_str()).x;

			float finalSize = std::max((float) titleSizeX, (float) artistSizeX) + 20;
			finalSize = std::max((float) finalSize, 175.f);

			ImGui::SetWindowSize({ finalSize, (float)(ShowControls ? 70 : 50) });

			ImGui::Text("Now Playing: %s", Title.c_str());
			ImGui::Text("Artist: %s", Artist.c_str());
			break;
		}
		case DisplayModeEnum::Compact:
		{
			ImVec2 MinBounds = ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);
			ImVec2 MaxBounds = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y);

			float titleSizeX = this->CalcTextSize(Title.c_str(), this->FontLarge).x;

			float artistSizeX = this->CalcTextSize(Artist.c_str(), this->FontRegular).x;

			float finalSize = std::min(225.f, std::max(std::max((float) titleSizeX, (float) artistSizeX) + 15, 175.f));

			ImGui::SetWindowSize({ finalSize, 70 });

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			float availableWidth = MaxBounds.x - MinBounds.x - 10.0f;
			ImVec2 titleTextPos = ImVec2(MinBounds.x + 5.0f, MinBounds.y + 8.0f);
			ImVec2 artistTextPos = ImVec2(MinBounds.x + 5.0f, MinBounds.y + 30.0f);
			float titleOverflow = titleSizeX - availableWidth;
			float artistOverflow = artistSizeX - availableWidth;

			ImGui::GetBackgroundDrawList()->AddRectFilled(
				MinBounds, MaxBounds, ImColor(35, 35, 35), this->BackgroundRounding
			);

			if (this->FontLarge != nullptr)
				ImGui::PushFont(this->FontLarge);

			if (titleOverflow <= 0.0f)
			{
				drawList->AddText(titleTextPos, IM_COL32(255, 255, 255, 255), Title.c_str());
			}
			else
			{
				float t = ImGui::GetTime();

				float scrollDistance = titleOverflow * 2.0f;

				float cycleTime = scrollDistance / g_AnimSpeed + 2.0f * g_AnimWaitTime;

				float cyclePos = fmod(t, cycleTime);

				float offset = 0.0f;
				if (cyclePos < g_AnimWaitTime)
				{
					offset = 0.0f;
				}
				else if (cyclePos < g_AnimWaitTime + (titleOverflow / g_AnimSpeed))
				{
					float scrollT = cyclePos - g_AnimWaitTime;
					offset = scrollT * g_AnimSpeed;
				}
				else if (cyclePos < g_AnimWaitTime + (titleOverflow / g_AnimSpeed) + g_AnimWaitTime)
				{
					offset = titleOverflow;
				}
				else
				{
					float scrollT = cyclePos - g_AnimWaitTime - (titleOverflow / g_AnimSpeed) - g_AnimWaitTime;
					offset = titleOverflow - scrollT * g_AnimSpeed;
				}

				ImVec2 scrollPos = ImVec2(titleTextPos.x - offset, titleTextPos.y);
				drawList->AddText(scrollPos, IM_COL32(255, 255, 255, 255), Title.c_str());
			}

			if (this->FontLarge != nullptr)
				ImGui::PopFont();

			if (this->FontRegular != nullptr)
				ImGui::PushFont(this->FontRegular);

			if (artistOverflow <= 0) {
				drawList->AddText(
					ImVec2(MinBounds.x + 5, MinBounds.y + 30), ImColor(255, 255, 255), Artist.c_str()
				);
			}
			else {
				float tA = ImGui::GetTime();

				float scrollDistanceA = artistOverflow * 2.0f;

				float cycleTimeA = scrollDistanceA / g_AnimSpeed + 2.0f * g_AnimWaitTime;

				float cyclePosA = fmod(tA, cycleTimeA);

				float offsetA = 0.0f;
				if (cyclePosA < g_AnimWaitTime)
				{
					offsetA = 0.0f;
				}
				else if (cyclePosA < g_AnimWaitTime + (artistOverflow / g_AnimSpeed))
				{
					float scrollT = cyclePosA - g_AnimWaitTime;
					offsetA = scrollT * g_AnimSpeed;
				}
				else if (cyclePosA < g_AnimWaitTime + (artistOverflow / g_AnimSpeed) + g_AnimWaitTime)
				{
					offsetA = artistOverflow;
				}
				else
				{
					float scrollA = cyclePosA - g_AnimWaitTime - (artistOverflow / g_AnimSpeed) - g_AnimWaitTime;
					offsetA = artistOverflow - scrollA * g_AnimSpeed;
				}

				ImVec2 scrollPosA = ImVec2(artistTextPos.x - offsetA, artistTextPos.y);
				drawList->AddText(scrollPosA, IM_COL32(255, 255, 255, 255), Artist.c_str());
			}

			if (this->FontRegular != nullptr)
				ImGui::PopFont();

			drawList->AddRectFilled(
				ImVec2(MinBounds.x + 5, MaxBounds.y - 10), ImVec2(MaxBounds.x - 5, MaxBounds.y - 5), ImColor(55, 55, 55), this->ProgressBarRounding
			);

			ImVec2 min = MinBounds;
			ImVec2 max = MaxBounds;

			static float animationProgress = 0.0f;

			float barHeight = 5.0f;
			float yPos = max.y - barHeight - g_Padding;

			float barStartX = min.x + g_Padding;
			float barEndX = max.x - g_Padding;

			float barWidth = barEndX - barStartX;

			float progress = this->m_SpotifyApi->GetProgress();
			float duration = this->m_SpotifyApi->GetDuration();
			float targetProgress = (duration > 0.0f) ? progress / duration : 0.0f;

			float progressFraction = (duration > 0.0f) ? progress / duration : 0.0f;
			progressFraction = std::clamp(progressFraction, 0.0f, 1.0f);

			float deltaTime = ImGui::GetIO().DeltaTime;
			float animationSpeed = 10.0f;

			animationProgress = ImLerp(animationProgress, targetProgress, 1.0f - std::exp(-animationSpeed * deltaTime));

			drawList->AddRectFilled(
				ImVec2(barStartX, yPos),
				ImVec2(barStartX + barWidth * animationProgress, yPos + barHeight),
				ImColor(ProgressBarColor[0], ProgressBarColor[1], ProgressBarColor[2]), this->ProgressBarRounding
			);

			int totalSec = static_cast<int>(duration / 1000.0f);
			int progressSec = static_cast<int>(progress / 1000.0f);

			std::string timeStr = std::format("{:01d}:{:02d} / {:01d}:{:02d}",
				progressSec / 60, progressSec % 60,
				totalSec / 60, totalSec % 60
			);

			ImVec2 textPos = ImVec2(barEndX - this->CalcTextSize(timeStr.c_str()).x, yPos - 16);

			drawList->AddText(textPos, IM_COL32(255, 255, 255, 180), timeStr.c_str());
			break;
		}
		case DisplayModeEnum::Extended:
		{
			break;
		}
	}
}

ImVec2 Syncify::CalcTextSize(const char* text, ImFont* font)
{
	if (font == nullptr)
		return ImGui::CalcTextSize(text);

	const float font_size = font->FontSize;

	ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, -1.0F, text, NULL, NULL);

	text_size.x = IM_FLOOR(text_size.x + 0.95f);

	return text_size;
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

	// Currently In A Game (Should Render If DisplayOverlay is Enabled)

	if (!this->isWindowOpen_ && this->ShowOverlay)
	{
		if (this->bNotPlaying() && this->HideWhenNotPlaying)
		{
			return;
		}

		cvarManager->executeCommand("openmenu " + GetMenuName());
		return;
	}

	if (this->isWindowOpen_ && this->HideWhenNotPlaying && this->bNotPlaying())
	{
		cvarManager->executeCommand("closemenu " + GetMenuName());
		return;
	}

	if (this->isWindowOpen_ && !this->ShowOverlay)
	{
		cvarManager->executeCommand("closemenu " + GetMenuName());
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

	j["Options"]["ShowOverlay"] = this->ShowOverlay;
	j["Options"]["HideWhenNotPlaying"] = this->HideWhenNotPlaying;

	j["Style"]["DisplayMode"] = this->CurrentDisplayMode.ordinal();
	j["Style"]["SizeMode"] = this->CurrentSizeMode.ordinal();

	j["Style"]["Rounding"] = this->BackgroundRounding;

	j["Style"]["ProgressBar"]["Color"]["R"] = this->ProgressBarColor[0];
	j["Style"]["ProgressBar"]["Color"]["G"] = this->ProgressBarColor[1];
	j["Style"]["ProgressBar"]["Color"]["B"] = this->ProgressBarColor[2];

	j["Style"]["ProgressBar"]["Rounding"] = this->ProgressBarRounding;

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
			this->ShowOverlay = options["ShowOverlay"];

		if (options.contains("HideWhenNotPlaying"))
			this->HideWhenNotPlaying = options["HideWhenNotPlaying"];
	} 

	if (data.contains("Style"))
	{
		nlohmann::json style = data["Style"];

		if (style.contains("Rounding"))
			this->BackgroundRounding = style["Rounding"];

		if (style.contains("DisplayMode"))
		{
			for (const DisplayMode* mode : DisplayMode::values())
			{
				if (mode->ordinal() == (int) style["DisplayMode"])
				{
					this->CurrentDisplayMode = *mode;
					break;
				}
			}
		}

		if (style.contains("SizeMode"))
		{
			for (const SizeMode* mode : SizeMode::values())
			{
				if (mode->ordinal() == (int) style["SizeMode"])
				{
					this->CurrentSizeMode = *mode;
					break;
				}
			}
		}

		if (style.contains("ProgressBar"))
		{
			nlohmann::json progressBar = style["ProgressBar"];

			if (progressBar.contains("Rounding"))
				this->ProgressBarRounding = progressBar["Rounding"];

			if (progressBar.contains("Color"))
			{
				nlohmann::json pbColor = progressBar["Color"];

				this->ProgressBarColor[0] = pbColor["R"];
				this->ProgressBarColor[1] = pbColor["G"];
				this->ProgressBarColor[2] = pbColor["B"];
			}
		}
	}

	inFile.close();
}