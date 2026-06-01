#include "Game/Spider.hpp"
#include "Game/AnimalMode.hpp"
#include "Game/Terrain.hpp"
#include "Game/GameCommon.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Core/Time.hpp"

Spider::Spider(AnimalMode* mode, Vec3 position)
	:Entity(mode, position)
{
	Initialize();
}

Spider::~Spider()
{
}

void Spider::Initialize()
{
	m_spider = CreateSkeleton();
}

void Spider::Update(float deltaSeconds)
{
	UpdateSpiderPose(deltaSeconds);
	UpdateSpiderVerts();

	if (g_theInput->WasKeyJustPressed('L'))
	{
		m_isDrawingLegTargetPos = !m_isDrawingLegTargetPos;
	}
}

void Spider::UpdateSpiderPose(float deltaSeconds)
{
	static float elapsedTime = 0.f;
	elapsedTime += deltaSeconds;
	m_directionChangeTimer += deltaSeconds;

	// Direction change
	SpiderRoam(deltaSeconds);

	// Animate spider legs 
	for (int spiderBoneIndex = 0; spiderBoneIndex < static_cast<int>(m_spider.m_bones.size()); ++spiderBoneIndex)
	{
		Bone& spiderBone = m_spider.m_bones[spiderBoneIndex];
		if (m_boneTypes[spiderBoneIndex] == BoneType::Femur)
		{
			float legMovement = sinf(elapsedTime * spiderBoneIndex * 0.2f) * 0.35f;
			spiderBone.SetLocalBoneRotation(Quat::MakeFromAxisAngle(Vec3::YAXE, legMovement));
		}
	}

	if (!m_isStationary)
	{
		// Move spider forward
		Vec3 spiderMove = m_moveDirection * m_speed * deltaSeconds;
		m_worldPosition -= spiderMove;

		// Project onto terrain
		float terrainZ = m_animalMode->m_terrain->GetHeightAtXY(m_worldPosition.x, m_worldPosition.y);
		m_worldPosition.z = terrainZ;

		// Check if spider is out of bounds, if so reset
		if (!m_animalMode->m_terrain->IsInBounds(static_cast<int>(m_worldPosition.x), static_cast<int>(m_worldPosition.y)))
		{
			float randomXLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
			float randomYLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
			m_worldPosition = Vec3(randomXLocation, randomYLocation, m_animalMode->m_terrain->GetHeightAtXY(randomXLocation, randomYLocation));
			elapsedTime = 0.f;
		}
	}

	// Spider body bobbing
	if (!m_isStationary)
	{
		float bodyBob = sinf(elapsedTime * 2.f * PI) * 0.03f;
		m_worldPosition.z += bodyBob;
	}

	SolveTerrainAlignment();
}

void Spider::SolveTerrainAlignment()
{
	float offset = 1.f;
	Terrain* terrain = m_animalMode->m_terrain;

	// Get current and neighbor positions
	float hCenter = terrain->GetHeightAtXY(m_worldPosition.x, m_worldPosition.y);
	float hForward = terrain->GetHeightAtXY(m_worldPosition.x + m_moveDirection.x * offset, m_worldPosition.y + m_moveDirection.y * offset);
	float hLeft = terrain->GetHeightAtXY(m_worldPosition.x + m_moveDirection.y * offset, m_worldPosition.y - m_moveDirection.x * offset);

	// Build local tangent vectors
	Vec3 forward = (Vec3(m_moveDirection.x, m_moveDirection.y, hForward - hCenter)).GetNormalized();
	Vec3 left = (Vec3(m_moveDirection.y, -m_moveDirection.x, hLeft - hCenter)).GetNormalized();

	// Calculating the terrain normal
	Vec3 up = CrossProduct3D(left, forward).GetNormalized();

	// Recalculating the orthogonal left vector
	left = CrossProduct3D(forward, up).GetNormalized();

	// Build orientation matrix
	Mat44 orientation;
	orientation.SetIJK3D(forward, left, up);
	orientation.SetTranslation3D(m_worldPosition);

	// Scaling up spider skeleton
	float spiderScale = 2.f;
	orientation.AppendScaleUniform3D(spiderScale);

	// Apply it to the skeleton
	m_spider.m_skeletonModelTransform = orientation;
	m_spider.UpdateSkeletonPose();
}

