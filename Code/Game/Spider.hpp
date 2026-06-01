#pragma once
#include "Game/Entity.hpp"
#include "Engine/Skeleton/Skeleton.hpp"
#include "Engine/AI/BehaviorNode.hpp"
// -----------------------------------------------------------------------------
class AnimalMode;
// -----------------------------------------------------------------------------
enum class BoneType
{
	Abdomen,
	Head,
	Femur,
	Other
};
// -----------------------------------------------------------------------------
class Spider : public Entity
{
public:
	Spider(AnimalMode* mode, Vec3 position);
	~Spider();

	virtual void Initialize() override;
	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;

	void DrawSpider() const;
	void DrawSpiderSkeleton() const;

	void SetIsRoaming(bool isRoaming);

private:
	Skeleton CreateSkeleton();

	void UpdateSpiderPose(float deltaSeconds);
	void SolveTerrainAlignment();
	void SpiderRoam(float deltaSeconds);
	void UpdateSpiderVerts();

private:
	Skeleton m_spider;
	std::vector<Vertex_PCU> m_spiderVerts;
	std::vector<Vertex_PCU> m_spiderSkeletonVerts;
	std::vector<BoneType> m_boneTypes;
	bool m_isDrawingLegTargetPos = false;

	// Directional changes
	Vec3  m_targetMoveDirection = Vec3::XAXE;
	float m_directionInterpTime = 0.f;
	float m_directionInterpDuration = 10.f;
	float m_directionChangeTimer = 0.f;
	float m_timeToChangeDir = 7.f;
	bool  m_isRoaming = false;
};
// -----------------------------------------------------------------------------