#pragma once
#include "Game/Game.h"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Core/EventSystem.hpp"

class App
{
public:
	App();
	~App();
	void Startup();
	void Shutdown();
	void RunFrame();

	void RunMainLoop();
	bool IsQuitting() const { return m_isQuitting; }
	static bool HandleQuitRequested(EventArgs& args);

	void CreateNewGameMode(GameMode gameMode);

	GameMode GetCurrentGameMode() const;
	GameMode GetNextGameMode() const;
	GameMode GetPreviousGameMode() const;

public:
	Camera m_screenCamera;
	Clock* m_gameClock = nullptr;
	
private:
	void BeginFrame();
	void Update();
	void Render() const;
	void EndFrame();

	void DevConsoleInterface();
	void SubscribeToEvents();

private:
	GameMode m_currentGameMode = GAME_MODE_3D;
	Game* m_game = nullptr;
	bool  m_isQuitting = false;
};