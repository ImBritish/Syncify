#include "pch.h"
#include "Syncify.h"

#include <fstream>

BAKKESMOD_PLUGIN(Syncify, "Syncify", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void Syncify::onLoad()
{
	_globalCvarManager = cvarManager;

	this->m_SpotifyApi = std::make_shared<SpotifyAPI>();

	this->LoadData();

	this->m_SpotifyApi->RefreshAccessToken(nullptr);

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
		ImGui::InputText("Client Id", this->m_SpotifyApi->GetClientId());
		ImGui::InputText("Client Secret", this->m_SpotifyApi->GetClientSecret(), ImGuiInputTextFlags_Password);

		if (ImGui::Button("Authenticate"))
		{
			this->m_SpotifyApi->Authenticate();
			this->SaveData();
		}
	}
	else
	{
		if (ImGui::Checkbox("Show Overlay", &DisplayOverlay))
		{
			gameWrapper->Execute([this](GameWrapper* gw)
				{
					cvarManager->executeCommand("togglemenu " + GetMenuName());
				}
			);
		}

		ImGui::Checkbox("Hide When 'Not Playing'", &this->HideWhenNotPlaying);
	}
}

void Syncify::Render()
{
	if (!ImGui::Begin(menuTitle_.c_str(), &isWindowOpen_, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
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

void Syncify::RenderWindow() ///TODO: Make a clean UI
{
	if (!DisplayOverlay || !isWindowOpen_ || !gameWrapper)
		return;

	if (!this->m_SpotifyApi || !this->m_SpotifyApi->IsAuthenticated())
		return;

	bool ShowControls = gameWrapper->IsCursorVisible() == 2;

	std::string Title = *this->m_SpotifyApi->GetTitle(), Artist = *this->m_SpotifyApi->GetArtist();

	float titleSizeX = ImGui::CalcTextSize(std::string(("Now Playing: ") + Title).c_str()).x;
	float artistSizeX = ImGui::CalcTextSize(std::string(("Artist: ") + Artist).c_str()).x;

	float trueSizeX = (titleSizeX >= artistSizeX ? titleSizeX : artistSizeX) + 20;

	if (trueSizeX < 175)
		trueSizeX = 175;

	ImGui::SetWindowSize({ trueSizeX, (float)(ShowControls ? 70 : 50) });

	ImGui::Text("Now Playing: %s", Title.c_str());
	ImGui::Text("Artist: %s", Artist.c_str());

	/*if (ShowControls)
	{
		ImGui::SetNextWindowSize({ 400, 100 });

		if (ImGui::Button("Previous"))
		{
			this->m_SpotifyApi->FetchMediaData();
		}

		ImGui::SameLine();

		if (ImGui::Button(this->m_SpotifyApi->IsPlaying() ? "Pause" : "Play"))
		{
			this->m_SpotifyApi->FetchMediaData();
		}

		ImGui::SameLine();

		if (ImGui::Button("Next"))
		{
			this->m_SpotifyApi->FetchMediaData();
		}
	}*/
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

	if (gameWrapper->IsInGame())
	{

		bool NotPlaying = *this->m_SpotifyApi->GetTitle() == "Not Playing" && *this->m_SpotifyApi->GetArtist() == "Not Playing";

		if (isWindowOpen_)
		{
			if (this->HideWhenNotPlaying)
			{
				if (NotPlaying)
				{
					cvarManager->executeCommand("togglemenu " + GetMenuName());
				}
			}
		}
		else
		{
			if (DisplayOverlay)
			{
				if (NotPlaying && this->HideWhenNotPlaying)
				{
					return;
				}
				else 
				{
					cvarManager->executeCommand("togglemenu " + GetMenuName());
				}
			}
		}

		//if (isWindowOpen_ && *this->m_SpotifyApi->GetTitle() == "Not Playing" && *this->m_SpotifyApi->GetArtist() == "Not Playing" && this->HideWhenNotPlaying)
		//{
		//	cvarManager->executeCommand("togglemenu " + GetMenuName());
		//}

		//if (!isWindowOpen_ && DisplayOverlay && gameWrapper->IsInGame())
		//	cvarManager->executeCommand("togglemenu " + GetMenuName());
	}
	else
	{
		if (isWindowOpen_)
			cvarManager->executeCommand("togglemenu " + GetMenuName());
	}
}

void Syncify::SaveData()
{
	std::filesystem::path latestSavePath = gameWrapper->GetDataFolder() / "Syncify" / "LatestSave.json";

	if (!std::filesystem::exists(latestSavePath.parent_path()))
	{
		std::filesystem::create_directories(latestSavePath.parent_path());
	}

	nlohmann::json saveData;

	saveData["ClientId"] = *this->m_SpotifyApi->GetClientId();
	saveData["ClientSecret"] = *this->m_SpotifyApi->GetClientSecret();
	saveData["AccessToken"] = *this->m_SpotifyApi->GetAccessToken();
	saveData["RefreshToken"] = *this->m_SpotifyApi->GetRefreshToken();

	nlohmann::json options = saveData["Options"];

	options["ShowOverlay"] = this->DisplayOverlay;
	options["HideWhenNotPlaying"] = this->HideWhenNotPlaying;

	std::ofstream outFile(latestSavePath);

	if (outFile.is_open())
	{
		outFile << saveData.dump(4);
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
			this->DisplayOverlay = options["ShowOverlay"];

		if (options.contains("HideWhenNotPlaying"))
			this->HideWhenNotPlaying = options["HideWhenNotPlaying"];
	}

	inFile.close();
	Log::Info("Loaded Latest Save.");
}