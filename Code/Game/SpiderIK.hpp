#pragma once
#include "Game/Entity.hpp"
#include "Engine/Core/Rgba8.h"
#include <vector>
// -----------------------------------------------------------------------------
class AnimalMode;
class VertexBuffer;
class IndexBuffer;
class Shader;
struct Vertex_PCUTBN;
// -----------------------------------------------------------------------------
constexpr float SPIDER_LERP_DURATION = 10.f;
constexpr float SPIDER_DIRECTION_TIME = 7.f;
constexpr float SPIDER_BODY_HEIGHT = 1.f;
constexpr float SPIDER_STEP_SPEED = 4.f;
constexpr int   MAX_SPIDER_VERTS = 2000;
constexpr int   MAX_SPIDER_INDICES = 2000;
constexpr float SPIDER_SUN_INTENSITY = 0.95f;
constexpr float SPIDER_AMBIENT_INTENSITY = 0.85f;
// -----------------------------------------------------------------------------
struct Leg
{
	Vec3 m_rootLocal;      // relative to body
	Vec3 m_jointPos;       // knee
	Vec3 m_footPos;        // current foot
	Vec3 m_targetPos;      // desired foot position

	float m_upperLen = 1.0f;
	float m_lowerLen = 1.0f;

	bool m_isStepping = false;
	float m_stepT = 0.f;
	Vec3 m_stepStart;

	float m_prevPhase = 0.f;
};
// -----------------------------------------------------------------------------
class SpiderIK : public Entity
{
public:
	SpiderIK(AnimalMode* mode, Vec3 position, Rgba8 color);
	~SpiderIK();

	virtual void Initialize() override;
	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;

	void UpdateSpiderPose(float deltaSeconds);
	void UpdateSpiderVerts();

private:
	void UpdateBody(float deltaSeconds);
	void UpdateLegs(float deltaSeconds);
	void SolveLegIK(Leg& leg);

	Vec3 GetTerrainPoint(Vec3 const& worldPos) const;
	Vec3 ClampToReach(Vec3 const& root, Vec3 const& target, float maxDist);
	void ResetLegsToCurrentPosition();

	void SpiderRoam(float deltaSeconds);

	void UpdateGPUBuffer();
	void DeleteBuffers();

private:
	std::vector<Leg> m_legs;
	std::vector<float> m_legPhaseOffsets;
	Vec3 m_velocity = Vec3::XAXE;
	bool m_debugDrawFootTargets = false;
	float m_stepPhase = 0.f;
	Rgba8 m_color = Rgba8::WHITE;

	VertexBuffer* m_vertexBuffer = nullptr;
	IndexBuffer* m_indexBuffer = nullptr;
	std::vector<Vertex_PCUTBN> m_spiderTBNVerts;
	std::vector<unsigned int> m_spiderIndices;
	Shader* m_spiderLit = nullptr;
	Vec3 m_sunDirection = Vec3(3.f, 1.f, -2.f);

	// Directional changes
	Vec3  m_targetMoveDirection = Vec3::XAXE;
	float m_directionInterpTime = 0.f;
	float m_directionChangeTimer = 0.f;
	float m_orientationDegrees = 0.f;
};
// -----------------------------------------------------------------------------