#pragma once

#include <string>

#include <httplib.h>

#include <nlohmann/json.hpp>

#include <Windows.h>
#include <shellapi.h>

using namespace httplib;

class SpotifyAPI
{
public:
	void Authenticate();
	void FetchMediaData();
	void ForceServerClose();
	void RefreshAccessToken(const std::function<void()>& onRefreshed);

	void SetTitle(const std::string& title) { this->Title = title; }
	void SetArtist(const std::string& artist) { this->Artist = artist; }

	void SetCustomStatusEnabled(bool state) { this->CustomStatus = state; }

	void SetClientId(const std::string& clientId);
	void SetClientSecret(const std::string& secret);
	void SetAccessToken(const std::string& accessToken);
	void SetRefreshToken(const std::string& refreshToken);

	bool IsAuthenticated() { return this->Authenticated; }

	std::string* GetTitle() { return &this->Title; }
	std::string* GetArtist() { return &this->Artist; }

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
	void RunAuthServer(std::string& code);

	std::string Encode(const std::string& url);

public:
	bool CustomStatus = false;

private:
	bool Authenticated = false;

std::string ClientId, ClientSecret, AccessToken, RefreshToken, TokenType, RedirectURL = "http://127.0.0.1:5173/callback";

httplib::Server m_Server{};
	std::thread m_ServerThread;
	std::atomic<bool> m_ServerRunning{false};

	std::mutex m_Mutex;
	std::condition_variable m_CondVar;
	bool m_CodeReceived = false;

bool CurrentlyPlaying = false, Explicit = false, Local = false;
	std::string Title = "Not Playing", Artist = "Not Playing";
	long Progress = 0, Duration = 0, Timestamp = 0;

std::chrono::steady_clock::time_point TokenExpiry;
};