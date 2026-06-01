#pragma once
#include "Game/Entity.hpp"
#include "Engine/Skeleton/Skeleton.hpp"
#include "Engine/Animation/AnimStateMachine.hpp"
#include "Engine/AI/BehaviorNode.hpp"
#include <vector>
// -----------------------------------------------------------------------------
constexpr float SNAKE_MAX_TURN = 60.f;
// -----------------------------------------------------------------------------
class AnimalMode;
class Animation;
class BehaviorTree;
// -----------------------------------------------------------------------------
enum class SnakeLocomotionMode
{
	SERPENTINE,
	CONCERTINA,
};
enum class SnakeSerpentineTechnique
{
	OSCILLATING_JOINTS,
	CURVATURE_DOWN_TAIL,
	NUM_MODES
};
// -----------------------------------------------------------------------------
enum class ConcertinaPhase
{
	ANCHORREAR,
	EXTENDFORWARD,
	ANCHORFRONT,
	PULLREAR
};
// -----------------------------------------------------------------------------
class Snake : public Entity
{
public:
	Snake(AnimalMode* animalMode, Vec3 position);
	~Snake();

	virtual void Initialize() override;
	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;

	void DrawSnake() const;
	void DrawSnakeSkeleton() const;
	void SnakeLocomotionKeyPresses();
	void IdleToggle();
	void ResetBindPose();

	bool IsMoving() const;
	AnimStateMachine GetSnakeStateMachine() const;

public:
	Vec3  m_targetMoveDirection = Vec3::XAXE;
	float m_directionInterpTime = 0.f;
	float m_directionInterpDuration = 40.f;
	AnimStateMachine m_snakeStateMachine;
	bool m_enableIdleTransitions = true;
	SnakeLocomotionMode m_locomotionMode = SnakeLocomotionMode::CONCERTINA;
	SnakeSerpentineTechnique m_currentSerpentineMode = SnakeSerpentineTechnique::CURVATURE_DOWN_TAIL;

private:
	Skeleton CreateSkeleton();

	void UpdateSnakePose(float deltaSeconds);
	void UpdateConcertinaMovement(float deltaSeconds);
	void ApplyConcertinaCurve(int startIndex, int endIndex, float anchorStrength);
	void SolveTerrainAlignment();
	void UpdateVerts();

	void SetupAnimations();
	void CheckTransitions();
	void DeleteAnimations();

	BehaviorTree* CreateSnakeBehaviorTree(Snake* snake);

private:
	Skeleton m_snakeSkeleton;
	std::vector<Vertex_PCU> m_snakeVerts;
	std::vector<Vertex_PCU> m_snakeVertsUntextured;
	std::vector<Vertex_PCU> m_snakeSkeletonVerts;

	Animation* m_snakeTailFlickIdleAnim = nullptr;
	Animation* m_snakeHeadRaiseIdleAnim = nullptr;
	Animation* m_snakeSlitherAnim = nullptr;
	BehaviorTree* m_snakeBT = nullptr;
	Texture* m_snakeTexture = nullptr;

	ConcertinaPhase m_concertinaPhase = ConcertinaPhase::ANCHORREAR;
	float m_concertinaTimer = 0.f;
	float m_concertinaPhaseDuration = 2.f;
	float m_concertinaProgress = 0.f;
	float m_sideWindTimer = 0.f;
	float m_rectilinearTimer = 0.f;

	std::vector<Vec3> m_snakeInitialPositions;
	std::vector<Vec3> m_snakeAnimationDirs;
};
// -----------------------------------------------------------------------------
/* Leaf Nodes */
class SnakeMoveNode : public BehaviorNode
{
public:
	SnakeMoveNode(Snake* snake)
		:m_snake(snake) {}

	BehaviorResult Update(float deltaSeconds) override;
	void Reset();

private:
	Snake* m_snake = nullptr;
	float  m_directionChangeTimer = 0.f;
	float  m_timeToChangeDir = 7.f;

	float m_moveDuration = 0.f;
	float m_maxMoveDuration = 30.f;
	bool  m_doneMoving = false;
};

class SnakeIdleNode : public BehaviorNode
{
public:
	SnakeIdleNode(Snake* snake, SnakeMoveNode* moveNode)
		:m_snake(snake), m_snakeMoveNode(moveNode) {}

	BehaviorResult Update(float deltaSeconds) override;
	void PlayRandomIdleAnimation();
	void Reset();

private:
	Snake* m_snake = nullptr;
	SnakeMoveNode* m_snakeMoveNode = nullptr;

	float m_idleDuration = 0.f;
	float m_maxIdleTime = 3.f;
	bool m_startedIdle = false;
};
// -----------------------------------------------------------------------------