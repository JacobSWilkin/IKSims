#include "Game/App.h"
#include "Game/Game.h"
#include "Game/Game2D.hpp"
#include "Game/Game3D.hpp"
#include "Game/CCDIKTest.hpp"
#include "Game/FABRIKTest.hpp"
#include "Game/RoboticArm.hpp"
#include "Game/AnimalMode.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Window/Window.hpp"
#include "Engine/Input/InputSystem.h"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Vertex_PCU.h"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DebugRender.hpp"

RandomNumberGenerator* g_rng = nullptr; // Created and owned by the App
App* g_theApp = nullptr;				// Created and owned by Main_Windows.cpp
Game* g_theGame = nullptr;
Renderer* g_theRenderer = nullptr;		// Created and owned by the App
AudioSystem* g_theAudio = nullptr;		// Created and owned by the App
Window* g_theWindow = nullptr;			// Created and owned by the App


App::App()
{
}

App::~App()
{
}

void App::Startup()
{
	// Create all Engine Subsystems
	EventSystemConfig eventSystemConfig;
	g_theEventSystem = new EventSystem(eventSystemConfig);

	InputSystemConfig inputConfig;
	g_theInput = new InputSystem(inputConfig);

	WindowConfig windowConfig;
	windowConfig.m_aspectRatio = 2.f;
	windowConfig.m_inputSystem = g_theInput;
	windowConfig.m_windowTitle = "LittleSims";
	windowConfig.m_isWindowFullScreen = true;
	g_theWindow = new Window(windowConfig);

	RendererConfig rendererConfig;
	rendererConfig.m_window = g_theWindow;
	g_theRenderer = new Renderer(rendererConfig);

	DevConsoleConfig devConsoleConfig;
	devConsoleConfig.m_renderer = g_theRenderer;
	devConsoleConfig.m_fontName = "Data/Fonts/SquirrelFixedFont";
	Camera* devConsoleCamera = new Camera();
	devConsoleCamera->SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
	devConsoleConfig.m_camera = devConsoleCamera;
	g_theDevConsole = new DevConsole(devConsoleConfig);

	g_theEventSystem->Startup();
	g_theDevConsole->Startup();
	g_theInput->Startup();
	g_theWindow->Startup();
	g_theRenderer->Startup();

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_theRenderer;
	debugRenderConfig.m_fontName = "Data/Fonts/SquirrelFixedFont";
	DebugRenderSystemStartup(debugRenderConfig);

	// Screen Camera
	m_screenCamera.SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
	DevConsoleInterface();

	// Event subscription
	SubscribeToEvents();

	m_gameClock = new Clock(Clock::GetSystemClock());

	// Game creation
	g_theGame = new Game3D(this);
	g_theGame->StartUp();
}

void App::Shutdown()
{
	g_theGame->Shutdown();
	delete g_theGame;
	g_theGame = nullptr;

	DebugRenderSystemShutdown();

	g_theRenderer->Shutdown();
	g_theWindow->Shutdown();
	g_theInput->Shutdown();
	g_theDevConsole->Shutdown();
	g_theEventSystem->Shutdown();

	delete g_theRenderer;
	delete g_theEventSystem;
	delete g_theWindow;
	delete g_theInput;
	delete g_theDevConsole;

	g_theRenderer = nullptr;
	g_theEventSystem = nullptr;
	g_theWindow = nullptr;
	g_theInput = nullptr;
	g_theDevConsole = nullptr;
}

void App::BeginFrame()
{
	Clock::TickSystemClock();

	g_theRenderer->BeginFrame();
	g_theEventSystem->BeginFrame();
	g_theWindow->BeginFrame();
	g_theInput->BeginFrame();
	g_theDevConsole->BeginFrame();

	DebugRenderBeginFrame();
}

void App::Render() const
{
	g_theRenderer->ClearScreen(Rgba8(150, 150, 150, 255));
	g_theGame->Render();
	g_theDevConsole->Render(AABB2(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y)));
}

