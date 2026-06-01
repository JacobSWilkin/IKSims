#include "Game/Octopus.hpp"
#include "Game/GameCommon.h"
#include "Game/Terrain.hpp"
#include "Game/AnimalMode.hpp"
#include "Engine/Input/InputSystem.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Math/MathUtils.h"

Octopus::Octopus(AnimalMode* mode, Vec3 position)
	:Entity(mode, position)
{
	m_octopusLit = g_theRenderer->CreateOrGetShader("Data/Shaders/BlinnPhong_Octopus", VertexType::VERTEX_PCUTBN);
	Initialize();
	UpdateOctopusVerts();
	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(MAX_OCTOPUS_VERTS * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	m_indexBuffer = g_theRenderer->CreateIndexBuffer(MAX_OCTOPUS_INDICES * sizeof(unsigned int), sizeof(unsigned int));
}

Octopus::~Octopus()
{
	DeleteBuffers();
}

void Octopus::Initialize()
{
	m_octopus = CreateOctopusSkeleton();

	m_octopusInitialPositions.clear();
	for (int octopusBoneIndex = 0; octopusBoneIndex < static_cast<int>(m_octopus.m_bones.size()); ++octopusBoneIndex)
	{
		Bone& octopusBone = m_octopus.m_bones[octopusBoneIndex];
		m_octopusInitialPositions.push_back(octopusBone.m_localPosition);
	}

	for (int armIndex = 0; armIndex < NUM_OCTOPUS_ARMS; ++armIndex)
	{
		int startIndex = 1 + armIndex * NUM_ARM_SEGMENTS;

		std::vector<int> chain;
		for (int chainIndex = 0; chainIndex < NUM_ARM_SEGMENTS; ++chainIndex)
		{
			chain.push_back(startIndex + chainIndex);
		}

		m_armChains.push_back(chain);
	}

	for (int armIndex = 0; armIndex < NUM_OCTOPUS_ARMS; ++armIndex)
	{
		m_armPhaseOffsets.push_back(g_rng->RollRandomFloatInRange(0.f, 6.28f));
		m_armAmplitude.push_back(g_rng->RollRandomFloatInRange(0.4f, 0.8f));
	}
}

void Octopus::Update(float deltaSeconds)
{
	m_octoDebugVerts.clear();
	if (g_theInput->WasKeyJustPressed('O'))
	{
		m_isDebugTargetsOn = !m_isDebugTargetsOn;
	}

	UpdateOctopusPose(deltaSeconds);
	UpdateOctopusVerts();
}

void Octopus::UpdateOctopusPose(float deltaSeconds)
{
	static float elapsedTime = 0.f;
	elapsedTime += deltaSeconds;
	m_directionChangeTimer += deltaSeconds;

	if (!m_isStationary)
	{
		// Direction change
		OctopusRoam(deltaSeconds);

		// Move octopus forward
		Vec3 octopusMove = m_moveDirection * m_speed * deltaSeconds;
		m_worldPosition -= octopusMove;

		// Project onto terrain
		float terrainZ = m_animalMode->m_terrain->GetHeightAtXY(m_worldPosition.x, m_worldPosition.y);
		m_worldPosition.z = terrainZ;

		// Check if octopus is out of bounds, if so reset
		if (!m_animalMode->m_terrain->IsInBounds(static_cast<int>(m_worldPosition.x), static_cast<int>(m_worldPosition.y)))
		{
			float randomXLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
			float randomYLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
			m_worldPosition = Vec3(randomXLocation, randomYLocation, m_animalMode->m_terrain->GetHeightAtXY(randomXLocation, randomYLocation));
			elapsedTime = 0.f;
		}
	}

	SolveTerrainAlignment();
	ResetBindPose();

	// Arm curling
	for (int arm = 0; arm < NUM_OCTOPUS_ARMS; ++arm)
	{
		std::vector<int> const& chain = m_armChains[arm];
		int baseIndex = chain.front();
		int tipIndex = chain.back();

		Vec3 baseWorld = m_octopus.m_bones[baseIndex].GetWorldBonePosition3D();
		Vec3 tipWorld = m_octopus.m_bones[tipIndex].GetWorldBonePosition3D();

		// Direction the arm naturally points
		Vec3 armDir = (tipWorld - baseWorld).GetNormalized();
		Vec3 sideways = CrossProduct3D(armDir, Vec3::ZAXE).GetNormalized();

		float noiseTime = elapsedTime * 0.4f;

		// Gentle smooth noise
		float ampNoise = sinf(noiseTime + arm * 10.f);
		ampNoise = ampNoise * 2.f - 1.f;
		float dynamicAmplitude = m_armAmplitude[arm] * (1.0f + ampNoise * 0.4f);

		float wave = sinf(elapsedTime * SWIM_TEMPO + m_armPhaseOffsets[arm]);
		Vec3 targetPos = tipWorld + sideways * wave * dynamicAmplitude;

		SolveCCDIK(chain, targetPos, 5, 0.01f);

		if (m_isDebugTargetsOn) 
		{
			AddVertsForSphere3D(m_octoDebugVerts, targetPos, 0.1f, Rgba8::GREEN);
		}
	}
}

void Octopus::ResetBindPose()
{
	for (int boneIndex = 1; boneIndex < static_cast<int>(m_octopus.m_bones.size()); ++boneIndex)
	{
		m_octopus.m_bones[boneIndex].SetLocalBoneRotation(Quat::DEFAULT);
	}
	m_octopus.UpdateSkeletonPose();
}

void Octopus::SolveTerrainAlignment()
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

	// Apply it to the skeleton
	m_octopus.m_skeletonModelTransform = orientation;
	m_octopus.UpdateSkeletonPose();
}

