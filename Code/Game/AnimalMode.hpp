#pragma once
#include "Game/Game.h"
#include "Engine/Animation/AnimStateMachine.hpp"
// -----------------------------------------------------------------------------
class App;
class Entity;
class Terrain;
class Snake;
class SnakeFollow;
class SpiderIK;
class Octopus;
class OctopusIK;
// -----------------------------------------------------------------------------
enum class AnimalRenderFilter
{
	ALL,
	OCTOPUS,
	SPIDER,
	SNAKE
};
// -----------------------------------------------------------------------------
class AnimalMode : public Game 
{
public:
	AnimalMode(App* owner);
	void InitializeAnimals();
	void InitializeSkybox();
	void InitializeSceneText();

	// Overrides
	void StartUp() override;
	void Update() override;

	void DebugToggles();

	void Render() const override;
	void Shutdown() override;

	// Updating
	void UpdateCameras(float deltaSeconds);
	void UpdateEntities(float deltaSeconds);

	// Rendering
	void RenderEntities() const;
	void GameModeAndControlsText() const;
	void RenderSkyBox() const;

	// Destruction
	void DeleteTerrain();
	void DeleteEntities();

public:
	std::vector<Vertex_PCU> m_textVerts;
	Vec3 m_camPos = m_cameraPos;
	EulerAngles m_camOrientation = m_cameraOrientation;

	// Animals
	SnakeFollow*   m_snake = nullptr;
	SnakeFollow* m_snakeFollow = nullptr;
	SpiderIK*  m_spider2 = nullptr;
	SpiderIK* m_spider = nullptr;
	Octopus* m_octopus = nullptr;

	// Stationary animals
	Snake* m_snakeStationary = nullptr;
	SpiderIK* m_spiderStationary = nullptr;
	OctopusIK* m_octopusStationary = nullptr;

	// Entity list
	std::vector<Entity*> m_allEntities;

	// Terrain
	Terrain* m_terrain = nullptr;

	// Skybox
	std::vector<Vertex_PCU> m_skyBoxVerts;
	Texture* m_skyBoxFrontTexture = nullptr;

	// Debug Draw
	bool m_isSkeletonBeingDrawn = false;
	bool m_isAnimalVertsBeingDrawn = true;
	bool m_debugBounds = false;
	AnimalRenderFilter m_renderFilter = AnimalRenderFilter::ALL;
};