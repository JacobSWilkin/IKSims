#pragma once
#include "Game/Entity.hpp"
#include "Engine/Skeleton/Skeleton.hpp"
// -----------------------------------------------------------------------------
class AnimalMode;
class VertexBuffer;
class IndexBuffer;
class Shader;
struct Vertex_PCUTBN;
// -----------------------------------------------------------------------------
constexpr int   NUM_OCTOPUS_ARMS = 8;
constexpr int   NUM_ARM_SEGMENTS = 8;
constexpr float SWIM_TEMPO       = 2.0f;
constexpr float OCTOPUS_INTERP_DURATION = 10.0f;
constexpr float OCTOPUS_DIRECTION_TIME = 7.f;
constexpr int   MAX_OCTOPUS_VERTS = 2000;
constexpr int   MAX_OCTOPUS_INDICES = 2000;
constexpr float OCTOPUS_SUN_INTENSITY = 0.55f;
constexpr float OCTOPUS_AMBIENT_INTENSITY = 0.35f;
// -----------------------------------------------------------------------------
class Octopus : public Entity
{
public:
	Octopus(AnimalMode* mode, Vec3 position);
	~Octopus();

	virtual void Initialize() override;
	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;

private:
	Skeleton CreateOctopusSkeleton();
	void DrawOctopus() const;
	void OctopusRoam(float deltaSeconds);
	void SolveCCDIK(std::vector<int> const& chainIndices, Vec3 const& targetPosition, int maxIterations, float threshold);
	void UpdateOctopusPose(float deltaSeconds);
	void ResetBindPose();
	void SolveTerrainAlignment();
	Vec3 GetTerrainPoint(Vec3 const& worldPos) const;
	void UpdateOctopusVerts();

	void UpdateGPUBuffer();
	void DeleteBuffers();

private:
	Skeleton m_octopus;
	std::vector<Vertex_PCU> m_octoVerts;
	std::vector<Vertex_PCU> m_octoSkeletonVerts;
	std::vector<Vertex_PCU> m_octoDebugVerts;
	std::vector<Vec3>       m_octopusInitialPositions;

	std::vector<std::vector<int>> m_armChains;
	std::vector<float> m_armPhaseOffsets;
	std::vector<float> m_armAmplitude;
	std::vector<bool>  m_isTipBone;

	VertexBuffer* m_vertexBuffer = nullptr;
	IndexBuffer* m_indexBuffer = nullptr;
	std::vector<Vertex_PCUTBN> m_octoTBNVerts;
	std::vector<unsigned int> m_octoIndices;
	Shader* m_octopusLit = nullptr;
	Vec3 m_sunDirection = Vec3(3.f, 1.f, -2.f);

	// Directional changes
	Vec3  m_targetMoveDirection = Vec3::XAXE;
	float m_directionInterpTime = 0.f;
	float m_directionChangeTimer = 0.f;
	bool  m_isRoaming = true;
	bool  m_isDebugTargetsOn = false;
};