Vec3 Octopus::GetTerrainPoint(Vec3 const& worldPos) const
{
	Terrain* terrain = m_animalMode->m_terrain;
	float height = terrain->GetHeightAtXY(worldPos.x, worldPos.y);
	return Vec3(worldPos.x, worldPos.y, height);
}

void Octopus::UpdateOctopusVerts()
{
	if (m_animalMode->m_isSkeletonBeingDrawn)
	{
		m_octoSkeletonVerts.clear();
		m_octopus.AddVertsForSkeleton3D(m_octoSkeletonVerts);
	}
	m_octoTBNVerts.clear();
	m_octoIndices.clear();

	// HEAD
	Bone const& head = m_octopus.m_bones[0];
	Vec3 headPos = head.GetWorldBonePosition3D();

	Vec3 iBasis = head.m_worldBoneTransform.GetIBasis3D();
	Vec3 jBasis = head.m_worldBoneTransform.GetJBasis3D();
	Vec3 kBasis = head.m_worldBoneTransform.GetKBasis3D();

	// Mantle
	AddVertsForSphere3D(m_octoTBNVerts, m_octoIndices, headPos - (jBasis * 0.2f) + (kBasis * 0.2f), 0.35f, Rgba8(200, 60, 200));

	// Head
	AddVertsForSphere3D(m_octoTBNVerts, m_octoIndices, headPos, 0.35f, Rgba8(200, 60, 200));

	// Eyes
	Vec3 eyeOffset = (iBasis * 0.30f + kBasis * 0.f);
	AddVertsForSphere3D(m_octoTBNVerts, m_octoIndices, headPos - eyeOffset + (kBasis * 0.2f) + (jBasis * 0.1f) + (iBasis * 0.1f), 0.12f, Rgba8::BLACK);
	AddVertsForSphere3D(m_octoTBNVerts, m_octoIndices, headPos + eyeOffset + (kBasis * 0.2f) + (jBasis * 0.1f) - (iBasis * 0.1f), 0.12f, Rgba8::BLACK);

	// ARMS
	for (int boneIndex = 1; boneIndex < static_cast<int>(m_octopus.m_bones.size()); ++boneIndex)
	{
		Bone const& bone = m_octopus.m_bones[boneIndex];

		Vec3 start = bone.GetWorldBonePosition3D();
		Vec3 end = start;

		if (bone.m_parentBoneIndex >= 0)
		{
			Bone const& parent = m_octopus.m_bones[bone.m_parentBoneIndex];
			end = parent.GetWorldBonePosition3D();
		}

		bool isTip = m_isTipBone[boneIndex];

		if (isTip)
		{
			AddVertsForIndexedCone3D(m_octoTBNVerts, m_octoIndices, end, start, 0.08f, Rgba8(160, 30, 160));
		}
		else
		{
			AddVertsForSphere3D(m_octoTBNVerts, m_octoIndices, end, 0.0775f, Rgba8(150, 35, 150));
			AddVertsForIndexedCylinder3D(m_octoTBNVerts, m_octoIndices, end, start, 0.08f, Rgba8(150, 35, 150), AABB2::ZERO_TO_ONE, 8, false);
		}
	}

	UpdateGPUBuffer();
}

