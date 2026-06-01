#pragma once
#include "Game/Entity.hpp"
#include "Engine/Core/Rgba8.h"
#include "Engine/Skeleton/Skeleton.hpp"
#include <vector>
// -----------------------------------------------------------------------------
class AnimalMode;
class VertexBuffer;
class IndexBuffer;
class Shader;
struct Vertex_PCUTBN;
// -----------------------------------------------------------------------------
constexpr int   NUM_TENTACLES = 8;
constexpr int   NUM_SMILE_SEGMENTS = 20;
constexpr int   NODES_PER_TENTACLE = 24;
constexpr int   OCTO_MAX_VERTS = 20000;
constexpr int   OCTO_MAX_INDICES = 20000;
constexpr int   MAX_ITERATIONS = 4;
constexpr float BODY_RAISE = 0.3f;
constexpr float BODY_TURN_SPEED = 2.5f;
constexpr float OCTO_MAX_SPEED = 6.f;
constexpr float OCTO_SUN_INTENSITY = 0.95f;
constexpr float OCTO_AMBIENT_INTENSITY = 0.85f;
constexpr float SEGMENT_LENGTH = 0.3f;
constexpr float BASE_RADIUS = 0.15f;
constexpr float TIP_RADIUS = 0.01f;
constexpr float SMILE_RADIUS = 0.08f;
constexpr float TENTACLE_TOTAL_LENGTH = SEGMENT_LENGTH * (NODES_PER_TENTACLE - 1);
// -----------------------------------------------------------------------------
struct Tentacle
{
	Vec3 m_rootLocal;

	std::vector<Vec3> m_nodes;     // chain positions
	std::vector<float> m_lengths;  // segment lengths

	Skeleton m_skeleton;

	Vec3 m_targetPos;

	bool m_isAnchored = false;
	float m_anchorTimer = 0.f;

	float m_phaseOffset = 0.f;
	Vec3 m_desiredTargetPos;

	std::vector<float> m_cachedTerrainZ;
};
// -----------------------------------------------------------------------------
class OctopusIK : public Entity
{
public:
	OctopusIK(AnimalMode* mode, Vec3 position, Rgba8 color);
	~OctopusIK();

	void Update(float deltaSeconds) override;
	void Render() const override;

private:
	void Initialize();

	void CreateTentacleSkeleton(Tentacle& tentacle, int tentacleIndex);

	void UpdateTentacles(float deltaSeconds);
	void SolveFABRIK(Tentacle& tentacle);
	void UpdateBody(float deltaSeconds);
	void UpdateVerts();
	float GetRadius(int nodeIndex, int nodeCount);
	void UpdateGPUBuffer();

	Vec3 GetTerrainPoint(Vec3 const& worldPos) const;
	void ResetTentaclesToCurrentPosition();

private:
	std::vector<Tentacle> m_tentacles;
	Vec3 m_velocity = Vec3::ZERO;
	Vec3 m_moveDir  = Vec3::XAXE;
	Vec3 m_roamDir = Vec3::XAXE;
	Vec3 m_lookDir = Vec3::XAXE;
	float m_roamTimer = 0.f;
	float m_roamDuration = 0.f;

	std::vector<Vertex_PCUTBN> m_verts;
	std::vector<unsigned int> m_indices;

	VertexBuffer* m_vbo = nullptr;
	IndexBuffer* m_ibo = nullptr;

	Shader* m_shader = nullptr;
	Rgba8 m_color;
	Vec3 m_sunDirection = Vec3(3.f, 1.f, -2.f);

	float m_time = 0.f;
	bool  m_debugDrawTargets = false;
};
// -----------------------------------------------------------------------------