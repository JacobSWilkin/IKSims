#pragma once
#include "Game/Game.h"
// -----------------------------------------------------------------------------
class App;
// -----------------------------------------------------------------------------
constexpr float TARGETPOS_MOVE_SPEED = 4.f;
// -----------------------------------------------------------------------------
class CCDIKTest : public Game
{
public:
	CCDIKTest(App* owner);

	// Overrides
	void StartUp() override;
	void Update() override;
	void Render() const override;
	void Shutdown() override;

	// Initialization
	Skeleton CreateTestChain();

	// Updating
	void UpdateCameras(float deltaSeconds);
	void TargetPositionMovement(float deltaSeconds);

	void AddJoint();
	void RemoveJoint();

	// Rendering
	void RenderChain() const;
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