void Octopus::UpdateGPUBuffer()
{
	if (!m_vertexBuffer)
	{
		return;
	}

	unsigned int sizeInBytes = static_cast<unsigned int>(m_octoTBNVerts.size()) * sizeof(Vertex_PCUTBN);
	g_theRenderer->CopyCPUToGPU(m_octoTBNVerts.data(), sizeInBytes, m_vertexBuffer);

	unsigned int indexBytes = static_cast<unsigned int>(m_octoIndices.size()) * sizeof(unsigned int);
	g_theRenderer->CopyCPUToGPU(m_octoIndices.data(), indexBytes, m_indexBuffer);
}

void Octopus::DeleteBuffers()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;
}

void Octopus::Render() const
{
	DrawOctopus();

	if (m_animalMode->m_isSkeletonBeingDrawn)
	{
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::DISABLED);
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(m_octoSkeletonVerts);
	}

	if (m_isDebugTargetsOn)
	{
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(m_octoDebugVerts);
	}
}

Skeleton Octopus::CreateOctopusSkeleton()
{
	Skeleton octopusSkeleton;
	octopusSkeleton.m_bones.clear();

	// Central head
	Bone head;
	head.m_boneName = "Head";
	head.SetLocalBonePosition(Vec3(0.f, 0.f, 0.75f));
	head.SetLocalBoneRotation(Quat::MakeFromAxisAngle(Vec3::ZAXE, ConvertDegreesToRadians(90.f)));
	m_isTipBone.push_back(false);
	octopusSkeleton.m_bones.push_back(head);

	//Bone mantleBulb;

	// Separating the octopus arms by 45 degrees
	float angleStep = 360.f / NUM_OCTOPUS_ARMS;
	float armLength = 3.f;

	float segmentLength = armLength / static_cast<float>(NUM_ARM_SEGMENTS);

	for (int armIndex = 0; armIndex < NUM_OCTOPUS_ARMS; ++armIndex)
	{
		float armAngleDegrees = armIndex * angleStep;
		float armAngleRadians = ConvertDegreesToRadians(armAngleDegrees);

		// Base direction
		float x = cosf(armAngleRadians);
		float y = sinf(armAngleRadians);

		// Offset adjustment
		Vec3 baseOffset = Vec3(x, y, -0.2f);
		Vec3 middleOffset = baseOffset * 0.7f;
		Vec3 tipOffset = baseOffset * 0.7f;

		int parentIndex = 0;

		Vec3 dir = Vec3(x, y, -0.2f).GetNormalized();

		for (int segment = 0; segment < NUM_ARM_SEGMENTS; ++segment)
		{
			Bone bone;
			bone.m_boneName = "Arm" + std::to_string(armIndex + 1) + "_Seg" + std::to_string(segment);
			bone.m_parentBoneIndex = parentIndex;
			bone.SetLocalBonePosition(dir * segmentLength);
			bool isTip = (segment == NUM_ARM_SEGMENTS - 1);
			m_isTipBone.push_back(isTip);
			octopusSkeleton.m_bones.push_back(bone);
			parentIndex = static_cast<int>(octopusSkeleton.m_bones.size() - 1);
		}
	}

	octopusSkeleton.UpdateSkeletonPose();
	return octopusSkeleton;
}

