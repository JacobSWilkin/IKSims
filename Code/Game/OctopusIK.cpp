#include "Game/OctopusIK.hpp"
#include "Game/AnimalMode.hpp"
#include "Game/Terrain.hpp"
#include "Game/App.h"
#include "Game/GameCommon.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Renderer/Renderer.h"

OctopusIK::OctopusIK(AnimalMode* mode, Vec3 position, Rgba8 color)
	: Entity(mode, position)
	, m_color(color)
{
	m_shader = g_theRenderer->CreateOrGetShader("Data/Shaders/BlinnPhong_Octopus", VertexType::VERTEX_PCUTBN);
	m_type   = AnimalType::OCTOPUS;

	Initialize();

	m_vbo = g_theRenderer->CreateVertexBuffer(OCTO_MAX_VERTS * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	m_ibo = g_theRenderer->CreateIndexBuffer(OCTO_MAX_INDICES * sizeof(unsigned int), sizeof(unsigned int));
}

OctopusIK::~OctopusIK()
{
	delete m_vbo;
	delete m_ibo;
}

void OctopusIK::Initialize()
{
	float terrainZ = GetTerrainPoint(m_worldPosition).z;
	m_worldPosition.z = terrainZ + 0.5f;

	for (int tentacleIndex = 0; tentacleIndex < NUM_TENTACLES; ++tentacleIndex)
	{
		float tentacleAngle = (360.f / NUM_TENTACLES) * tentacleIndex;
		Vec3 tentacleDir = Vec3(CosDegrees(tentacleAngle), SinDegrees(tentacleAngle), -0.2f).GetNormalized();

		Tentacle tentacle;
		tentacle.m_rootLocal = tentacleDir * 0.5f;
		tentacle.m_phaseOffset = static_cast<float>(tentacleIndex) * 0.3f;

		Vec3 root = m_worldPosition + tentacle.m_rootLocal;
		int nodeCount = NODES_PER_TENTACLE;
		float segmentLength = TENTACLE_TOTAL_LENGTH / (float)(nodeCount - 1);
		for (int nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
		{
			Vec3 tentaclePosition = root + tentacleDir * segmentLength * (float)nodeIndex;

			float terrainZIs = GetTerrainPoint(tentaclePosition).z;
			if (tentaclePosition.z < terrainZIs + 0.02f)
			{
				tentaclePosition.z = terrainZIs + 0.02f;
			}

			tentacle.m_nodes.push_back(tentaclePosition);

			if (nodeIndex < nodeCount - 1)
			{
				tentacle.m_lengths.push_back(segmentLength);
			}
		}

		CreateTentacleSkeleton(tentacle, tentacleIndex);

		tentacle.m_desiredTargetPos = tentacle.m_nodes.back();
		tentacle.m_targetPos = tentacle.m_nodes.back();

		tentacle.m_cachedTerrainZ.resize(NODES_PER_TENTACLE);

		m_tentacles.push_back(tentacle);
	}
}

void OctopusIK::CreateTentacleSkeleton(Tentacle& tentacle, int tentacleIndex)
{
	tentacle.m_skeleton.m_bones.clear();

	for (int tentacleNodeIndex = 0; tentacleNodeIndex < NODES_PER_TENTACLE; ++tentacleNodeIndex)
	{
		Bone bone;
		bone.m_boneName = Stringf("Tentacle%d_Node%d", tentacleIndex, tentacleNodeIndex);

		bone.m_parentBoneIndex = 0;
		if (tentacleNodeIndex == 0)
		{
			bone.m_parentBoneIndex = -1;
		}
		else
		{
			bone.m_parentBoneIndex = tentacleNodeIndex - 1;
		}

		tentacle.m_skeleton.m_bones.push_back(bone);
	}

	tentacle.m_skeleton.UpdateSkeletonPose();

	// Setting the inverse bind pose
	for (int tentacleBoneIndex = 0; tentacleBoneIndex < static_cast<int>(tentacle.m_skeleton.m_bones.size()); ++tentacleBoneIndex)
	{
		Bone& bone = tentacle.m_skeleton.m_bones[tentacleBoneIndex];
		bone.m_bindWorldPosition = tentacle.m_nodes[tentacleBoneIndex];
		bone.m_inverseBindPose = bone.m_worldBoneTransform.GetOrthonormalInverse();
	}
}

void OctopusIK::Update(float deltaSeconds)
{
	if (g_theApp->m_gameClock->IsPaused())
	{
		return;
	}

	m_time += deltaSeconds;

	if (g_theInput->WasKeyJustPressed('O'))
	{
		m_debugDrawTargets = !m_debugDrawTargets;
	}

	// Reset check
	if (!m_animalMode->m_terrain->IsInBounds(static_cast<int>(m_worldPosition.x), static_cast<int>(m_worldPosition.y)))
	{
		float randomXLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
		float randomYLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
		m_worldPosition = Vec3(randomXLocation, randomYLocation, m_animalMode->m_terrain->GetHeightAtXY(randomXLocation, randomYLocation));
		ResetTentaclesToCurrentPosition();
	}

	UpdateTentacles(deltaSeconds);
	UpdateBody(deltaSeconds);
	UpdateVerts();
}

void OctopusIK::UpdateTentacles(float deltaSeconds)
{
	int anchors = 0;

	for (int tentacleIndex = 0; tentacleIndex < static_cast<int>(m_tentacles.size()); ++tentacleIndex)
	{
		Tentacle& tentacle = m_tentacles[tentacleIndex];
		Vec3 root = m_worldPosition + tentacle.m_rootLocal;

		// Decide our anchor
		Vec3 forward = m_roamDir; 
		if (forward.GetLengthSquared() < 0.001f)
		{
			forward = Vec3::XAXE;
		}

		float alignment = DotProduct3D(tentacle.m_rootLocal.GetNormalized(), forward);
		float frontFactor = GetClampedZeroToOne((alignment + 1.f) * 0.5f);

		if (!tentacle.m_isAnchored && anchors < 2)
		{
			// Push front tentacles further
			float dist = Interpolate(3.0f, 9.0f, frontFactor);

			// Stronger forward bias for front tentacles
			float forwardBias = Interpolate(0.2f, 0.8f, frontFactor);
			float heightBias = g_rng->RollRandomFloatInRange(-0.5f, 1.5f);

			Vec3  tentacleRadialDirection = tentacle.m_rootLocal.GetNormalized();
			Vec3  tentacleBlendedDirection = Interpolate(tentacleRadialDirection, forward, forwardBias).GetNormalized();

			float angleOffset = g_rng->RollRandomFloatInRange(-40.f, 40.f);
			Vec3  tentacleDirection = tentacleBlendedDirection.GetRotatedAboutZDegrees(angleOffset);

			Vec3 desired = m_worldPosition + tentacleDirection * dist + Vec3(0.f, 0.f, heightBias);

			tentacle.m_desiredTargetPos = GetTerrainPoint(desired);

			tentacle.m_isAnchored = true;
			tentacle.m_anchorTimer = 0.f;
			anchors++;
		}
		else if (!tentacle.m_isAnchored)
		{
			Vec3 tentacleBaseDirection = tentacle.m_rootLocal.GetNormalized();

			float wanderingAngle = sinf(m_time + tentacle.m_phaseOffset) * 45.f;
			float tentacleTilt = sinf(m_time * 0.7f + tentacle.m_phaseOffset) * 0.5f;

			Vec3 dir = tentacleBaseDirection.GetRotatedAboutZDegrees(wanderingAngle);
			dir.z += tentacleTilt;
			dir = dir.GetNormalized();

			float radius = g_rng->RollRandomFloatInRange(1.0f, 2.0f);
			Vec3 idleTarget = m_worldPosition + dir * radius;

			// Keeping tentacles above terrain
			float terrainZ = GetTerrainPoint(idleTarget).z;
			if (idleTarget.z < terrainZ + 0.3f)
			{
				idleTarget.z = terrainZ + 0.3f;
			}

			tentacle.m_desiredTargetPos = idleTarget;
		}

		// Releasing anchor here to prevent stiffness
		tentacle.m_anchorTimer += deltaSeconds;
		float anchorDuration = Interpolate(1.2f, 3.0f, frontFactor);

		if (tentacle.m_anchorTimer > anchorDuration)
		{
			tentacle.m_isAnchored = false;
		}


		// Dragging targets instead of stepping them
		float followSpeed = tentacle.m_isAnchored ? 1.f : 0.5f;
		tentacle.m_targetPos = Interpolate(tentacle.m_targetPos, tentacle.m_desiredTargetPos, deltaSeconds * followSpeed);

		for (int nodeIndex = 0; nodeIndex < static_cast<int>(tentacle.m_nodes.size()); ++nodeIndex)
		{
			Vec3 const& node = tentacle.m_nodes[nodeIndex];
			tentacle.m_cachedTerrainZ[nodeIndex] = m_animalMode->m_terrain->GetHeightAtXY(node.x, node.y);
		}

		SolveFABRIK(tentacle);

		// Tentacle wave motion
		Vec3 baseDir = (tentacle.m_nodes.back() - root).GetNormalized();
		Vec3 bendDir = CrossProduct3D(baseDir, Vec3::ZAXE).GetNormalized();

		for (int nodeIndex = 0; nodeIndex < static_cast<int>(tentacle.m_nodes.size()); ++nodeIndex)
		{
			float alpha = static_cast<float>(nodeIndex) / static_cast<float>((tentacle.m_nodes.size() - 1));
			float weight = alpha * alpha;

			float tentacleWave = sinf(m_time * 3.f + alpha * 4.f + tentacle.m_phaseOffset);
			tentacle.m_nodes[nodeIndex] += bendDir * tentacleWave * 0.02f * weight;
		}
	}
}

void OctopusIK::SolveFABRIK(Tentacle& tentacle)
{
	Vec3 root = m_worldPosition + tentacle.m_rootLocal;
	Vec3 target = tentacle.m_targetPos;

	for (int iterationIndex = 0; iterationIndex < MAX_ITERATIONS; ++iterationIndex)
	{
		tentacle.m_nodes.back() = target;
		for (int nodeIndex = static_cast<int>(tentacle.m_nodes.size() - 2); nodeIndex >= 0; --nodeIndex)
		{
			Vec3 tentacleDirection = (tentacle.m_nodes[nodeIndex] - tentacle.m_nodes[nodeIndex + 1]).GetNormalized();
			tentacle.m_nodes[nodeIndex] = tentacle.m_nodes[nodeIndex + 1] + tentacleDirection * tentacle.m_lengths[nodeIndex];
		}

		tentacle.m_nodes[0] = root;
		for (int nodeIndex = 1; nodeIndex < static_cast<int>(tentacle.m_nodes.size()); ++nodeIndex)
		{
			Vec3 tentacleDirection = (tentacle.m_nodes[nodeIndex] - tentacle.m_nodes[nodeIndex - 1]).GetNormalized();
			tentacle.m_nodes[nodeIndex] = tentacle.m_nodes[nodeIndex - 1] + tentacleDirection * tentacle.m_lengths[nodeIndex - 1];
		}
	}

	for (int nodeIndex = 0; nodeIndex < static_cast<int>(tentacle.m_nodes.size()); ++nodeIndex)
	{
		Vec3& node = tentacle.m_nodes[nodeIndex];
		float terrainZ = tentacle.m_cachedTerrainZ[nodeIndex];

		if (node.z < terrainZ + 0.02f)
		{
			node.z = terrainZ + 0.02f;
		}
	}
}

void OctopusIK::UpdateBody(float deltaSeconds)
{
	m_roamTimer -= deltaSeconds;

	if (m_roamTimer <= 0.f)
	{
		float angle = g_rng->RollRandomFloatInRange(0.f, 360.f);
		Vec3 newDir = Vec3(CosDegrees(angle), SinDegrees(angle), 0.f).GetNormalized();
		m_roamDir = Interpolate(m_roamDir, newDir, 0.5f);

		m_roamDuration = g_rng->RollRandomFloatInRange(6.5f, 11.0f);
		m_roamTimer = m_roamDuration;
	}

	Vec3 avg = Vec3::ZERO;
	float totalWeight = 0.f;

	Vec3 forward = m_roamDir;
	if (forward.GetLengthSquared() < 0.001f)
	{
		forward = Vec3::XAXE;
	}
	forward.Normalize();

	for (int tentacleIndex = 0; tentacleIndex < static_cast<int>(m_tentacles.size()); ++tentacleIndex)
	{
		Tentacle const& tentacle = m_tentacles[tentacleIndex];
		if (tentacle.m_isAnchored)
		{
			Vec3 tentacleDir = tentacle.m_rootLocal.GetNormalized();

			float alignment = DotProduct3D(tentacleDir, forward);
			float frontFactor = GetClampedZeroToOne((alignment + 1.f) * 0.5f);
			float weight = Interpolate(0.5f, 2.0f, frontFactor);

			avg += tentacle.m_targetPos * weight;
			totalWeight += weight;
		}
	}

	if (totalWeight > 0.f)
	{
		avg /= totalWeight;
		avg.z += BODY_RAISE;

		Vec3 toTarget = (avg - m_worldPosition);
		Vec3 desiredMove = toTarget + m_roamDir * 1.0f;

		m_velocity = Interpolate(m_velocity, desiredMove, deltaSeconds * 1.0f);
		if (m_velocity.GetLength() > OCTO_MAX_SPEED)
		{
			m_velocity = m_velocity.GetNormalized() * OCTO_MAX_SPEED;
		}

		m_worldPosition += m_velocity * deltaSeconds;
	}

	m_lookDir = Interpolate(m_lookDir, m_roamDir, deltaSeconds * BODY_TURN_SPEED);
	m_lookDir.Normalize();
}

void OctopusIK::UpdateVerts()
{
	m_verts.clear();
	m_indices.clear();

	// Body
	AddVertsForSphere3D(m_verts, m_indices, m_worldPosition, 0.6f, m_color);

	Vec3 forward = m_lookDir;
	if (forward.GetLengthSquared() < 0.001f)
	{
		forward = Vec3::XAXE;
	}
	forward.Normalize();

	Vec3 up = Vec3::ZAXE;
	if (fabsf(DotProduct3D(forward, up)) > 0.99f)
	{
		up = Vec3::YAXE;
	}

	Vec3 right = CrossProduct3D(forward, up).GetNormalized();
	up = CrossProduct3D(right, forward).GetNormalized();

	Vec3 leftEyeOffset = (-right * 0.25f) + (up * 0.3f);
	Vec3 rightEyeOffset = (right * 0.25f) + (up * 0.3f);
	Vec3 faceCenter = m_worldPosition + forward * 0.5f;

	// Eyes
	Vec3 leftEyePos = faceCenter + leftEyeOffset;
	Vec3 rightEyePos = faceCenter + rightEyeOffset;

	AddVertsForSphere3D(m_verts, m_indices, leftEyePos, 0.05f, Rgba8::BLACK);
	AddVertsForSphere3D(m_verts, m_indices, rightEyePos, 0.05f, Rgba8::BLACK);

	for (int tentacleIndex = 0; tentacleIndex < static_cast<int>(m_tentacles.size()); ++tentacleIndex)
	{
		Tentacle const& tentacle = m_tentacles[tentacleIndex];
		AddVertsForSphere3D(m_verts, m_indices, tentacle.m_nodes[0], 0.15f, m_color);
		for (int nodeIndex = 0; nodeIndex < static_cast<int>(tentacle.m_nodes.size() - 1); ++nodeIndex)
		{
			Vec3 p0 = tentacle.m_nodes[nodeIndex];
			Vec3 p1 = tentacle.m_nodes[nodeIndex + 1];

			Vec3 dir = (p1 - p0).GetNormalized();
			float overlap = 0.03f;

			Vec3 start = p0 - dir * overlap;
			Vec3 end = p1 + dir * overlap;

			float r0 = GetRadius(nodeIndex, static_cast<int>(tentacle.m_nodes.size()));
			float r1 = GetRadius(nodeIndex + 1, static_cast<int>(tentacle.m_nodes.size()));

			int sides = (r0 > 0.08f) ? 12 : 6; 
			AddVertsForIndexedTaperedCylinder3D(m_verts, m_indices, start, end, r0, r1, m_color, AABB2::ZERO_TO_ONE, sides, true);
		}

		if (m_animalMode->m_isSkeletonBeingDrawn)
		{
			for (int nodeIndex = 0; nodeIndex < static_cast<int>(tentacle.m_nodes.size() - 1); ++nodeIndex)
			{
				Vec3 p0 = tentacle.m_nodes[nodeIndex];
				Vec3 p1 = tentacle.m_nodes[nodeIndex + 1];

				// Bones
				AddVertsForIndexedCylinder3D(m_verts, m_indices, p0, p1, 0.02f, Rgba8::WHITE, AABB2::ZERO_TO_ONE, 16, false);

				// Joints
				AddVertsForSphere3D(m_verts, m_indices, p0, 0.04f, Rgba8::WHITE);
				AddVertsForSphere3D(m_verts, m_indices, p1, 0.04f, Rgba8::WHITE);
			}
		}
	}

	if (m_debugDrawTargets)
	{
		for (int tentacleIndex = 0; tentacleIndex < static_cast<int>(m_tentacles.size()); ++tentacleIndex)
		{
			Tentacle const& tentacle = m_tentacles[tentacleIndex];
			Rgba8 color = tentacle.m_isAnchored ? Rgba8::GREEN : Rgba8::RED;
			AddVertsForSphere3D(m_verts, m_indices, tentacle.m_targetPos, 0.08f, color);
		}
	}

	UpdateGPUBuffer();
}

float OctopusIK::GetRadius(int nodeIndex, int nodeCount)
{
	float t = static_cast<float>(nodeIndex) / static_cast<float>((nodeCount - 1));
	return Interpolate(BASE_RADIUS, TIP_RADIUS, t);
}

void OctopusIK::Render() const
{
	if (m_animalMode->m_isAnimalVertsBeingDrawn)
	{
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetLightingConstants(m_sunDirection, OCTO_SUN_INTENSITY, OCTO_AMBIENT_INTENSITY);
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
		if (m_animalMode->m_isSkeletonBeingDrawn)
		{
			g_theRenderer->SetDepthMode(DepthMode::DISABLED);
		}
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->BindShader(m_shader);
		g_theRenderer->DrawIndexedVertexBuffer(m_vbo, m_ibo, static_cast<unsigned int>(m_indices.size()));
	}
}

void OctopusIK::UpdateGPUBuffer()
{
	g_theRenderer->CopyCPUToGPU(m_verts.data(), static_cast<unsigned int>(m_verts.size()) * sizeof(Vertex_PCUTBN), m_vbo);
	g_theRenderer->CopyCPUToGPU(m_indices.data(), static_cast<unsigned int>(m_indices.size()) * sizeof(unsigned int), m_ibo);
}

Vec3 OctopusIK::GetTerrainPoint(Vec3 const& worldPos) const
{
	Terrain* terrain = m_animalMode->m_terrain;
	float terrainHeight = terrain->GetHeightAtXY(worldPos.x, worldPos.y);
	return Vec3(worldPos.x, worldPos.y, terrainHeight);
}

void OctopusIK::ResetTentaclesToCurrentPosition()
{
	for (int tentacleIndex = 0; tentacleIndex < static_cast<int>(m_tentacles.size()); ++tentacleIndex)
	{
		Tentacle& tentacle = m_tentacles[tentacleIndex];

		Vec3 root = m_worldPosition + tentacle.m_rootLocal;
		Vec3 dir = tentacle.m_rootLocal.GetNormalized();

		for (int nodeIndex = 0; nodeIndex < static_cast<int>(tentacle.m_nodes.size()); ++nodeIndex)
		{
			float segmentLength = TENTACLE_TOTAL_LENGTH / (float)(tentacle.m_nodes.size() - 1);
			Vec3 pos = root + dir * segmentLength * (float)nodeIndex;

			float terrainZ = GetTerrainPoint(pos).z;
			if (pos.z < terrainZ + 0.02f)
			{
				pos.z = terrainZ + 0.02f;
			}

			tentacle.m_nodes[nodeIndex] = pos;
		}

		// Reset target to tip
		Vec3 tip = tentacle.m_nodes.back();
		tentacle.m_targetPos = tip;
		tentacle.m_desiredTargetPos = tip;

		// Reset anchor state
		tentacle.m_isAnchored = false;
		tentacle.m_anchorTimer = 0.f;
	}
}