#pragma once
#include "Game/Game.h"
// -----------------------------------------------------------------------------
class App;
// -----------------------------------------------------------------------------
constexpr float TARGET_MOVE_SPEED = 4.f;
// -----------------------------------------------------------------------------
class FABRIKTest : public Game
{
public:
	FABRIKTest(App* owner);

	void StartUp() override;
	void Update() override;
	void TargetPositionMovement(float deltaSeconds);
	void Render() const override;
	void Shutdown() override;

	// Initialization
	Skeleton CreateTestChain();

	// Updating
	void UpdateCameras(float deltaSeconds);
	void AddJoint();
	void RemoveJoint();

	// Rendering
	void RenderSkeleton() const;
	void RenderGrid() const;
	void SetWorldConstants() const;

	// Destruction

private:
	std::vector<Vertex_PCU> m_skeletonVerts;
	std::vector<Vertex_PCU> m_textVerts;
	Skeleton m_skeleton;
	Vec3 m_targetPosition = Vec3::ZERO;
	bool m_isTargetPlayerControlled = false;

	Shader* m_worldShader = nullptr;
	ConstantBuffer* m_world_CBO = nullptr;
};