void Octopus::DrawOctopus() const
{
	if (!m_animalMode->m_isAnimalVertsBeingDrawn)
	{
		return;
	}

	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindShader(m_octopusLit);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, static_cast<unsigned int>(m_octoIndices.size()));
}

void Octopus::OctopusRoam(float deltaSeconds)
{
	if (m_directionChangeTimer >= OCTOPUS_DIRECTION_TIME)
	{
		m_directionChangeTimer = 0.f;

		// Picking a new random direction for the octopus to go
		float angle = g_rng->RollRandomFloatInRange(0.f, 360.f);
		Vec2 directionXY = Vec2(CosDegrees(angle), SinDegrees(angle));
		Vec3 newDirection = Vec3(directionXY.x, directionXY.y, 0.f).GetNormalized();
		m_targetMoveDirection = newDirection;
		m_directionInterpTime = 0.f;
	}

	// Smoothly interpolate move direction
	if (m_moveDirection != m_targetMoveDirection)
	{
		m_directionInterpTime += deltaSeconds;
		float fractionTowardEnd = GetClamped(m_directionInterpTime / OCTOPUS_INTERP_DURATION, 0.f, 1.f);

		float easedFraction = SmoothStep3(fractionTowardEnd);
		m_moveDirection = SLerp(m_moveDirection, m_targetMoveDirection, easedFraction).GetNormalized();
	}
}

