#pragma once

#include <string>

#include <httplib.h>

#include "../json/json.hpp"

#include <Windows.h>
#include <shellapi.h>

using namespace httplib;

struct SongMeta 
{

};

class SpotifyAPI
{
public:
	void Authenticate();
	void GetCurrentPlaying();
	void ForceServerClose();

	void SetClientId(const std::string& clientId);
	void SetClientSecret(const std::string& secret);
public:
	bool IsAuthenticated() { return this->Authenticated; }

	std::string* GetClientId() { return &this->ClientId; }
	std::string* GetClientSecret() { return &this->ClientSecret; }
	std::string* GetAccessToken() { return &this->AccessToken; }
private:
	void ExchangeCode(const std::string& code);
	void RunAuthServer(std::string& code);

	std::string Encode(const std::string& url);
private:
	bool Authenticated = false;
private:
	std::string ClientId = "", ClientSecret = "", AccessToken = "", RefreshToken = "", TokenType = "", RedirectURL = "http://127.0.0.1:8080/callback";
private:
	Server m_Server;
	std::thread m_ServerThread;
	std::atomic<bool> m_ServerRunning{ false };

	std::mutex m_Mutex;
	std::condition_variable m_CondVar;
	bool m_CodeReceived = false;
};