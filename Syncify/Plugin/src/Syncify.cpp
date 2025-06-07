#include "pch.h"
#include "Syncify.h"

BAKKESMOD_PLUGIN(Syncify, "Syncify", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void Syncify::onLoad()
{
	_globalCvarManager = cvarManager;

	cvarManager->registerCvar("syncify_client_id", "19b91574a66144269ad94e606a31a588", "ClientId for Spotify App", false, false, 0.f, false, 0.f, true);
	cvarManager->registerCvar("syncify_client_secret", "82fde3b427ca46f083735ff6b0fe4048", "Client Secret for Spotify App", false, false, 0.f, false, 0.f, true);

	this->m_SpotifyApi = std::make_shared<SpotifyAPI>();

	this->m_SpotifyApi->SetClientId(cvarManager->getCvar("syncify_client_id").getStringValue());
	this->m_SpotifyApi->SetClientSecret(cvarManager->getCvar("syncify_client_secret").getStringValue());

	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) 
		{
			this->RenderCanvas(canvas);
		}
	);
}

void Syncify::onUnload()
{
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
			this->cvarManager->getCvar("syncify_client_id").setValue(*this->m_SpotifyApi->GetClientId());
			this->cvarManager->getCvar("syncify_client_secret").setValue(*this->m_SpotifyApi->GetClientSecret());

			this->m_SpotifyApi->Authenticate();
		}
	}
}

void Syncify::RenderWindow()
{

}

void Syncify::RenderCanvas(CanvasWrapper& canvas)
{

}