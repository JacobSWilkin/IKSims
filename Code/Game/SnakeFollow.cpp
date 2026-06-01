#include "Game/SnakeFollow.hpp"
#include "Game/AnimalMode.hpp"
#include "Game/Terrain.hpp"
#include "Game/GameCommon.h"
#include "Engine/Renderer/Renderer.h"

SnakeFollow::SnakeFollow(AnimalMode* animalMode, Vec3 position, Rgba8 color)
	:Entity(animalMode, position), m_snakeColor(color)
{
	m_snakeLit = g_theRenderer->CreateOrGetShader("Data/Shaders/BlinnPhong_Snake", VertexType::VERTEX_PCUTBN);
	m_skinningBuffer = g_theRenderer->CreateConstantBuffer(sizeof(SkinningConstants));
	m_snakeTexture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/snakeskin.jpg");
	m_type = AnimalType::SNAKE;

	Initialize();
	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(MAX_SNAKE_VERTS * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	UpdateVerts();
}

SnakeFollow::~SnakeFollow()
{
	DeleteBuffers();

	delete m_skinningBuffer;
	m_skinningBuffer = nullptr;
}

void SnakeFollow::Initialize()
{
	m_snakeFollowSkeleton = CreateSkeleton();
}

void SnakeFollow::Update(float deltaSeconds)
{
	UpdateSnakeFollowPose(deltaSeconds);
	SolveTerrainAlignment();
	UpdateVerts();
}

void SnakeFollow::Render() const
{
	DrawSnake();
}

Skeleton SnakeFollow::CreateSkeleton()
{
	Skeleton skeleton;
	skeleton.m_bones.clear();

	for (int segmentIndex = 0; segmentIndex < SNAKE_SEGMENT_COUNT; ++segmentIndex)
	{
		Bone bone;
		bone.m_boneName = Stringf("Segment%d", segmentIndex);
		if (segmentIndex == 0)
		{
			bone.m_parentBoneIndex = -1;
		}
		else
		{
			bone.m_parentBoneIndex = segmentIndex - 1;
		}
		skeleton.m_bones.push_back(bone);
	}

	skeleton.UpdateSkeletonPose();

	for (int boneIndex = 0; boneIndex < static_cast<int>(skeleton.m_bones.size()); ++boneIndex)
	{
		Bone& bone = skeleton.m_bones[boneIndex];
		bone.m_bindWorldPosition = bone.m_worldBoneTransform.GetTranslation3D();
		bone.m_inverseBindPose = bone.m_worldBoneTransform.GetOrthonormalInverse();
	}

	return skeleton;
}

void SnakeFollow::DrawSnake() const
{
	if (m_animalMode->m_isAnimalVertsBeingDrawn)
	{
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

		// Drawing textured
		g_theRenderer->BindShader(m_snakeLit);
		g_theRenderer->BindTexture(m_snakeTexture);
		g_theRenderer->DrawVertexBuffer(m_vertexBuffer, static_cast<unsigned int>(m_snakeTBNVerts.size()));

		// Drawing untextured
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(m_snakeVertsFollowUntextured);
	}
}

void SnakeFollow::UpdateSnakeFollowPose(float deltaSeconds)
{
	static float elapsedTime = 0.f;
	elapsedTime += deltaSeconds;

	m_timeUntilTurn -= deltaSeconds;

	if (m_timeUntilTurn <= 0.f)
	{
		// Pick a new random turn (left or right)
		m_targetTurnAngle = g_rng->RollRandomFloatInRange(-45.f, 45.f);

		// How long to apply this turn
		m_timeUntilTurn = g_rng->RollRandomFloatInRange(1.0f, 3.0f);
	}

	// Apply gradual turning
	float turnThisFrame = m_targetTurnAngle * deltaSeconds;
	m_moveDirection = m_moveDirection.GetRotatedAboutZDegrees(turnThisFrame);
	m_moveDirection.Normalize();

	Vec3 snakeMove = -m_moveDirection * m_speed * deltaSeconds;
	m_worldPosition += snakeMove;

	if (!m_isStationary)
	{
		// Project onto terrain
		float terrainZ = m_animalMode->m_terrain->GetHeightAtXY(m_worldPosition.x, m_worldPosition.y);
		m_worldPosition.z = terrainZ;

		// Head driving the directional change
		{
			Bone& head = m_snakeFollowSkeleton.m_bones[0];

			Vec3 forward = m_moveDirection;
			Vec3 worldUp = Vec3::ZAXE;

			if (fabsf(DotProduct3D(forward, worldUp)) > 0.99f)
			{
				worldUp = Vec3::YAXE;
			}

			Vec3 left = CrossProduct3D(worldUp, forward).GetNormalized();
			Vec3 up = CrossProduct3D(forward, left).GetNormalized();

			Mat44 basis;
			basis.SetIJK3D(forward, left, up);
			basis.SetTranslation3D(m_worldPosition);

			head.m_worldBoneTransform = basis;
		}

		// Body segments follow previous segment on constraint chain
		for (int snakeBoneIndex = 1; snakeBoneIndex < static_cast<int>(m_snakeFollowSkeleton.m_bones.size()); ++snakeBoneIndex)
		{
			Bone& prev = m_snakeFollowSkeleton.m_bones[snakeBoneIndex - 1];
			Bone& curr = m_snakeFollowSkeleton.m_bones[snakeBoneIndex];

			Vec3 prevPos = prev.m_worldBoneTransform.GetTranslation3D();
			Vec3 currPos = curr.m_worldBoneTransform.GetTranslation3D();

			Vec3 toPrev = prevPos - currPos;
			float dist = toPrev.GetLength();

			if (dist > 0.0001f)
			{
				Vec3 boneDirection = toPrev / dist;
				Vec3 newPosition = prevPos - boneDirection * SNAKE_SEGMENT_LENGTH;

				Vec3 worldUp = Vec3::ZAXE;
				if (fabsf(DotProduct3D(boneDirection, worldUp)) > 0.99f)
				{
					worldUp = Vec3::YAXE;
				}

				Vec3 left = CrossProduct3D(worldUp, boneDirection).GetNormalized();
				Vec3 up = CrossProduct3D(boneDirection, left).GetNormalized();

				Mat44 basis;
				basis.SetIJK3D(boneDirection, left, up);
				basis.SetTranslation3D(newPosition);

				curr.m_worldBoneTransform = basis;
			}
		}

		// Reset if out of bounds
		if (!m_animalMode->m_terrain->IsInBounds(static_cast<int>(m_worldPosition.x), static_cast<int>(m_worldPosition.y)))
		{
			float randomXLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
			float randomYLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
			m_worldPosition = Vec3(randomXLocation, randomYLocation, m_animalMode->m_terrain->GetHeightAtXY(randomXLocation, randomYLocation));
			elapsedTime = 0.f;
		}
	}
}

void SnakeFollow::SolveTerrainAlignment()
{
	float offset = 1.f;
	Terrain* terrain = m_animalMode->m_terrain;

	// Get current and neighbor positions
	float hCenter = terrain->GetHeightAtXY(m_worldPosition.x, m_worldPosition.y);
	float hForward = terrain->GetHeightAtXY(m_worldPosition.x + m_moveDirection.x * offset, m_worldPosition.y + m_moveDirection.y * offset);
	float hLeft = terrain->GetHeightAtXY(m_worldPosition.x + m_moveDirection.y * offset, m_worldPosition.y - m_moveDirection.x * offset);

	// Move entire skeleton to world position
	Vec3 forward = (Vec3(m_moveDirection.x, m_moveDirection.y, hForward - hCenter)).GetNormalized();
	Vec3 left = (Vec3(m_moveDirection.y, -m_moveDirection.x, hLeft - hCenter)).GetNormalized();
	Vec3 up = CrossProduct3D(left, forward).GetNormalized();
	left = CrossProduct3D(forward, up).GetNormalized();

	Mat44 rotation;
	rotation.SetIJK3D(forward, left, up);

	Vec3 headLocalPos = m_snakeFollowSkeleton.m_bones[0].m_localPosition;
	Vec3 headWorldOffset = rotation.TransformPosition3D(headLocalPos);

	Vec3 skeletonHeadOrigin = m_worldPosition - headWorldOffset;
	rotation.SetTranslation3D(skeletonHeadOrigin);

	// Push bone down to ground and align with terrain
	for (int boneIndex = 0; boneIndex < static_cast<int>(m_snakeFollowSkeleton.m_bones.size()); ++boneIndex)
	{
		Bone& bone = m_snakeFollowSkeleton.m_bones[boneIndex];
		Vec3 worldPos = bone.m_worldBoneTransform.GetTranslation3D();
		float terrainZ = terrain->GetHeightAtXY(worldPos.x, worldPos.y);

		worldPos.z = terrainZ;

		Mat44 mat = bone.m_worldBoneTransform;
		mat.SetTranslation3D(worldPos);
		bone.m_worldBoneTransform = mat;
	}
}

void SnakeFollow::UpdateVerts()
{
	m_snakeTBNVerts.clear();
	m_snakeVertsFollowUntextured.clear();

	for (int boneIndex = 0; boneIndex < static_cast<int>(m_snakeFollowSkeleton.m_bones.size()); ++boneIndex)
	{
		Bone const& bone = m_snakeFollowSkeleton.m_bones[boneIndex];
		Vec3 worldPosition = bone.m_worldBoneTransform.GetTranslation3D();
		float radius = 0.25f;

		// HEAD
		if (boneIndex == 0)
		{
			// Head sphere
			AddVertsForSphere3D(m_snakeTBNVerts, worldPosition, 0.3f, m_snakeColor);

			// Eyes offset from head position
			Vec3 forward = bone.m_worldBoneTransform.GetIBasis3D();
			Vec3 left = bone.m_worldBoneTransform.GetJBasis3D();
			Vec3 up = bone.m_worldBoneTransform.GetKBasis3D();

			// Positioning the eyes
			Vec3 leftEyePosition = worldPosition - forward * 0.2f + left * 0.1f + up * 0.25f;
			Vec3 rightEyePosition = worldPosition - forward * 0.2f - left * 0.1f + up * 0.25f;

			// White eye bases
			AddVertsForSphere3D(m_snakeVertsFollowUntextured, leftEyePosition, 0.05f, Rgba8::WHITE);
			AddVertsForSphere3D(m_snakeVertsFollowUntextured, rightEyePosition, 0.05f, Rgba8::WHITE);

			// Black pupils
			Vec3 pupilOffset = forward * 0.03f;
			AddVertsForSphere3D(m_snakeVertsFollowUntextured, leftEyePosition - pupilOffset, 0.025f, Rgba8::BLACK);
			AddVertsForSphere3D(m_snakeVertsFollowUntextured, rightEyePosition - pupilOffset, 0.025f, Rgba8::BLACK);
		}
		// TAIL
		else if (boneIndex == static_cast<int>(m_snakeFollowSkeleton.m_bones.size()) - 1)
		{
			Vec3 tailTip = worldPosition;
			Vec3 tailBase = m_snakeFollowSkeleton.m_bones[boneIndex - 1].m_worldBoneTransform.GetTranslation3D();

			AddVertsForConeTBN3D(m_snakeTBNVerts, tailBase, tailTip, radius, m_snakeColor, AABB2::ZERO_TO_ONE, 16, false);
		}
		// BODY SEGMENTS
		else
		{
			AddVertsForSphere3D(m_snakeTBNVerts, worldPosition, radius, m_snakeColor);

			if (boneIndex == 0)
			{
				continue;
			}

			Vec3 start = m_snakeFollowSkeleton.m_bones[boneIndex - 1].m_worldBoneTransform.GetTranslation3D();
			Vec3 end = bone.m_worldBoneTransform.GetTranslation3D();

			AddVertsForCylinder3D(m_snakeTBNVerts, start, end, radius, m_snakeColor, AABB2::ZERO_TO_ONE, 16, false);
		}
	}

	UpdateGPUBuffer();
}

void SnakeFollow::UpdateGPUBuffer()
{
	if (!m_vertexBuffer)
	{
		return;
	}

	unsigned int sizeInBytes = static_cast<unsigned int>(m_snakeTBNVerts.size()) * sizeof(Vertex_PCUTBN);
	g_theRenderer->CopyCPUToGPU(m_snakeTBNVerts.data(), sizeInBytes, m_vertexBuffer);
}

void SnakeFollow::DeleteBuffers()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;
}
