#define CPPHTTPLIB_OPENSSL_SUPPORT

#include "pch.h"

#include "SpotifyAPI.h"

void SpotifyAPI::Authenticate()
{
	std::thread([this]() 
		{
			std::string auth_url = std::format(
				"https://accounts.spotify.com/authorize?response_type=code&client_id={}&redirect_uri={}&scope={}",
				this->ClientId,
				this->RedirectURL,
				this->Encode("user-read-playback-state user-modify-playback-state user-read-currently-playing")
			);

			ShellExecuteA(NULL, "open", auth_url.c_str(), NULL, NULL, SW_SHOWNORMAL);

			std::string code;
			this->RunAuthServer(code);
		}
	).detach();
}

void SpotifyAPI::GetCurrentPlaying()
{
	if (this->AccessToken.empty()) 
		return;

	CurlRequest request;

	request.url = "https://api.spotify.com/v1/me/player/currently-playing";

	request.headers = {
		{ "Authorization", std::format("Bearer {}", this->AccessToken) }
	};

	HttpWrapper::SendCurlRequest(request, [this](int code, std::string result)
		{
			try
			{
				nlohmann::json json = nlohmann::json::parse(result);

				Log::Info(json.dump(4));
			}
			catch (const std::exception& e)
			{
				Log::Error(e.what());
			}
		}
	);
}

void SpotifyAPI::SetClientId(const std::string& clientId)
{
	this->ClientId = clientId;
}

void SpotifyAPI::SetClientSecret(const std::string& secret)
{
	this->ClientSecret = secret;
}

void SpotifyAPI::RunAuthServer(std::string& code)
{
	if (this->m_ServerRunning)
		return;

	this->m_ServerRunning = true;

	this->m_Server.Get("/callback", [&](const Request& req, Response& res)
		{
			if (req.has_param("code"))
			{
				code = req.get_param_value("code");

				Log::Info("Received Code: {}", code);

				res.set_content("Spotify Authorization Complete! You can close this page.", "text/html");

				this->Authenticated = true;

				if (!code.empty())
				{
					this->ExchangeCode(code);
				}
				else
				{
					Log::Error("No authorization code received.");
				}
			}
			else
			{
				res.set_content("Authorization failed.", "text/html");
			}

			std::thread([this]() 
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					this->ForceServerClose();

					this->GetCurrentPlaying();
				}
			).detach();
		}
	);

	this->m_ServerThread = std::thread([this]()
		{
			Log::Info("Waiting for Spotify...");
			this->m_Server.listen("127.0.0.1", 8080);
		}
	);

	std::unique_lock<std::mutex> lock(m_Mutex);
	m_CondVar.wait(lock, [this]() 
		{
			return this->m_CodeReceived;
		}
	);
}

void SpotifyAPI::ForceServerClose()
{
	Log::Info("Shutting down Auth Server");

	this->m_ServerRunning = false;
	this->m_Server.stop();

	if (this->m_ServerThread.joinable())
		this->m_ServerThread.join();
}

std::string SpotifyAPI::Encode(const std::string& url)
{
	std::ostringstream escaped;
	escaped.fill('0');
	escaped << std::hex;

	for (unsigned char c : url) {
		if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			escaped << c;
		}
		else if (c == ' ') {
			escaped << "%20";
		}
		else {
			escaped << '%' << std::uppercase << std::setw(2) << int(c);
		}
	}

	return escaped.str();
}

void SpotifyAPI::ExchangeCode(const std::string& code)
{
	CurlRequest request;
	request.url = "https://accounts.spotify.com/api/token";

	request.body = std::format(
		"grant_type=authorization_code&code={}&redirect_uri={}&client_id={}&client_secret={}",
		code, this->RedirectURL, this->ClientId, this->ClientSecret
	);

	request.headers = {
		{ "Content-Type", "application/x-www-form-urlencoded" }
	};

	HttpWrapper::SendCurlRequest(request, [this](int code, std::string result)
		{
			try
			{
				if (code != 200)
				{
					Log::Error("SpotifyAPI::ExchangeCode: Request failed with status {}: {}", code, result);
					return;
				}

				auto json = nlohmann::json::parse(result);

				if (!json.contains("access_token"))
				{
					Log::Error("SpotifyAPI::ExchangeCode: No access_token in response: {}", result);
					return;
				}

				this->AccessToken = json["access_token"];
				this->RefreshToken = json["refresh_token"];
				this->TokenType = json.value("token_type", "Bearer");

				Log::Info("Access token obtained.");
			}
			catch (const std::exception& e)
			{
				Log::Error("SpotifyAPI::ExchangeCode: Failed to parse response: {}", result);
			}
		}
	);
}