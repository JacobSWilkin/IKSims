#pragma once
#include "Game/GameCommon.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Math/MathUtils.h"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Vertex_PCU.h"
#include "Engine/Skeleton/Skeleton.hpp"
// -----------------------------------------------------------------------------
class BitmapFont;
//------------------------------------------------------------------------------
constexpr int MAX_SKINNING_BONES = 128;
constexpr int SKINNING_REGISTER  = 5;
// -----------------------------------------------------------------------------
struct WorldConstants
{
	Vec4 CameraPosition = Vec4(0.f, 0.f, 0.f, 0.f);
	float FogNearDistance = 0.f;
	float FogFarDistance = 0.f;
	Vec2 WorldPadding = Vec2::ZERO;
};
// -----------------------------------------------------------------------------
struct SkinningConstants
{
	Mat44 m_boneMatrices[MAX_SKINNING_BONES];
};
// -----------------------------------------------------------------------------
class Game
{
public:
	// Initialization
	Game(App* owner) : m_app(owner) {};
	virtual ~Game() {};
	virtual void StartUp() = 0;
	void InitializeGrid();

	// Updating
	virtual void Update() = 0;

	// Rendering
	virtual void Render() const = 0;

	// Clearing
	virtual void Shutdown() = 0;

	// Input
	void AdjustForPauseAndTimeDistortion(float deltaSeconds);

	// Camera
	void    FreeFlyControls(float deltaSeconds);
	Vec3	GetCameraFwdNormal() const;

protected:
	App*		m_app = nullptr;
	Camera      m_gameWorldCamera;
	EulerAngles m_cameraOrientation = EulerAngles::ZERO;
	Vec3		m_cameraPos = Vec3(-10.f, 0.f, 5.f);

	BitmapFont* m_font = nullptr;
	AABB2		m_gameSceneBounds = AABB2(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
	std::vector<Vertex_PCU> m_gridVerts;
	bool m_isGridOn = true;
};