void Spider::SpiderRoam(float deltaSeconds)
{
	if (m_isRoaming)
	{
		if (m_directionChangeTimer >= m_timeToChangeDir)
		{
			m_directionChangeTimer = 0.f;

			// Picking a new random direction for the spider to go
			float angle = g_rng->RollRandomFloatInRange(0.f, 60.f);
			Vec2 directionXY = Vec2(CosDegrees(angle), SinDegrees(angle));
			Vec3 newDirection = Vec3(directionXY.x, directionXY.y, 0.f).GetNormalized();
			m_targetMoveDirection = newDirection;
			m_directionInterpTime = 0.f;
		}

		// Smoothly interpolate move direction
		if (m_moveDirection != m_targetMoveDirection)
		{
			m_directionInterpTime += deltaSeconds;
			float fractionTowardEnd = GetClamped(m_directionInterpTime / m_directionInterpDuration, 0.f, 1.f);

			float easedFraction = SmoothStep3(fractionTowardEnd);
			m_moveDirection = SLerp(m_moveDirection, m_targetMoveDirection, easedFraction).GetNormalized();
		}
	}
}

void Spider::UpdateSpiderVerts()
{
	m_spiderVerts.clear();
	if (m_animalMode->m_isSkeletonBeingDrawn)
	{
		m_spiderSkeletonVerts.clear();
		m_spider.AddVertsForSkeleton3D(m_spiderSkeletonVerts);
	}

	for (int boneIndex = 0; boneIndex < static_cast<int>(m_spider.m_bones.size()); ++boneIndex)
	{
		Bone const& bone = m_spider.m_bones[boneIndex];
		Vec3 boneWorldPos = bone.GetWorldBonePosition3D();
		float radius = 0.1f;

		if (m_boneTypes[boneIndex] == BoneType::Abdomen)
		{
			AddVertsForSphere3D(m_spiderVerts, boneWorldPos, 0.4f, Rgba8::BLACK);
		}
		else if (m_boneTypes[boneIndex] == BoneType::Head)
		{
			AddVertsForSphere3D(m_spiderVerts, boneWorldPos, 0.3f, Rgba8::BLACK);
		}
		else
		{
			AddVertsForSphere3D(m_spiderVerts, boneWorldPos, radius, Rgba8::BLACK);
		}

		if (bone.m_parentBoneIndex == -1)
		{
			continue;
		}

		Vec3 start = m_spider.m_bones[bone.m_parentBoneIndex].m_worldBoneTransform.GetTranslation3D();
		Vec3 end = bone.m_worldBoneTransform.GetTranslation3D();

		AddVertsForCylinder3D(m_spiderVerts, start, end, radius, Rgba8::BLACK);
	}
}

void Spider::Render() const
{
	DrawSpider();
	DrawSpiderSkeleton();
}

void Spider::DrawSpiderSkeleton() const
{
	if (m_animalMode->m_isSkeletonBeingDrawn)
	{
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::DISABLED);
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(m_spiderSkeletonVerts);
	}
}

void Spider::DrawSpider() const
{
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->BindTexture(nullptr);
	if (m_animalMode->m_isAnimalVertsBeingDrawn)
	{
		g_theRenderer->DrawVertexArray(m_spiderVerts);
	}
}

void Spider::SetIsRoaming(bool isRoaming)
{
	m_isRoaming = isRoaming;
}

