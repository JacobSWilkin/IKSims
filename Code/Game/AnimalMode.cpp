#include "Game/AnimalMode.hpp"
#include "Game/App.h"
#include "Game/Terrain.hpp"
#include "Game/Snake.hpp"
#include "Game/SnakeFollow.hpp"
#include "Game/SpiderIK.hpp"
#include "Game/Octopus.hpp"
#include "Game/OctopusIK.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Core/VertexUtils.h"
#include "Engine/Animation/Animation.hpp"
#include "Engine/Math/SmoothNoise.hpp"
#include "Engine/Input/InputSystem.h"

AnimalMode::AnimalMode(App* owner)
	:Game(owner)
{
}

void AnimalMode::StartUp()
{
	m_font = g_theRenderer->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");

	// Setting initial camera position
	m_cameraPos = Vec3(16.5f, 15.15f, 20.f);
	m_cameraOrientation = EulerAngles(45.f, 30.f, 0.f);

	// Initialize terrain
	m_terrain = new Terrain(Vec3::ZERO);
	m_terrain->InitializeTerrainHills();

	// Get skybox textures
	m_skyBoxFrontTexture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/SkyboxTest.png");

	InitializeAnimals();
	InitializeSkybox();
	InitializeSceneText();
}

void AnimalMode::InitializeAnimals()
{
	// Initialize the skeletons
	//m_snake             = new SnakeFollow(this, Vec3(35.f, 35.f, 0.f), Rgba8::BROWN);
	m_spider            = new SpiderIK(this, Vec3(130.f, 50.f, 0.f), Rgba8::DARKRED);
	m_snakeFollow       = new SnakeFollow(this, Vec3(50.f, 55.f, 0.f), Rgba8::SAPPHIRE);
	//m_snakeStationary   = new Snake(this, Vec3(0.f, -10.f, 0.f));
	m_spider2           = new SpiderIK(this, Vec3(50.f, 50.f, 0.f), Rgba8(15, 15, 15));
	//m_spiderStationary  = new SpiderIK(this, Vec3(0.f, -15.f, 0.f), Rgba8(15, 15, 15));
	//m_octopus           = new Octopus(this, Vec3(40.f, 45.f, 10.f));
	m_octopusStationary = new OctopusIK(this, Vec3(40.f, 15.f, 1.f), Rgba8::PURPLE);

	//m_snake->SetSpeed(2.5f);
	m_snakeFollow->SetSpeed(3.5f);
	m_spider->SetSpeed(3.f);
	m_spider2->SetSpeed(2.f);
	//m_octopus->SetSpeed(1.f);
	m_octopusStationary->SetSpeed(2.f);

	// Set stationary
	//m_octopusStationary->SetIsStationary(true);
	//m_snakeStationary->SetIsStationary(true);
	//m_spiderStationary->SetIsStationary(true);

	//m_allEntities.push_back(m_snake);
	m_allEntities.push_back(m_snakeFollow);
	//m_allEntities.push_back(m_snakeStationary);
	m_allEntities.push_back(m_spider);
	m_allEntities.push_back(m_spider2);
	//m_allEntities.push_back(m_spiderStationary);
	//m_allEntities.push_back(m_octopus);
	m_allEntities.push_back(m_octopusStationary);
}

void AnimalMode::InitializeSkybox()
{
	AddVertsForSphere3D(m_skyBoxVerts, m_cameraPos, 1000.0f);
}

