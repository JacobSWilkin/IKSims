#include "Game/SpiderIK.hpp"
#include "Game/AnimalMode.hpp"
#include "Game/Terrain.hpp"
#include "Game/App.h"
#include "Game/GameCommon.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Renderer/Renderer.h"

SpiderIK::SpiderIK(AnimalMode* mode, Vec3 position, Rgba8 color)
	:Entity(mode, position), m_color(color)
{
	m_spiderLit = g_theRenderer->CreateOrGetShader("Data/Shaders/BlinnPhong_Spider", VertexType::VERTEX_PCUTBN);
	m_type      = AnimalType::SPIDER;
	
	Initialize();
	UpdateSpiderVerts();
	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(MAX_SPIDER_VERTS * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	m_indexBuffer = g_theRenderer->CreateIndexBuffer(MAX_SPIDER_INDICES * sizeof(unsigned int), sizeof(unsigned int));
}

SpiderIK::~SpiderIK()
{
	DeleteBuffers();
}

void SpiderIK::Initialize()
{
	float bodyRadius = 0.4f;

	for (int legIndex = 0; legIndex < 4; ++legIndex)
	{
		float forward = (legIndex - 1.5f) * 1.0f;
		float forwardBias = forward * 1.3f;

		// LEFT
		{
			Leg leg;
			Vec3 dir = Vec3(forwardBias, -1.5f, -0.3f).GetNormalized();
			float legRootOffset = bodyRadius + 0.35f;
			leg.m_rootLocal = dir * legRootOffset;

			Vec3 worldRoot = m_worldPosition + leg.m_rootLocal;

			Vec3 outward = dir;
			leg.m_footPos = GetTerrainPoint(worldRoot + outward * 1.8f);
			leg.m_targetPos = leg.m_footPos;

			m_legs.push_back(leg);
		}

		// RIGHT
		{
			Leg leg;

			Vec3 dir = Vec3(forwardBias, 1.5f, -0.3f).GetNormalized();
			float legRootOffset = bodyRadius + 0.35f;
			leg.m_rootLocal = dir * legRootOffset;

			Vec3 worldRoot = m_worldPosition + leg.m_rootLocal;

			Vec3 outward = dir;
			leg.m_footPos = GetTerrainPoint(worldRoot + outward * 1.8f);
			leg.m_targetPos = leg.m_footPos;

			m_legs.push_back(leg);
		}
	}

	//m_legPhaseOffsets =
	//{
	//	0.0f, 0.50f,
	//	0.20f, 0.70f,
	//	0.50f, 0.10f,
	//	0.70f, 0.30f
	//};

	// Alternating gait
	m_legPhaseOffsets =
	{
		0.0f, 0.5f,
		0.5f, 0.0f,
		0.0f, 0.5f,
		0.5f, 0.0f
	};
}

void SpiderIK::Update(float deltaSeconds)
{
	if (g_theApp->m_gameClock->IsPaused())
	{
		return;
	}

	if (g_theInput->WasKeyJustPressed('L'))
	{
		m_debugDrawFootTargets = !m_debugDrawFootTargets;
	}

	UpdateSpiderPose(deltaSeconds);
	UpdateSpiderVerts();
}

void SpiderIK::UpdateSpiderPose(float deltaSeconds)
{
	static float elapsedTime = 0.f;
	elapsedTime += deltaSeconds;
	m_directionChangeTimer += deltaSeconds;

	if (m_isStationary)
	{
		m_moveDirection = -Vec3::XAXE;
	}

	// Check if spider is out of bounds, if so reset
	if (!m_isStationary)
	{
		m_worldPosition += m_moveDirection * deltaSeconds * m_speed;

		if (!m_animalMode->m_terrain->IsInBounds(static_cast<int>(m_worldPosition.x), static_cast<int>(m_worldPosition.y)))
		{
			float randomXLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
			float randomYLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
			m_worldPosition = Vec3(randomXLocation, randomYLocation, m_animalMode->m_terrain->GetHeightAtXY(randomXLocation, randomYLocation));
			ResetLegsToCurrentPosition();
		}

		// Direction change
		SpiderRoam(deltaSeconds);
	}

	float targetYaw = Atan2Degrees(m_moveDirection.y, m_moveDirection.x);
	m_orientationDegrees = targetYaw;

	UpdateLegs(deltaSeconds);
	UpdateBody(deltaSeconds);
}

void SpiderIK::UpdateSpiderVerts()
{
	m_spiderTBNVerts.clear();
	m_spiderIndices.clear();

	// Body
	Vec3 bodyA = Vec3(-0.25f, 0, 0).GetRotatedAboutZDegrees(m_orientationDegrees);
	Vec3 bodyB = Vec3(0.25f, 0, 0).GetRotatedAboutZDegrees(m_orientationDegrees);

	AddVertsForSphere3D(m_spiderTBNVerts, m_spiderIndices, m_worldPosition + bodyA, 0.48f, m_color);
	AddVertsForSphere3D(m_spiderTBNVerts, m_spiderIndices, m_worldPosition + bodyB, 0.32f, m_color);

	for (int legIndex = 0; legIndex < static_cast<int>(m_legs.size()); ++legIndex)
	{
		Leg const& leg = m_legs[legIndex];
		Vec3 rotatedRoot = leg.m_rootLocal.GetRotatedAboutZDegrees(m_orientationDegrees);
		Vec3 root = m_worldPosition + rotatedRoot;
		Vec3 bodyAttach = m_worldPosition + rotatedRoot.GetNormalized() * 0.3f;

		AddVertsForIndexedCylinder3D(m_spiderTBNVerts, m_spiderIndices, bodyAttach, root, 0.12f, m_color, AABB2::ZERO_TO_ONE, 16, false);

		float sideOffset = (legIndex % 2 == 0) ? 0.25f : -0.25f;

		Vec3 p0 = root;
		Vec3 p1 = (root * 0.7f + leg.m_footPos * 0.3f) + Vec3(0, sideOffset * 0.5f, 0.5f);
		Vec3 p2 = (root * 0.3f + leg.m_footPos * 0.7f) + Vec3(0, sideOffset * 0.5f, 0.3f);
		Vec3 p3 = leg.m_footPos;

		// Meshing out smooth spider leg
		AddVertsForSphere3D(m_spiderTBNVerts, m_spiderIndices, p0, 0.12f, m_color);
		AddVertsForIndexedTaperedCylinder3D(m_spiderTBNVerts, m_spiderIndices, p0, p1, 0.12f, 0.10f, m_color, AABB2::ZERO_TO_ONE, 16, false);
		AddVertsForSphere3D(m_spiderTBNVerts, m_spiderIndices, p1, 0.1f, m_color);
		AddVertsForIndexedTaperedCylinder3D(m_spiderTBNVerts, m_spiderIndices, p1, p2, 0.10f, 0.08f, m_color, AABB2::ZERO_TO_ONE, 16, false);
		AddVertsForSphere3D(m_spiderTBNVerts, m_spiderIndices, p2, 0.08f, m_color);
		AddVertsForIndexedTaperedCylinder3D(m_spiderTBNVerts, m_spiderIndices, p2, p3, 0.08f, 0.02f, m_color, AABB2::ZERO_TO_ONE, 16, false);

		if (m_animalMode->m_isSkeletonBeingDrawn)
		{
			// Bones
			AddVertsForIndexedCylinder3D(m_spiderTBNVerts, m_spiderIndices, p0, p1, 0.02f, Rgba8::WHITE);
			AddVertsForIndexedCylinder3D(m_spiderTBNVerts, m_spiderIndices, p1, p2, 0.02f, Rgba8::WHITE);
			AddVertsForIndexedCylinder3D(m_spiderTBNVerts, m_spiderIndices, p2, p3, 0.02f, Rgba8::WHITE);

			// Joints
			AddVertsForSphere3D(m_spiderTBNVerts, m_spiderIndices, p0, 0.05f, Rgba8::WHITE);
			AddVertsForSphere3D(m_spiderTBNVerts, m_spiderIndices, p1, 0.05f, Rgba8::WHITE);
			AddVertsForSphere3D(m_spiderTBNVerts, m_spiderIndices, p2, 0.05f, Rgba8::WHITE);
			AddVertsForSphere3D(m_spiderTBNVerts, m_spiderIndices, p3, 0.06f, Rgba8::WHITE);
		}

		// Foot targets
		if (m_debugDrawFootTargets)
		{
			AddVertsForSphere3D(m_spiderTBNVerts, m_spiderIndices, leg.m_targetPos, 0.08f, Rgba8::GREEN);
		}
	}

	UpdateGPUBuffer();
}

void SpiderIK::UpdateBody(float deltaSeconds)
{
	float avgHeight = 0.f;
	int count = 0;

	for (int legIndex = 0; legIndex < static_cast<int>(m_legs.size()); ++legIndex)
	{
		Leg const& leg = m_legs[legIndex];
		if (!leg.m_isStepping)
		{
			avgHeight += leg.m_targetPos.z;
			count++;
		}
	}

	if (count > 0)
	{
		avgHeight /= static_cast<float>(count);
	}

	float targetZ = avgHeight + SPIDER_BODY_HEIGHT;
	m_worldPosition.z = Interpolate(m_worldPosition.z, targetZ, deltaSeconds * 10.f);
}

void SpiderIK::Render() const
{
	if (m_animalMode->m_isAnimalVertsBeingDrawn)
	{
		g_theRenderer->SetLightingConstants(m_sunDirection, SPIDER_SUN_INTENSITY, SPIDER_AMBIENT_INTENSITY);
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
		if (m_animalMode->m_isSkeletonBeingDrawn)
		{
			g_theRenderer->SetDepthMode(DepthMode::DISABLED);
		}
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->BindShader(m_spiderLit);
		g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, static_cast<unsigned int>(m_spiderIndices.size()));
	}
}

