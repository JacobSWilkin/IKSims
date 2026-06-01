#pragma once
#include "Game/Game.h"
#include <deque>
// -----------------------------------------------------------------------------
class App;
// -----------------------------------------------------------------------------
constexpr float CAMERA_MIN_ZOOM = 0.2f;
constexpr float CAMERA_MAX_ZOOM = 5.f;
constexpr float SPIDER_LEG_RADIUS = 125.f;
constexpr int   SNAKE_BONE_COUNT = 55;
constexpr float SNAKE_HEAD_SPEED = 400.f;
constexpr float SNAKE_SEGMENT_SPACING = 20.f;
// -----------------------------------------------------------------------------
enum class GameMode2D
{
	SPIDER_2D,
	SNAKE_CHAIN,
	ROBOT_ARM,
	NUM_2D_MODES
};
// -----------------------------------------------------------------------------
struct SpiderLeg2D
{
	std::vector<Vec2> joints;
	std::vector<float> lengths;

	Vec2 footPos;
	Vec2 footTarget;
	Vec2 footStart;

	float stepT = 1.f;
};
// -----------------------------------------------------------------------------
class Game2D : public Game
{
public:
	Game2D(App* owner);
	~Game2D();

	// Overrides
	void StartUp() override;

	void Update() override;
	void UpdateSpider(float deltaSeconds);
	void UpdateRobotArm(float deltaSeconds);
	void UpdateSnake(float deltaSeconds);
	void UpdateCameras();

	void Render() const override;
	void RenderRobotArm() const;
	void RenderSnake() const;
	void RenderSpider() const;

	void Shutdown() override;

	// Initialization
	Skeleton CreateRobotArmSkeleton2D();
	Skeleton CreateSnakeSkeleton2D();

	// Updating
	void CameraKeyPresses(float deltaSeconds);

	// Rendering
	void GameModeAndControlsText() const;
	void SolveFABRIK2D(SpiderLeg2D& leg, Vec2 const& root, Vec2 const& target);
	void SolveFABRIKRobotArm(Skeleton& skeleton, Vec2 const& target);

private:
	std::vector<Vertex_PCU> m_skeletonVerts;
	Vec2 m_screenCamPos = Vec2::ZERO;
	float m_cameraZoom = 1.f;
	GameMode2D m_mode = GameMode2D::ROBOT_ARM;
	Camera m_uiCamera;

	// Spider data
	std::vector<SpiderLeg2D> m_legs;
	float m_legAngles[8] = {};
	Vec2 m_spiderBodyPos = Vec2::ZERO;
	bool m_showFootTargets = false;

	// Snake data
	Skeleton m_snakeSkeleton;
	SkeletonStyle m_snakeStyle;

	// Robot Arm
	std::vector<float> m_robotArmLengths;
	Skeleton m_robotArmSkeleton;
	SkeletonStyle m_robotArmStyle;
	Vec2 m_robotTarget = Vec2::ZERO;
	bool m_draggingTarget = false;
};