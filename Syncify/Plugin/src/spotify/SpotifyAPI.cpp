#include "pch.h"

#include "SpotifyAPI.h"

#include <functional>

#define CPPHTTPLIB_OPENSSL_SUPPORT

void SpotifyAPI::Authenticate()
{
	std::string auth_url = std::format(
		"https://accounts.spotify.com/authorize?response_type=code&client_id={}&redirect_uri={}&scope={}",
		this->ClientId,
		this->Encode(this->RedirectURL),
		this->Encode("user-read-playback-state user-modify-playback-state user-read-currently-playing")
	);

	ShellExecuteA(nullptr, "open", auth_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

	this->RunAuthServer();
}

void SpotifyAPI::Disconnect()
{
	this->ForceServerClose();

	this->Authenticated = false;
	this->AccessToken.clear();
	this->RefreshToken.clear();
	this->TokenType.clear();
	this->TokenExpiry = {};

	this->Title = "Not Playing";
	this->Artist = "Not Playing";
	this->CurrentlyPlaying = false;
	this->Progress = 0;
	this->Duration = 0;
	this->Timestamp = 0;
}

void SpotifyAPI::FetchMediaData()
{
	this->FetchMediaDataInternal(true);
}

void SpotifyAPI::FetchMediaDataInternal(bool allowRefreshRetry)
{
	if (this->AccessToken.empty() || this->IsAccessTokenExpired())
	{
		if (this->RefreshToken.empty())
		{
			this->Title = "Auth Required";
			this->Artist = "Missing refresh token";
			this->Authenticated = false;
			return;
		}

		Log::Info("SpotifyAPI: Access token expired or missing, refreshing...");
		this->RefreshAccessToken([this]()
			{
				this->FetchMediaDataInternal(false);
			});
		return;
	}

	CurlRequest request;

	request.url = "https://api.spotify.com/v1/me/player/currently-playing";

	request.headers = {
		{ "Authorization", std::string("Bearer " + AccessToken) }
	};

	HttpWrapper::SendCurlRequest(request, [this, allowRefreshRetry](int code, const std::string& result)
		{
			switch (code)
			{
			case 429: {
				this->Title = "Rate Limited";
				this->Artist = "Code: 429";
				return;
			}
			case 403: {
				if (allowRefreshRetry && !this->RefreshToken.empty())
				{
					Log::Warning("SpotifyAPI: Received 403, refreshing token and retrying once...");
					this->RefreshAccessToken([this]()
						{
							this->FetchMediaDataInternal(false);
						});
					return;
				}

				this->Title = "Bad OAuth request";
				this->Artist = "Code: 403";
				return;
			}
			case 401: {
				if (allowRefreshRetry && !this->RefreshToken.empty())
				{
					Log::Warning("SpotifyAPI: Received 401, refreshing token and retrying once...");
					this->RefreshAccessToken([this]()
						{
							this->FetchMediaDataInternal(false);
						});
					return;
				}

				this->Title = "Bad OAuth request";
				this->Artist = "Code: 401";
				return;
			}
			case 204: {
				this->Title = "Not Playing";
				this->Artist = "Not Playing";

				this->Progress = 0;
				this->Duration = 0;

				this->CurrentlyPlaying = false;
				return;
			}
			default:
				break;
			}

			try
			{
				nlohmann::json json = nlohmann::json::parse(result);

				this->CurrentlyPlaying = json["is_playing"];
				this->Progress = json["progress_ms"];
				this->Timestamp = json["timestamp"];

				nlohmann::json item = json["item"];

				this->Explicit = item["explicit"];
				this->Local = item["is_local"];

				this->Duration = item["duration_ms"];

				this->Title = item["name"];

				this->Artist = "";

				nlohmann::json artists = item["artists"];

				std::ostringstream oss;

				for (int i = 0; i < artists.size(); i++)
				{
					std::string artistName = artists[i]["name"];

					oss << artistName;

					if (i != artists.size() - 1)
					{
						oss << ", ";
					}
				}

				this->Artist = oss.str();
			}
			catch (const std::exception& e)
			{
				Log::Info("Code: {}", code);
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

void SpotifyAPI::SetAccessToken(const std::string& accessToken)
{
	if (this->AccessToken == accessToken)
		return;

	this->AccessToken = accessToken;
	this->NotifyTokensChanged();
}

void SpotifyAPI::SetRefreshToken(const std::string& refreshToken)
{
	if (this->RefreshToken == refreshToken)
		return;

	this->RefreshToken = refreshToken;
	this->NotifyTokensChanged();
}

void SpotifyAPI::SetOnTokensChanged(const std::function<void()>& onTokensChanged)
{
	this->m_OnTokensChanged = onTokensChanged;
}

void SpotifyAPI::RunAuthServer()
{
	if (this->m_ServerRunning)
		return;

	this->m_ServerRunning = true;

	if (!this->m_CallbackRegistered)
	{
		this->m_Server.Get("/callback", [&](const Request& req, Response& res)
			{
				if (req.has_param("code"))
				{
					const std::string code = req.get_param_value("code");

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
						this->FetchMediaData();
					}
				).detach();
			}
		);

		this->m_CallbackRegistered = true;
	}

	this->m_ServerThread = std::thread([this]()
		{
			Log::Info("Waiting for Spotify...");
			this->m_Server.listen("127.0.0.1", 5173);
		}
	);
}

bool SpotifyAPI::IsAccessTokenExpired() const
{
	return this->TokenExpiry.time_since_epoch().count() <= 0 || std::chrono::system_clock::now() >= this->TokenExpiry;
}

long long SpotifyAPI::GetTokenExpiryUnix() const
{
	if (this->TokenExpiry.time_since_epoch().count() <= 0)
		return 0;

	return std::chrono::duration_cast<std::chrono::seconds>(this->TokenExpiry.time_since_epoch()).count();
}

void SpotifyAPI::SetTokenExpiryUnix(long long expiryUnixSeconds)
{
	if (expiryUnixSeconds <= 0)
	{
		this->TokenExpiry = {};
		return;
	}

	this->TokenExpiry = std::chrono::system_clock::time_point(std::chrono::seconds(expiryUnixSeconds));
}

void SpotifyAPI::NotifyTokensChanged()
{
	if (this->m_OnTokensChanged)
		this->m_OnTokensChanged();
}

void SpotifyAPI::ForceServerClose()
{
	if (!this->m_ServerRunning)
		return;

	Log::Warning("Shutting down Auth Server");

	this->m_ServerRunning = false;
	this->m_Server.stop();

	if (this->m_ServerThread.joinable())
		this->m_ServerThread.join();
}

void SpotifyAPI::RefreshAccessToken(const std::function<void()>& onRefreshed)
{
	if (this->RefreshToken.empty())
		return;

	if (this->m_RefreshInFlight)
	{
		//Log::Info("SpotifyAPI::RefreshAccessToken: Refresh already in progress, skipping.");
		return;
	}

	constexpr auto kRefreshCooldown = std::chrono::seconds(30);
	const auto now = std::chrono::steady_clock::now();

	if (this->m_LastRefreshAttempt.time_since_epoch().count() > 0)
	{
		const auto elapsed = now - this->m_LastRefreshAttempt;
		if (elapsed < kRefreshCooldown)
		{
			const auto waitSeconds = std::chrono::duration_cast<std::chrono::seconds>(kRefreshCooldown - elapsed).count();
			//Log::Info("SpotifyAPI::RefreshAccessToken: Cooldown active ({}s remaining), skipping.", waitSeconds);
			return;
		}
	}

	this->m_LastRefreshAttempt = now;
	this->m_RefreshInFlight = true;

	CurlRequest request;
	request.url = "https://accounts.spotify.com/api/token";

	request.body = std::format(
		"grant_type=refresh_token&refresh_token={}&client_id={}&client_secret={}",
		this->RefreshToken, this->ClientId, this->ClientSecret
	);

	request.headers = {
		{ "Content-Type", "application/x-www-form-urlencoded" }
	};

	HttpWrapper::SendCurlRequest(request, [this, onRefreshed](int code, const std::string& result)
		{
			auto finish = [this]()
				{
					this->m_RefreshInFlight = false;
				};

			try
			{
				if (code != 200)
				{
					Log::Error("SpotifyAPI::RefreshAccessToken: Request failed with status {}: {}", code, result);
					this->Authenticated = false;
					finish();
					return;
				}

				nlohmann::json json = nlohmann::json::parse(result);

				if (!json.contains("access_token"))
				{
					Log::Error("SpotifyAPI::RefreshAccessToken: No access_token in response: {}", result);
					this->Authenticated = false;
					finish();
					return;
				}

				this->SetAccessToken(json["access_token"]);

				if (json.contains("refresh_token"))
					this->SetRefreshToken(json["refresh_token"]);

				int expiresIn = json["expires_in"];

				this->TokenExpiry = std::chrono::system_clock::now() + std::chrono::seconds(expiresIn);

				Log::Info("Successfully refreshed access token.");

				this->Authenticated = true;

				finish();

				if (onRefreshed)
					onRefreshed();
			}
			catch (const std::exception& e)
			{
				this->Authenticated = false;
				Log::Error("SpotifyAPI::RefreshAccessToken: Failed to parse response: {}", e.what());
				finish();
			}
		});
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
			escaped << '%' << std::uppercase << std::setw(2) << static_cast<int>(c);
		}
	}

	return escaped.str();
}

bool SpotifyAPI::IsSongEnded()
{
	long timeLeft = this->Duration - this->Progress;

	return timeLeft <= 0;
}

void SpotifyAPI::ExchangeCode(const std::string& code)
{
	CurlRequest request;
	request.url = "https://accounts.spotify.com/api/token";

	request.body = std::format(
		"grant_type=authorization_code&code={}&redirect_uri={}&client_id={}&client_secret={}",
		code, this->Encode(this->RedirectURL), this->ClientId, this->ClientSecret
	);

	request.headers = {
		{ "Content-Type", "application/x-www-form-urlencoded" }
	};

	HttpWrapper::SendCurlRequest(request, [this](int code, const std::string& result)
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

				this->SetAccessToken(json["access_token"]);
				this->SetRefreshToken(json["refresh_token"]);
				this->TokenType = json.value("token_type", "Bearer");

				int expiresIn = json["expires_in"];

				this->TokenExpiry = std::chrono::system_clock::now() + std::chrono::seconds(expiresIn);
			}
			catch (const std::exception& e)
			{
				Log::Error("SpotifyAPI::ExchangeCode: Failed to parse response: {}", result);
			}
		}
	);
}