void SpiderIK::UpdateLegs(float deltaSeconds)
{
	m_stepPhase += deltaSeconds * 1.5f;
	m_stepPhase = fmodf(m_stepPhase, 1.f);

	for (int legIndex = 0; legIndex < static_cast<int>(m_legs.size()); ++legIndex)
	{
		Leg& leg = m_legs[legIndex];

		float legPhase = fmodf(m_stepPhase + m_legPhaseOffsets[legIndex], 1.f);
		bool phaseJustStarted = (leg.m_prevPhase >= 0.5f && legPhase < 0.5f);

		leg.m_prevPhase = legPhase;

		Vec3 rotatedRoot = leg.m_rootLocal.GetRotatedAboutZDegrees(m_orientationDegrees);
		Vec3 worldRoot = m_worldPosition + rotatedRoot;

		Vec3 velocity = m_moveDirection;
		Vec3 forward = Vec3::ZERO;

		if (!m_isStationary)
		{
			forward = velocity.GetNormalized();
			if (forward.GetLengthSquared() < 0.001f)
			{
				forward = Vec3::XAXE;
			}
		}

		float maxReach = leg.m_upperLen + leg.m_lowerLen;

		float stride = 1.5f;
		Vec3 outward = leg.m_rootLocal.GetNormalized().GetRotatedAboutZDegrees(m_orientationDegrees);
		Vec3 desired = GetTerrainPoint(worldRoot + forward * stride + outward * 1.0f);

		float dist = (leg.m_footPos - desired).GetLength();

		if (!leg.m_isStepping && phaseJustStarted)
		{
			if (m_isStationary || dist > maxReach * 0.65f)
			{
				leg.m_isStepping = true;
				leg.m_stepT = 0.f;
				leg.m_stepStart = leg.m_footPos;
				leg.m_targetPos = desired;
			}
		}

		if (leg.m_isStepping)
		{
			leg.m_stepT += deltaSeconds * SPIDER_STEP_SPEED;

			float t = GetClamped(leg.m_stepT, 0.f, 1.f);
			float height = sinf(t * PI) * 0.8f;

			leg.m_footPos = Interpolate(leg.m_stepStart, leg.m_targetPos, t);
			leg.m_footPos.z += height;

			if (t >= 1.f)
			{
				leg.m_isStepping = false;
				leg.m_footPos = leg.m_targetPos;
			}
		}

		SolveLegIK(leg);
	}
}

