#pragma once

#include <string>
#include <chrono>
#include <functional>

#include <httplib.h>

#include <nlohmann/json.hpp>

#include <Windows.h>
#include <shellapi.h>

using namespace httplib;

class SpotifyAPI
{
public:
	void Authenticate();
	void Disconnect();
	void FetchMediaData();
	void ForceServerClose();
	void RefreshAccessToken(const std::function<void()>& onRefreshed);
	void SetOnTokensChanged(const std::function<void()>& onTokensChanged);
	bool IsAccessTokenExpired() const;
	long long GetTokenExpiryUnix() const;
	void SetTokenExpiryUnix(long long expiryUnixSeconds);

	void SetTitle(const std::string& title) { this->Title = title; }
	void SetArtist(const std::string& artist) { this->Artist = artist; }

	void SetCustomStatusEnabled(bool state) { this->CustomStatus = state; }

	void SetClientId(const std::string& clientId);
	void SetClientSecret(const std::string& secret);
	void SetAccessToken(const std::string& accessToken);
	void SetRefreshToken(const std::string& refreshToken);

	bool IsAuthenticated() { return this->Authenticated || (!this->AccessToken.empty() && !this->IsAccessTokenExpired()); }

	std::string* GetTitle() { return &this->Title; }
	std::string* GetArtist() { return &this->Artist; }
	const std::string& GetAlbumCoverUrl() const { return this->AlbumCoverUrl; }

	bool IsPlaying() { return this->CurrentlyPlaying; }
	bool IsExplicit() { return this->Explicit; }
	bool IsLocal() { return this->Local; }

	long GetProgress() { return this->Progress; }
	long GetDuration() { return this->Duration; }
	long GetTimestamp() { return this->Timestamp; }

	bool IsSongEnded();
	bool* UseCustomStatus() { return &this->CustomStatus; }

	std::string* GetClientId() { return &this->ClientId; }
	std::string* GetClientSecret() { return &this->ClientSecret; }
	std::string* GetAccessToken() { return &this->AccessToken; }
	std::string* GetRefreshToken() { return &this->RefreshToken; }

private:
	void ExchangeCode(const std::string& code);
	void RunAuthServer();
	void FetchMediaDataInternal(bool allowRefreshRetry);
	void NotifyTokensChanged();

	std::string Encode(const std::string& url);
public:
	bool CustomStatus = false;
private:
	bool Authenticated = false;

	std::string ClientId, ClientSecret, AccessToken, RefreshToken, TokenType, RedirectURL = "http://127.0.0.1:5173/callback";

	httplib::Server m_Server{};
	std::thread m_ServerThread;
	std::atomic<bool> m_ServerRunning{ false };
	bool m_CallbackRegistered = false;

	bool CurrentlyPlaying = false, Explicit = false, Local = false;
	std::string Title = "Not Playing", Artist = "Not Playing";
	std::string AlbumCoverUrl{};
	long Progress = 0, Duration = 0, Timestamp = 0;

	std::chrono::system_clock::time_point TokenExpiry{};
	std::chrono::steady_clock::time_point m_LastRefreshAttempt{};
	std::atomic<bool> m_RefreshInFlight{ false };
	std::function<void()> m_OnTokensChanged;
};