Skeleton Spider::CreateSkeleton()
{
	Skeleton spiderSkeleton;
	spiderSkeleton.m_bones.clear();

	Bone abdomen;
	abdomen.m_boneName = "Abdomen";
	abdomen.SetLocalBonePosition(Vec3(0.f, 0.f, 0.6f));
	m_boneTypes.push_back(BoneType::Abdomen);
	spiderSkeleton.m_bones.push_back(abdomen);

	/* ----------------Spider Head----------------- */
	Bone head;
	head.m_boneName = "Head";
	head.m_parentBoneIndex = 0;
	head.SetLocalBonePosition(Vec3(-0.25f, 0.f, -0.1f));
	m_boneTypes.push_back(BoneType::Head);
	spiderSkeleton.m_bones.push_back(head);
	/* -------------------------------------------- */

	/* ------------Spider Left front leg-----------*/
	Bone leftFrontFemur;
	leftFrontFemur.m_boneName = "LeftFrontFemur";
	leftFrontFemur.m_parentBoneIndex = 1;
	leftFrontFemur.SetLocalBonePosition(Vec3(-0.65f, 0.25f, 0.5f));
	m_boneTypes.push_back(BoneType::Femur);
	spiderSkeleton.m_bones.push_back(leftFrontFemur);

	Bone leftFrontTibia;
	leftFrontTibia.m_boneName = "LeftFrontTibia";
	leftFrontTibia.m_parentBoneIndex = 2;
	leftFrontTibia.SetLocalBonePosition(Vec3(-0.4f, 0.25f, 0.0f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftFrontTibia);

	Bone leftFrontMetaTarsus;
	leftFrontMetaTarsus.m_boneName = "LeftFrontMetaTarsus";
	leftFrontMetaTarsus.m_parentBoneIndex = 3;
	leftFrontMetaTarsus.SetLocalBonePosition(Vec3(-0.2f, 0.25f, -0.5f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftFrontMetaTarsus);

	Bone leftFrontTarsus;
	leftFrontTarsus.m_boneName = "LeftFrontTarsus";
	leftFrontTarsus.m_parentBoneIndex = 4;
	leftFrontTarsus.SetLocalBonePosition(Vec3(-0.2f, 0.25f, -0.5f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftFrontTarsus);
	/* -------------------------------------------- */

	/* ------------Spider Right front leg-----------*/
	Bone rightFrontFemur;
	rightFrontFemur.m_boneName = "RightFrontFemur";
	rightFrontFemur.m_parentBoneIndex = 1;
	rightFrontFemur.SetLocalBonePosition(Vec3(-0.65f, -0.25f, 0.5f));
	m_boneTypes.push_back(BoneType::Femur);
	spiderSkeleton.m_bones.push_back(rightFrontFemur);

	Bone rightFrontTibia;
	rightFrontTibia.m_boneName = "RightFrontTibia";
	rightFrontTibia.m_parentBoneIndex = 6;
	rightFrontTibia.SetLocalBonePosition(Vec3(-0.4f, -0.25f, 0.0f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightFrontTibia);

	Bone rightFrontMetaTarsus;
	rightFrontMetaTarsus.m_boneName = "RightFrontMetaTarsus";
	rightFrontMetaTarsus.m_parentBoneIndex = 7;
	rightFrontMetaTarsus.SetLocalBonePosition(Vec3(-0.2f, -0.25f, -0.5f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightFrontMetaTarsus);

	Bone rightFrontTarsus;
	rightFrontTarsus.m_boneName = "RightFrontTarsus";
	rightFrontTarsus.m_parentBoneIndex = 8;
	rightFrontTarsus.SetLocalBonePosition(Vec3(-0.2f, -0.25f, -0.5f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightFrontTarsus);
	/* -------------------------------------------- */

	/* ----------Spider Left Front Middle Leg------ */
	Bone leftFrontMiddleFemur;
	leftFrontMiddleFemur.m_boneName = "LeftFrontMidFemur";
	leftFrontMiddleFemur.m_parentBoneIndex = 1;
	leftFrontMiddleFemur.SetLocalBonePosition(Vec3(-0.15f, 0.5f, 0.f));
	m_boneTypes.push_back(BoneType::Femur);
	spiderSkeleton.m_bones.push_back(leftFrontMiddleFemur);

	Bone leftFrontMiddleTibia;
	leftFrontMiddleTibia.m_boneName = "LeftFrontMiddleTibia";
	leftFrontMiddleTibia.m_parentBoneIndex = 10;
	leftFrontMiddleTibia.SetLocalBonePosition(Vec3(-0.15f, 0.25f, 0.f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftFrontMiddleTibia);

	Bone leftFrontMiddleMetaTarsus;
	leftFrontMiddleMetaTarsus.m_boneName = "LeftFrontMiddleMetaTarsus";
	leftFrontMiddleMetaTarsus.m_parentBoneIndex = 11;
	leftFrontMiddleMetaTarsus.SetLocalBonePosition(Vec3(-0.15f, 0.15f, -0.3f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftFrontMiddleMetaTarsus);

	Bone leftFrontMiddleTarsus;
	leftFrontMiddleTarsus.m_boneName = "LeftFrontMiddleTarsus";
	leftFrontMiddleTarsus.m_parentBoneIndex = 12;
	leftFrontMiddleTarsus.SetLocalBonePosition(Vec3(-0.15f, 0.1f, -0.2f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftFrontMiddleTarsus);
	/* -------------------------------------------- */

	/* ----------Spider Right Front Middle Leg------ */
	Bone rightFrontMiddleFemur;
	rightFrontMiddleFemur.m_boneName = "RightFrontMidFemur";
	rightFrontMiddleFemur.m_parentBoneIndex = 1;
	rightFrontMiddleFemur.SetLocalBonePosition(Vec3(-0.15f, -0.5f, 0.f));
	m_boneTypes.push_back(BoneType::Femur);
	spiderSkeleton.m_bones.push_back(rightFrontMiddleFemur);

	Bone rightFrontMiddleTibia;
	rightFrontMiddleTibia.m_boneName = "RightFrontMiddleTibia";
	rightFrontMiddleTibia.m_parentBoneIndex = 14;
	rightFrontMiddleTibia.SetLocalBonePosition(Vec3(-0.15f, -0.25f, 0.f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightFrontMiddleTibia);

	Bone rightFrontMiddleMetaTarsus;
	rightFrontMiddleMetaTarsus.m_boneName = "RightFrontMiddleMetaTarsus";
	rightFrontMiddleMetaTarsus.m_parentBoneIndex = 15;
	rightFrontMiddleMetaTarsus.SetLocalBonePosition(Vec3(-0.15f, -0.15f, -0.3f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightFrontMiddleMetaTarsus);

	Bone rightFrontMiddleTarsus;
	rightFrontMiddleTarsus.m_boneName = "RightFrontMiddleTarsus";
	rightFrontMiddleTarsus.m_parentBoneIndex = 16;
	rightFrontMiddleTarsus.SetLocalBonePosition(Vec3(-0.15f, -0.1f, -0.2f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightFrontMiddleTarsus);
	/* -------------------------------------------- */

	/* ----------Spider Left Back Middle Leg------ */
	Bone leftBackMiddleFemur;
	leftBackMiddleFemur.m_boneName = "LeftBackMidFemur";
	leftBackMiddleFemur.m_parentBoneIndex = 1;
	leftBackMiddleFemur.SetLocalBonePosition(Vec3(0.15f, 0.5f, 0.f));
	m_boneTypes.push_back(BoneType::Femur);
	spiderSkeleton.m_bones.push_back(leftBackMiddleFemur);

	Bone leftBackMiddleTibia;
	leftBackMiddleTibia.m_boneName = "LeftBackMiddleTibia";
	leftBackMiddleTibia.m_parentBoneIndex = 18;
	leftBackMiddleTibia.SetLocalBonePosition(Vec3(0.15f, 0.25f, 0.f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftBackMiddleTibia);

	Bone leftBackMiddleMetaTarsus;
	leftBackMiddleMetaTarsus.m_boneName = "LeftBackMiddleMetaTarsus";
	leftBackMiddleMetaTarsus.m_parentBoneIndex = 19;
	leftBackMiddleMetaTarsus.SetLocalBonePosition(Vec3(0.15f, 0.15f, -0.3f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftBackMiddleMetaTarsus);

	Bone leftBackMiddleTarsus;
	leftBackMiddleTarsus.m_boneName = "LeftBackMiddleTarsus";
	leftBackMiddleTarsus.m_parentBoneIndex = 20;
	leftBackMiddleTarsus.SetLocalBonePosition(Vec3(0.15f, 0.1f, -0.2f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftBackMiddleTarsus);
	/* -------------------------------------------- */

	/* ----------Spider Right Back Middle Leg------ */
	Bone rightBackMiddleFemur;
	rightBackMiddleFemur.m_boneName = "RightBackMidFemur";
	rightBackMiddleFemur.m_parentBoneIndex = 1;
	rightBackMiddleFemur.SetLocalBonePosition(Vec3(0.15f, -0.5f, 0.f));
	m_boneTypes.push_back(BoneType::Femur);
	spiderSkeleton.m_bones.push_back(rightBackMiddleFemur);

	Bone rightBackMiddleTibia;
	rightBackMiddleTibia.m_boneName = "RightBackMiddleTibia";
	rightBackMiddleTibia.m_parentBoneIndex = 22;
	rightBackMiddleTibia.SetLocalBonePosition(Vec3(0.15f, -0.25f, 0.f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightBackMiddleTibia);

	Bone rightBackMiddleMetaTarsus;
	rightBackMiddleMetaTarsus.m_boneName = "RightBackMiddleMetaTarsus";
	rightBackMiddleMetaTarsus.m_parentBoneIndex = 23;
	rightBackMiddleMetaTarsus.SetLocalBonePosition(Vec3(0.15f, -0.15f, -0.3f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightBackMiddleMetaTarsus);

	Bone rightBackMiddleTarsus;
	rightBackMiddleTarsus.m_boneName = "RightBackMiddleTarsus";
	rightBackMiddleTarsus.m_parentBoneIndex = 24;
	rightBackMiddleTarsus.SetLocalBonePosition(Vec3(0.15f, -0.1f, -0.2f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightBackMiddleTarsus);
	/* -------------------------------------------- */

	/* ------------Spider Left back leg-----------*/
	Bone leftBackFemur;
	leftBackFemur.m_boneName = "LeftBackFemur";
	leftBackFemur.m_parentBoneIndex = 1;
	leftBackFemur.SetLocalBonePosition(Vec3(0.8f, 0.25f, 0.5f));
	m_boneTypes.push_back(BoneType::Femur);
	spiderSkeleton.m_bones.push_back(leftBackFemur);

	Bone leftBackTibia;
	leftBackTibia.m_boneName = "LeftBackTibia";
	leftBackTibia.m_parentBoneIndex = 26;
	leftBackTibia.SetLocalBonePosition(Vec3(0.4f, 0.25f, 0.0f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftBackTibia);

	Bone leftBackMetaTarsus;
	leftBackMetaTarsus.m_boneName = "LeftBackMetaTarsus";
	leftBackMetaTarsus.m_parentBoneIndex = 27;
	leftBackMetaTarsus.SetLocalBonePosition(Vec3(0.2f, 0.25f, -0.5f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftBackMetaTarsus);

	Bone leftBackTarsus;
	leftBackTarsus.m_boneName = "LeftBackTarsus";
	leftBackTarsus.m_parentBoneIndex = 28;
	leftBackTarsus.SetLocalBonePosition(Vec3(0.2f, 0.25f, -0.5f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(leftBackTarsus);
	/* -------------------------------------------- */

	/* ------------Spider Right back leg-----------*/
	Bone rightBackFemur;
	rightBackFemur.m_boneName = "RightBackFemur";
	rightBackFemur.m_parentBoneIndex = 1;
	rightBackFemur.SetLocalBonePosition(Vec3(0.8f, -0.25f, 0.5f));
	m_boneTypes.push_back(BoneType::Femur);
	spiderSkeleton.m_bones.push_back(rightBackFemur);

	Bone rightBackTibia;
	rightBackTibia.m_boneName = "RightBackTibia";
	rightBackTibia.m_parentBoneIndex = 30;
	rightBackTibia.SetLocalBonePosition(Vec3(0.4f, -0.25f, 0.0f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightBackTibia);

	Bone rightBackMetaTarsus;
	rightBackMetaTarsus.m_boneName = "RightBackMetaTarsus";
	rightBackMetaTarsus.m_parentBoneIndex = 31;
	rightBackMetaTarsus.SetLocalBonePosition(Vec3(0.2f, -0.25f, -0.5f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightBackMetaTarsus);

	Bone rightBackTarsus;
	rightBackTarsus.m_boneName = "RightBackTarsus";
	rightBackTarsus.m_parentBoneIndex = 32;
	rightBackTarsus.SetLocalBonePosition(Vec3(0.2f, -0.25f, -0.5f));
	m_boneTypes.push_back(BoneType::Other);
	spiderSkeleton.m_bones.push_back(rightBackTarsus);
	/* -------------------------------------------- */

	spiderSkeleton.UpdateSkeletonPose();
	spiderSkeleton.CaptureBindPose();
	return spiderSkeleton;
}