void Octopus::SolveCCDIK(std::vector<int> const& chainIndices, Vec3 const& targetPosition, int maxIterations, float threshold)
{
	// Check if there are enough bones for a chain
	if (chainIndices.size() < 2)
	{
		return;
	}

	float totalChainLength = 0.f;

	for (int chainIndex = 0; chainIndex < static_cast<int>(chainIndices.size() - 1); ++chainIndex)
	{
		int chainIndexA = chainIndices[chainIndex];
		int chainIndexB = chainIndices[chainIndex + 1];
		Vec3 chainIndexAPosition = m_octopus.m_bones[chainIndexA].GetWorldBonePosition3D();
		Vec3 chainIndexBPosition = m_octopus.m_bones[chainIndexB].GetWorldBonePosition3D();
		totalChainLength += (chainIndexBPosition - chainIndexAPosition).GetLength();
	}

	// Clamp if target is unreachable
	Vec3  rootPosition = m_octopus.m_bones[chainIndices[0]].GetWorldBonePosition3D();
	float distToTarget = (targetPosition - rootPosition).GetLength();

	Vec3 clampedTargetPos = targetPosition;
	bool targetUnreachable = false;
	if (distToTarget > totalChainLength)
	{
		targetUnreachable = true;
		Vec3 direction = (targetPosition - rootPosition).GetNormalized();
		clampedTargetPos = rootPosition + direction * totalChainLength;
	}

	distToTarget = (clampedTargetPos - rootPosition).GetLength();

	if (targetUnreachable)
	{
		Vec3 direction = (targetPosition - rootPosition).GetNormalized();

		// Straightening the chain in the target direction
		for (int chainIndex = 0; chainIndex < static_cast<int>(chainIndices.size()) - 1; ++chainIndex)
		{
			int jointIndex = chainIndices[chainIndex];
			int nextIndex = chainIndices[chainIndex + 1];

			Vec3 jointPos = m_octopus.m_bones[jointIndex].GetWorldBonePosition3D();
			Vec3 nextPos = m_octopus.m_bones[nextIndex].GetWorldBonePosition3D();
			Vec3 toNext = (nextPos - jointPos).GetNormalized();

			float dot = DotProduct3D(toNext, direction);
			dot = GetClamped(dot, -1.f, 1.f);
			float angle = acosf(dot);

			if (angle > 0.0001f)
			{
				Vec3 axis = CrossProduct3D(toNext, direction);
				if (axis.GetLengthSquared() > 0.00001f)
				{
					axis.Normalize();
					Quat rotation = Quat::MakeFromAxisAngle(axis, angle);

					Quat parentWorldRot = Quat::DEFAULT;
					int parentIndex = m_octopus.m_bones[jointIndex].m_parentBoneIndex;
					if (parentIndex != -1)
					{
						parentWorldRot = m_octopus.m_bones[parentIndex].GetWorldBoneRotation3D();
					}

					// Converting world delta to local delta
					Quat localDelta = parentWorldRot.QuatInverse() * rotation * parentWorldRot;

					// Apply to local rotation
					Quat newLocalRot = localDelta * m_octopus.m_bones[jointIndex].m_localRotation;
					m_octopus.m_bones[jointIndex].SetLocalBoneRotation(newLocalRot);
				}
			}
		}

		m_octopus.UpdateSkeletonPose();
		return;
	}

	for (int iterationIndex = 0; iterationIndex < maxIterations; ++iterationIndex)
	{
		bool breakLoop = false;

		for (int chainIndex = static_cast<int>(chainIndices.size()) - 2; chainIndex >= 0; --chainIndex)
		{
			int jointIndex = chainIndices[chainIndex];
			int endEffectorIndex = chainIndices.back();

			Vec3 jointPos = m_octopus.m_bones[jointIndex].GetWorldBonePosition3D();
			Vec3 endEffectorPos = m_octopus.m_bones[endEffectorIndex].GetWorldBonePosition3D();

			Vec3 toEndEffector = endEffectorPos - jointPos;
			Vec3 toTarget = clampedTargetPos - jointPos;

			if (toEndEffector.GetLengthSquared() < 0.00001f || toTarget.GetLengthSquared() < 0.00001f)
			{
				continue;
			}

			toEndEffector.Normalize();
			toTarget.Normalize();

			// Compute rotation to align end effector to target
			float effectorTargetDot = DotProduct3D(toEndEffector, toTarget);
			effectorTargetDot = GetClamped(effectorTargetDot, -1.f, 1.f);
			float angle = acosf(effectorTargetDot);

			// Stop if already fully extended and aligned
			if (effectorTargetDot > 0.9999f && distToTarget > totalChainLength - 0.0001f)
			{
				breakLoop = true;
				break;
			}
			// Rotate to target if there is an angle greater than our threshold
			else if (angle > 0.001f)
			{
				Vec3 rotationAxisWorld = CrossProduct3D(toEndEffector, toTarget);

				if (rotationAxisWorld.GetLengthSquared() > 0.00001f)
				{
					rotationAxisWorld.Normalize();

					// Convert world axis into joint local space
					rotationAxisWorld.Normalize();
					Quat rotationQuat = Quat::MakeFromAxisAngle(rotationAxisWorld, angle);

					Quat parentWorldRot = Quat::DEFAULT;
					int parentIndex = m_octopus.m_bones[jointIndex].m_parentBoneIndex;
					if (parentIndex != -1)
					{
						parentWorldRot = m_octopus.m_bones[parentIndex].GetWorldBoneRotation3D();
					}

					// Converting world delta to local delta
					Quat localDelta = parentWorldRot.QuatInverse() * rotationQuat * parentWorldRot;

					// Apply to local rotation
					Quat newLocalRot = localDelta * m_octopus.m_bones[jointIndex].m_localRotation;
					m_octopus.m_bones[jointIndex].SetLocalBoneRotation(newLocalRot);

				}
			}
		}
		m_octopus.UpdateSkeletonPose(); 

		// Breaking out of outer loop
		if (breakLoop)
		{
			break;
		}

		// Convergence check
		Vec3 newEffectorPos = m_octopus.m_bones[chainIndices.back()].GetWorldBonePosition3D();
		if ((newEffectorPos - clampedTargetPos).GetLength() <= threshold)
		{
			break;
		}
	}
}