void AnimalMode::InitializeSceneText()
{
	m_font->AddVertsForTextInBox2D(m_textVerts, "Animal Mode", m_gameSceneBounds, 20.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.97f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[V] Toggle animal verts", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.945f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[G] Toggle animal skeletons", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.925f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[L] Toggle IK Spider leg targets", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.905f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[B] Visualize walkable Bounds", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.885f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[O] Toggle Octopus leg targets", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.865f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[T] Hold for time slow", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.845f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[P] Pause the scene", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.825f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[1] Render all", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.805f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[2] Render Octopus only", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.785f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[3] Render Spider only", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.765f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[4] Render Snake only", m_gameSceneBounds, 15.f, Rgba8::GOLD, 1.f, Vec2(0.f, 0.745f));
}

void AnimalMode::Update()
{
	double deltaSeconds = g_theApp->m_gameClock->GetDeltaSeconds();
	double totalTime = g_theApp->m_gameClock->GetTotalSeconds();
	double frameRate = Clock::GetSystemClock().GetFrameRate();

	UpdateEntities(static_cast<float>(deltaSeconds));

	std::string timeScaleText = Stringf("Time: %0.2fs FPS: %0.2f", totalTime, GetClamped(frameRate, 0.0, 60.0));
	DebugAddScreenText(timeScaleText, m_gameSceneBounds, 15.f, Vec2(0.98f, 0.97f), 0.f);

	if (m_debugBounds)
	{
		DebugAddWorldWireAABB3(m_terrain->m_terrainBounds, 0.f, Rgba8::DARKRED, Rgba8::DARKRED);
	}

	DebugToggles();
	AdjustForPauseAndTimeDistortion(static_cast<float>(deltaSeconds));
	UpdateCameras(static_cast<float>(deltaSeconds));
}

void AnimalMode::DebugToggles()
{
	if (g_theInput->WasKeyJustPressed('G'))
	{
		m_isSkeletonBeingDrawn = !m_isSkeletonBeingDrawn;
	}
	if (g_theInput->WasKeyJustPressed('V'))
	{
		m_isAnimalVertsBeingDrawn = !m_isAnimalVertsBeingDrawn;
	}
	if (g_theInput->WasKeyJustPressed('B'))
	{
		m_debugBounds = !m_debugBounds;
	}
	if (g_theInput->WasKeyJustPressed('1'))
	{
		m_renderFilter = AnimalRenderFilter::ALL;
	}
	if (g_theInput->WasKeyJustPressed('2'))
	{
		m_renderFilter = AnimalRenderFilter::OCTOPUS;
	}
	if (g_theInput->WasKeyJustPressed('3'))
	{
		m_renderFilter = AnimalRenderFilter::SPIDER;
	}
	if (g_theInput->WasKeyJustPressed('4'))
	{
		m_renderFilter = AnimalRenderFilter::SNAKE;
	}
}

void AnimalMode::Render() const
{
	g_theRenderer->BeginCamera(m_gameWorldCamera);
	g_theRenderer->ClearScreen(Rgba8::SAPPHIRE);
	RenderSkyBox();
	m_terrain->Render();
	RenderEntities();
	g_theRenderer->EndCamera(m_gameWorldCamera);

	g_theRenderer->BeginCamera(g_theApp->m_screenCamera);
	GameModeAndControlsText();
	g_theRenderer->EndCamera(g_theApp->m_screenCamera);

	DebugRenderWorld(m_gameWorldCamera);
	DebugRenderScreen(g_theApp->m_screenCamera);
}

void AnimalMode::Shutdown()
{
	DeleteTerrain();
	DeleteEntities();
}

void AnimalMode::DeleteTerrain()
{
	delete m_terrain;
	m_terrain = nullptr;
}

void AnimalMode::DeleteEntities()
{
	for (int entityIndex = 0; entityIndex < static_cast<int>(m_allEntities.size()); ++entityIndex)
	{
		Entity*& entity = m_allEntities[entityIndex];
		if (entity != nullptr)
		{
			delete entity;
			entity = nullptr;
		}
	}
}

void AnimalMode::UpdateCameras(float deltaSeconds)
{
	// Game Camera
	Mat44 cameraToRender(Vec3::ZAXE, -Vec3::XAXE, Vec3::YAXE, Vec3::ZERO);
	m_gameWorldCamera.SetCameraToRenderTransform(cameraToRender);

	FreeFlyControls(deltaSeconds);
	m_cameraOrientation.m_pitchDegrees = GetClamped(m_cameraOrientation.m_pitchDegrees, -85.f, 85.f);
	m_cameraOrientation.m_rollDegrees = GetClamped(m_cameraOrientation.m_rollDegrees, -45.f, 45.f);

	m_gameWorldCamera.SetPositionAndOrientation(m_cameraPos, m_cameraOrientation);
	m_gameWorldCamera.SetPerspectiveView(2.f, 60.f, 0.1f, 1500.f);
}

void AnimalMode::UpdateEntities(float deltaSeconds)
{
	for (int entityIndex = 0; entityIndex < static_cast<int>(m_allEntities.size()); ++entityIndex)
	{
		Entity*& entity = m_allEntities[entityIndex];
		if (entity)
		{
			entity->Update(deltaSeconds);
		}
	}
}

void AnimalMode::RenderEntities() const
{
	for (int entityIndex = 0; entityIndex < static_cast<int>(m_allEntities.size()); ++entityIndex)
	{
		Entity const* entity = m_allEntities[entityIndex];
		if (!entity)
		{
			continue;
		}

		if (m_renderFilter != AnimalRenderFilter::ALL)
		{
			if (m_renderFilter == AnimalRenderFilter::OCTOPUS && entity->m_type != AnimalType::OCTOPUS)
			{
				continue;
			}

			if (m_renderFilter == AnimalRenderFilter::SPIDER && entity->m_type != AnimalType::SPIDER)
			{
				continue;
			}

			if (m_renderFilter == AnimalRenderFilter::SNAKE && entity->m_type != AnimalType::SNAKE)
			{
				continue;
			}
		}

		entity->Render();
	}
}

void AnimalMode::GameModeAndControlsText() const
{
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->BindTexture(&m_font->GetTexture());
	g_theRenderer->DrawVertexArray(m_textVerts);
}

void AnimalMode::RenderSkyBox() const
{
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetModelConstants();
	g_theRenderer->BindTexture(m_skyBoxFrontTexture);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(m_skyBoxVerts);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
}