void SpiderIK::SolveLegIK(Leg& leg)
{
	Vec3 rotatedRoot = leg.m_rootLocal.GetRotatedAboutZDegrees(m_orientationDegrees);
	Vec3 root = m_worldPosition + rotatedRoot;
	Vec3 target = leg.m_footPos;

	float a = leg.m_upperLen;
	float b = leg.m_lowerLen;

	float maxReach = a + b;
	target = ClampToReach(root, target, maxReach);

	Vec3 toTarget = target - root;
	float dist = toTarget.GetLength();

	dist = GetClamped(dist, 0.001f, a + b - 0.001f);

	Vec3 dir = toTarget.GetNormalized();

	Vec3 outward = (root - m_worldPosition).GetNormalized();
	Vec3 bendDir = CrossProduct3D(outward, dir).GetNormalized();

	if (bendDir.GetLengthSquared() < 0.001f)
	{
		bendDir = Vec3(0, 0, 1);
	}

	float cosAngle = (a * a + dist * dist - b * b) / (2.f * a * dist);
	float sinAngle = sqrtf(1.f - cosAngle * cosAngle);

	Vec3 jointOffset = dir * (cosAngle * a) + bendDir * (sinAngle * a);

	leg.m_jointPos = root + jointOffset;
}

Vec3 SpiderIK::GetTerrainPoint(Vec3 const& worldPos) const
{
	Terrain* terrain = m_animalMode->m_terrain;
	float height = terrain->GetHeightAtXY(worldPos.x, worldPos.y);
	return Vec3(worldPos.x, worldPos.y, height);
}