void App::Update()
{
	m_gameClock->GetDeltaSeconds();
	if (g_theDevConsole->GetMode() == DevConsoleMode::OPEN_FULL || GetActiveWindow() != Window::s_mainWindow->GetHwnd() || m_currentGameMode == GameMode::GAME_MODE_2D)
	{
		g_theInput->SetCursorMode(CursorMode::POINTER);
	}
	else
	{
		g_theInput->SetCursorMode(CursorMode::FPS);
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_TILDE))
	{
		g_theDevConsole->ToggleMode(DevConsoleMode::OPEN_FULL);
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_F7))
	{
		m_currentGameMode = GetNextGameMode();
		CreateNewGameMode(m_currentGameMode);
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_F6))
	{
		m_currentGameMode = GetPreviousGameMode();
		CreateNewGameMode(m_currentGameMode);
	}

	if (g_theGame != nullptr)
	{
		g_theGame->Update();
	}
}

void App::EndFrame()
{
	g_theEventSystem->EndFrame();
	g_theInput->EndFrame();
	g_theWindow->EndFrame();
	g_theRenderer->EndFrame();
	g_theDevConsole->EndFrame();

	DebugRenderEndFrame();
}

void App::DevConsoleInterface()
{
	// Write control interface into devconsole
	g_theDevConsole->AddLine(Rgba8::CYAN, "Welcome to LittleSims!");
	g_theDevConsole->AddLine(Rgba8::SEAWEED, "----------------------------------------------------------------------");
	g_theDevConsole->AddLine(Rgba8::CYAN, "MODES:");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "Game3D");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "Game2D");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "CCDIKTest");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "FABRIKTest");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "RoboticArm3D");
	g_theDevConsole->AddLine(Rgba8::LIGHTYELLOW, "AnimalMode");
	g_theDevConsole->AddLine(Rgba8::SEAWEED, "----------------------------------------------------------------------");
}

void App::SubscribeToEvents()
{
	SubscribeEventCallbackFunction("Quit", HandleQuitRequested);
}

void App::RunFrame()
{
	BeginFrame();	
	Update();		
	Render();		
	EndFrame();
}

void App::RunMainLoop()
{
	while (!IsQuitting())
	{
		RunFrame();
	}
}

bool App::HandleQuitRequested(EventArgs& args)
{
	UNUSED(args);
	g_theApp->m_isQuitting = true;
	return true;
}

void App::CreateNewGameMode(GameMode gameMode)
{
	if (g_theGame != nullptr)
	{
		g_theGame->Shutdown();
		delete g_theGame;
		g_theGame = nullptr;
	}

	m_currentGameMode = gameMode;

	// Create the new game mode
	if (gameMode == GAME_MODE_3D)
	{
		m_screenCamera.SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
		g_theGame = new Game3D(this);
	}
	else if (gameMode == GAME_MODE_2D)
	{
		g_theGame = new Game2D(this);
	}
	else if (gameMode == GAME_MODE_CCD_VISUAL)
	{
		m_screenCamera.SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
		g_theGame = new CCDIKTest(this);
	}
	else if (gameMode == GAME_MODE_FABRIK_VISUAL)
	{
		g_theGame = new FABRIKTest(this);
	}
	else if (gameMode == GAME_MODE_ROBOTIC_ARM)
	{
		g_theGame = new RoboticArmMode(this);
	}
	else if (gameMode == GAME_MODE_ANIMALS)
	{
		g_theGame = new AnimalMode(this);
	}

	g_theGame->StartUp();
}

GameMode App::GetCurrentGameMode() const
{
	return m_currentGameMode;
}

GameMode App::GetNextGameMode() const
{
	return static_cast<GameMode>((m_currentGameMode + 1) % GAME_MODE_COUNT);
}

GameMode App::GetPreviousGameMode() const
{
	return static_cast<GameMode>((m_currentGameMode - 1 + GAME_MODE_COUNT) % GAME_MODE_COUNT);
}