Vec3 SpiderIK::ClampToReach(Vec3 const& root, Vec3 const& target, float maxDist)
{
	Vec2 rootXY = Vec2(root.x, root.y);
	Vec2 targetXY = Vec2(target.x, target.y);

	Vec2 toTarget = targetXY - rootXY;
	float dist = toTarget.GetLength();

	if (dist > maxDist)
	{
		Vec2 clampedXY = rootXY + toTarget.GetNormalized() * maxDist;
		float z = GetTerrainPoint(Vec3(clampedXY.x, clampedXY.y, 0.f)).z;
		return Vec3(clampedXY.x, clampedXY.y, z);
	}

	return target;
}

void SpiderIK::ResetLegsToCurrentPosition()
{
	for (int legIndex = 0; legIndex < static_cast<int>(m_legs.size()); ++legIndex)
	{
		Leg& leg = m_legs[legIndex];

		Vec3 rotatedRoot = leg.m_rootLocal.GetRotatedAboutZDegrees(m_orientationDegrees);
		Vec3 worldRoot = m_worldPosition + rotatedRoot;

		Vec3 outward = leg.m_rootLocal.GetNormalized().GetRotatedAboutZDegrees(m_orientationDegrees);

		Vec3 newFoot = GetTerrainPoint(worldRoot + outward * 1.8f);

		leg.m_footPos = newFoot;
		leg.m_targetPos = newFoot;
		leg.m_stepStart = newFoot;

		leg.m_isStepping = false;
		leg.m_stepT = 0.f;
	}
}

void SpiderIK::SpiderRoam(float deltaSeconds)
{
	if (m_directionChangeTimer >= SPIDER_DIRECTION_TIME)
	{
		m_directionChangeTimer = 0.f;

		// Picking a new random direction for the spider to go
		float currentAngle = Atan2Degrees(m_moveDirection.y, m_moveDirection.x);
		float delta = g_rng->RollRandomFloatInRange(-45.f, 45.f);
		float newAngle = currentAngle + delta;

		Vec2 directionXY = Vec2(CosDegrees(newAngle), SinDegrees(newAngle));
		m_targetMoveDirection = Vec3(directionXY.x, directionXY.y, 0.f).GetNormalized();
		m_directionInterpTime = 0.f;
	}

	// Smoothly interpolate move direction
	if (m_moveDirection != m_targetMoveDirection)
	{
		m_directionInterpTime += deltaSeconds;
		float fractionTowardEnd = GetClamped(m_directionInterpTime / SPIDER_DIRECTION_TIME, 0.f, 1.f);

		float easedFraction = SmoothStep3(fractionTowardEnd);
		m_moveDirection = SLerp(m_moveDirection, m_targetMoveDirection, easedFraction).GetNormalized();
	}
}

void SpiderIK::UpdateGPUBuffer()
{
	if (!m_vertexBuffer)
	{
		return;
	}

	if (!m_indexBuffer)
	{
		return;
	}

	unsigned int sizeInBytes = static_cast<unsigned int>(m_spiderTBNVerts.size()) * sizeof(Vertex_PCUTBN);
	g_theRenderer->CopyCPUToGPU(m_spiderTBNVerts.data(), sizeInBytes, m_vertexBuffer);

	unsigned int indexBytes = static_cast<unsigned int>(m_spiderIndices.size()) * sizeof(unsigned int);
	g_theRenderer->CopyCPUToGPU(m_spiderIndices.data(), indexBytes, m_indexBuffer);
}

void SpiderIK::DeleteBuffers()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;
}
