#include "Game/Snake.hpp"
#include "Game/GameCommon.h"
#include "Game/AnimalMode.hpp"
#include "Game/Terrain.hpp"
#include "Engine/Input/InputSystem.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Animation/Animation.hpp"
#include "Engine/AI/BehaviorTree.hpp"
#include "Engine/Math/Splines.hpp"

Snake::Snake(AnimalMode* animalMode, Vec3 position)
	:Entity(animalMode, position)
{
	m_snakeTexture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/snakeskin.jpg");
	Initialize();
}

Snake::~Snake()
{
	DeleteAnimations();

	delete m_snakeBT;
	m_snakeBT = nullptr;
}

void Snake::Initialize()
{
	// Initialize the skeleton
	m_snakeSkeleton = CreateSkeleton();

	m_snakeInitialPositions.clear();
	for (int snakeBoneIndex = 0; snakeBoneIndex < static_cast<int>(m_snakeSkeleton.m_bones.size()); ++snakeBoneIndex)
	{
		Bone& snakeBone = m_snakeSkeleton.m_bones[snakeBoneIndex];
		m_snakeInitialPositions.push_back(snakeBone.m_localPosition);

		Vec3 direction = Vec3::XAXE;
		if (snakeBone.m_parentBoneIndex != -1)
		{
			Vec3 parentPos = m_snakeSkeleton.m_bones[snakeBone.m_parentBoneIndex].m_localPosition;
			direction = snakeBone.m_localPosition - parentPos;
			if (!direction.IsNearlyZero())
			{
				direction.Normalize();
			}
		}
		m_snakeAnimationDirs.push_back(direction);
	}

	SetupAnimations();
	m_snakeBT = CreateSnakeBehaviorTree(this);
}

void Snake::Update(float deltaSeconds)
{
	SnakeLocomotionKeyPresses();
	IdleToggle();

	// Updating behavior tree
    if (m_snakeBT)
    {
        m_snakeBT->Update(deltaSeconds);
    }

	// Updating anims
    CheckTransitions();
	m_snakeStateMachine.Update(m_snakeSkeleton, deltaSeconds);

	// Updating pose
    UpdateSnakePose(deltaSeconds);
    UpdateVerts();
}

void Snake::IdleToggle()
{
	if (g_theInput->WasKeyJustPressed('I'))
	{
		m_enableIdleTransitions = !m_enableIdleTransitions;

		if (!m_enableIdleTransitions)
		{
			// Force slither so we never remain in idle state
			m_snakeStateMachine.SetAnimationState("Slither");
		}
	}
}

void Snake::SnakeLocomotionKeyPresses()
{
	if (g_theInput->WasKeyJustPressed('1'))
	{
		m_locomotionMode = SnakeLocomotionMode::SERPENTINE;
		ResetBindPose();
	}
	if (g_theInput->WasKeyJustPressed('2'))
	{
		m_locomotionMode = SnakeLocomotionMode::CONCERTINA;
		ResetBindPose();
	}

	if (g_theInput->WasKeyJustPressed('Q'))
	{
		int serpMode = static_cast<int>(m_currentSerpentineMode);
		serpMode = (serpMode + 1) % static_cast<int>(SnakeSerpentineTechnique::NUM_MODES);
		m_currentSerpentineMode = static_cast<SnakeSerpentineTechnique>(serpMode);
	}
}

void Snake::ResetBindPose()
{
	for (int snakeBoneIndex = 0; snakeBoneIndex < static_cast<int>(m_snakeSkeleton.m_bones.size()); ++snakeBoneIndex)
	{
		m_snakeSkeleton.m_bones[snakeBoneIndex].SetLocalBonePosition(m_snakeInitialPositions[snakeBoneIndex]);
	}
	m_snakeSkeleton.UpdateSkeletonPose();
}

void Snake::CheckTransitions()
{
	if (!m_enableIdleTransitions)
	{
		if (m_snakeStateMachine.GetCurrentAnimStateName() != "Slither")
		{
			m_snakeStateMachine.SetAnimationState("Slither");
		}
		return;
	}

	if (IsMoving())
	{
		if (m_locomotionMode == SnakeLocomotionMode::SERPENTINE)
		{
			if (m_snakeStateMachine.GetCurrentAnimStateName() != "Slither")
			{
				m_snakeStateMachine.TriggerTransition("StartSlither");
			}
		}
		else
		{
			m_snakeStateMachine.TriggerTransition("StopSlither");
		}
	}
	else
	{
		if (!IsMoving())
		{
			if (m_snakeStateMachine.GetCurrentAnimStateName() == "Slither")
			{
				m_snakeStateMachine.TriggerTransition("StopSlither");
			}
		}
	}
}

void Snake::UpdateSnakePose(float deltaSeconds)
{
	static float elapsedTime = 0.f;
	elapsedTime += deltaSeconds;

	if (!m_isStationary && m_isMoving)
	{
		// Smoothly interpolate move direction
		if (m_moveDirection != m_targetMoveDirection)
		{
			m_directionInterpTime += deltaSeconds;
			float fractionTowardEnd = GetClamped(m_directionInterpTime / m_directionInterpDuration, 0.f, 1.f);

			float easedFraction = SmoothStep3(fractionTowardEnd);
			m_moveDirection = SLerp(m_moveDirection, m_targetMoveDirection, easedFraction).GetNormalized();
		}

		// Move snake forward
		if (m_locomotionMode == SnakeLocomotionMode::SERPENTINE)
		{
			if (m_currentSerpentineMode == SnakeSerpentineTechnique::CURVATURE_DOWN_TAIL)
			{
				// Base intended velocity
				float wavePush = sinf(elapsedTime * 2.f) * 0.5f;

				Vec3 lateral = CrossProduct3D(m_moveDirection, Vec3::ZAXE).GetNormalized();
				Vec3 forward = m_moveDirection.GetNormalized();

				Vec3 desiredVelocity = (-m_moveDirection * m_speed) + (lateral * wavePush);

				// Decompose velocity
				float forwardComponent = DotProduct3D(desiredVelocity, forward);
				float lateralComponent = DotProduct3D(desiredVelocity, lateral);

				// Apply small friction
				float lateralFriction = 0.1f;
				Vec3 correctedVelocity = forward * forwardComponent + lateral * (lateralComponent * lateralFriction);

				// Apply movement
				m_worldPosition += correctedVelocity * deltaSeconds;
			}
			else
			{
				Vec3 snakeMove = -m_moveDirection * m_speed * deltaSeconds;
				m_worldPosition += snakeMove;
			}
		}
		else if (m_locomotionMode == SnakeLocomotionMode::CONCERTINA)
		{
			UpdateConcertinaMovement(deltaSeconds);
		}

		// Project onto terrain
		float terrainZ = m_animalMode->m_terrain->GetHeightAtXY(m_worldPosition.x, m_worldPosition.y);
		m_worldPosition.z = terrainZ;

		// Reset if out of bounds
		if (!m_animalMode->m_terrain->IsInBounds(static_cast<int>(m_worldPosition.x), static_cast<int>(m_worldPosition.y)))
		{
			float randomXLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
			float randomYLocation = g_rng->RollRandomFloatInRange(TERRAIN_RESET_MIN, TERRAIN_RESET_MAX);
			m_worldPosition = Vec3(randomXLocation, randomYLocation, m_animalMode->m_terrain->GetHeightAtXY(randomXLocation, randomYLocation));
			elapsedTime = 0.f;
		}
	}

	SolveTerrainAlignment();
}

void Snake::UpdateConcertinaMovement(float deltaSeconds)
{
	/* Concertina Phases 
	PHASE 1:  Anchor back half
	PHASE 2 : Extend head forward
	PHASE 3 : Anchor head
	PHASE 4 : Pull rear forward
	*/

	m_concertinaTimer += deltaSeconds;
	m_concertinaProgress += deltaSeconds * 0.1f;

	if (m_concertinaProgress > 1.f)
	{
		m_concertinaProgress = 0.f;
	}

	float phaseTime = m_concertinaTimer / m_concertinaPhaseDuration;
	phaseTime = GetClamped(phaseTime, 0.f, 1.f);
	float easedPhaseTime = SmoothStep3(phaseTime);
	
	int   boneCount = static_cast<int>(m_snakeSkeleton.m_bones.size());
	int   middleIndex = boneCount / 2;

	switch (m_concertinaPhase)
	{
		case ConcertinaPhase::ANCHORREAR:
		{
			ApplyConcertinaCurve(middleIndex, boneCount - 1, easedPhaseTime);
			break;
		}
		case ConcertinaPhase::EXTENDFORWARD:
		{
			m_worldPosition += -m_moveDirection * m_speed * deltaSeconds;

			ApplyConcertinaCurve(middleIndex, boneCount - 1, 1.f - easedPhaseTime);
			break;
		}
		case ConcertinaPhase::ANCHORFRONT:
		{
			ApplyConcertinaCurve(0, middleIndex, easedPhaseTime);
			break;
		}
		case ConcertinaPhase::PULLREAR:
		{
			m_worldPosition += -m_moveDirection * m_speed * deltaSeconds;

			ApplyConcertinaCurve(0, middleIndex, 1.f - easedPhaseTime);
			break;
		}
	}

	if (m_concertinaTimer >= m_concertinaPhaseDuration)
	{
		m_concertinaTimer = 0.f;
		m_concertinaPhase = static_cast<ConcertinaPhase>(((static_cast<int>(m_concertinaPhase) + 1) % 4));
	}
}

void Snake::ApplyConcertinaCurve(int startIndex, int endIndex, float anchorStrength)
{
	float frequency = 1.2f;
	float baseAngle = 5.f;
	float anchorAngle = 60.f;

	int boneCount = static_cast<int>(m_snakeSkeleton.m_bones.size());
	float waveShift = m_concertinaProgress * 2.f * PI;   

	for (int snakeBoneIndex = 0; snakeBoneIndex < boneCount; ++snakeBoneIndex)
	{
		Bone& bone = m_snakeSkeleton.m_bones[snakeBoneIndex];
		float time = static_cast<float>(snakeBoneIndex) / static_cast<float>(boneCount);

		// Anchor envelope
		float envelope = 0.f;
		if (snakeBoneIndex >= startIndex && snakeBoneIndex <= endIndex)
		{
			envelope = anchorStrength;
		}

		float maxAngle = baseAngle + envelope * (anchorAngle - baseAngle);
		float phase = time * frequency * 2.f * PI + waveShift;
		float curve = sinf(phase) * maxAngle;

		float prevTime = static_cast<float>((snakeBoneIndex - 1.f)) / static_cast<float>(boneCount);
		float prevPhase = prevTime * frequency * 2.f * PI + waveShift;
		float prevCurve = sinf(prevPhase) * maxAngle;

		float angleDelta = curve - prevCurve;

		Quat rot = Quat::MakeFromEulerAngles(EulerAngles(0.f, 0.f, angleDelta));
		bone.SetLocalBoneRotation(rot);
	}

	m_snakeSkeleton.UpdateSkeletonPose();
}

void Snake::SolveTerrainAlignment()
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

	Vec3 headLocalPos = m_snakeSkeleton.m_bones[0].m_localPosition;
	Vec3 headWorldOffset = rotation.TransformPosition3D(headLocalPos);

	Vec3 skeletonHeadOrigin = m_worldPosition - headWorldOffset;
	rotation.SetTranslation3D(skeletonHeadOrigin);

	m_snakeSkeleton.m_skeletonModelTransform = rotation;
	m_snakeSkeleton.UpdateSkeletonPose();

	// Push bone down to ground and align with terrain
	for (int boneIndex = 0; boneIndex < static_cast<int>(m_snakeSkeleton.m_bones.size()); ++boneIndex)
	{
		Bone& bone = m_snakeSkeleton.m_bones[boneIndex];
		Vec3 worldPos = bone.m_worldBoneTransform.GetTranslation3D();
		float terrainZ = terrain->GetHeightAtXY(worldPos.x, worldPos.y);

		worldPos.z = terrainZ;

		bone.m_worldBoneTransform.SetTranslation3D(worldPos);
	}
}

void Snake::UpdateVerts()
{
	m_snakeVerts.clear();
	m_snakeVertsUntextured.clear();
	if (m_animalMode->m_isSkeletonBeingDrawn)
	{
		m_snakeSkeletonVerts.clear();
	}

	if (m_animalMode->m_isSkeletonBeingDrawn)
	{
		m_snakeSkeleton.AddVertsForSkeleton3D(m_snakeSkeletonVerts);
	}

	for (int boneIndex = 0; boneIndex < static_cast<int>(m_snakeSkeleton.m_bones.size()); ++boneIndex)
	{
		Bone const& bone = m_snakeSkeleton.m_bones[boneIndex];
		Vec3 worldPosition = bone.m_worldBoneTransform.GetTranslation3D();
		float radius = 0.25f;

		// HEAD
		if (boneIndex == 0)
		{
			// Head sphere
			AddVertsForSphere3D(m_snakeVerts, worldPosition, 0.3f, Rgba8::BROWN);

			// Eyes offset from head position
			Vec3 forward = bone.m_worldBoneTransform.GetIBasis3D();
			Vec3 left = bone.m_worldBoneTransform.GetJBasis3D();
			Vec3 up = bone.m_worldBoneTransform.GetKBasis3D();

			// Positioning the eyes
			Vec3 leftEyePosition = worldPosition - forward * 0.2f + left * 0.1f + up * 0.25f;
			Vec3 rightEyePosition = worldPosition - forward * 0.2f - left * 0.1f + up * 0.25f;

			// White eye bases
			AddVertsForSphere3D(m_snakeVertsUntextured, leftEyePosition, 0.05f, Rgba8::WHITE);
			AddVertsForSphere3D(m_snakeVertsUntextured, rightEyePosition, 0.05f, Rgba8::WHITE);

			// Black pupils
			Vec3 pupilOffset = forward * 0.03f; 
			AddVertsForSphere3D(m_snakeVertsUntextured, leftEyePosition - pupilOffset, 0.025f, Rgba8::BLACK);
			AddVertsForSphere3D(m_snakeVertsUntextured, rightEyePosition - pupilOffset, 0.025f, Rgba8::BLACK);
		}
		// TAIL
		else if (boneIndex == static_cast<int>(m_snakeSkeleton.m_bones.size()) - 1)
		{
			int parentIndex = bone.m_parentBoneIndex;
			if (parentIndex != -1)
			{
				Vec3 parentPosition = m_snakeSkeleton.m_bones[parentIndex].m_worldBoneTransform.GetTranslation3D();
				Vec3 tailTip = worldPosition;
				Vec3 tailBase = parentPosition;
				AddVertsForCone3D(m_snakeVerts, tailBase, tailTip, radius, Rgba8::BROWN, AABB2::ZERO_TO_ONE, 16);
			}
		}
		// BODY SEGMENTS
		else
		{
			AddVertsForSphere3D(m_snakeVerts, worldPosition, radius, Rgba8::BROWN);

			if (bone.m_parentBoneIndex == -1)
			{
				continue;
			}

			Vec3 start = m_snakeSkeleton.m_bones[bone.m_parentBoneIndex].m_worldBoneTransform.GetTranslation3D();
			Vec3 end = bone.m_worldBoneTransform.GetTranslation3D();

			AddVertsForCylinder3D(m_snakeVerts, start, end, radius, Rgba8::BROWN, AABB2::ZERO_TO_ONE, 16);
		}
	}
}

void Snake::Render() const
{
	DrawSnake();
	DrawSnakeSkeleton();
}

void Snake::DrawSnakeSkeleton() const
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
		g_theRenderer->DrawVertexArray(m_snakeSkeletonVerts);
	}
}

void Snake::DrawSnake() const
{
	if (m_animalMode->m_isAnimalVertsBeingDrawn)
	{
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
		g_theRenderer->BindShader(nullptr);

		// Drawing textured
		g_theRenderer->BindTexture(m_snakeTexture);
		g_theRenderer->DrawVertexArray(m_snakeVerts);

		// Drawing untextured
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(m_snakeVertsUntextured);
	}
}

bool Snake::IsMoving() const
{
	return m_isMoving;
}

AnimStateMachine Snake::GetSnakeStateMachine() const
{
	return m_snakeStateMachine;
}

Skeleton Snake::CreateSkeleton()
{
	Skeleton snakeSkeleton;
	snakeSkeleton.m_bones.clear();

	// Ascending to descending
	Bone head;
	head.m_boneName = "Head";
	head.SetLocalBonePosition(Vec3(0.f, 3.f, 0.2f));
	snakeSkeleton.m_bones.push_back(head);

	Bone firstConnector;
	firstConnector.m_boneName = "FirstCoil";
	firstConnector.m_parentBoneIndex = 0;
	firstConnector.SetLocalBonePosition(Vec3(3.f, 0.25f, 0.f));
	snakeSkeleton.m_bones.push_back(firstConnector);

	Bone secondConnector;
	secondConnector.m_boneName = "SecondCoil";
	secondConnector.m_parentBoneIndex = 1;
	secondConnector.SetLocalBonePosition(Vec3(3.f, -0.25f, 0.f));
	snakeSkeleton.m_bones.push_back(secondConnector);

	Bone thirdConnector;
	thirdConnector.m_boneName = "ThirdCoil";
	thirdConnector.m_parentBoneIndex = 2;
	thirdConnector.SetLocalBonePosition(Vec3(3.f, 0.25f, 0.f));
	snakeSkeleton.m_bones.push_back(thirdConnector);

	Bone fourthConnector;
	fourthConnector.m_boneName = "FourthCoil";
	fourthConnector.m_parentBoneIndex = 3;
	fourthConnector.SetLocalBonePosition(Vec3(3.f, -0.25f, 0.f));
	snakeSkeleton.m_bones.push_back(fourthConnector);

	Bone fifthConnector;
	fifthConnector.m_boneName = "FifthCoil";
	fifthConnector.m_parentBoneIndex = 4;
	fifthConnector.SetLocalBonePosition(Vec3(3.f, 0.25f, 0.f));
	snakeSkeleton.m_bones.push_back(fifthConnector);

	Bone sixthConnector;
	sixthConnector.m_boneName = "SixthCoil";
	sixthConnector.m_parentBoneIndex = 5;
	sixthConnector.SetLocalBonePosition(Vec3(3.f, 0.25f, 0.f));
	snakeSkeleton.m_bones.push_back(sixthConnector);

	Bone seventhConnector;
	seventhConnector.m_boneName = "SeventhCoil";
	seventhConnector.m_parentBoneIndex = 6;
	seventhConnector.SetLocalBonePosition(Vec3(3.f, -0.25f, 0.f));
	snakeSkeleton.m_bones.push_back(seventhConnector);

	Bone eighthConnector;
	eighthConnector.m_boneName = "EighthCoil";
	eighthConnector.m_parentBoneIndex = 7;
	eighthConnector.SetLocalBonePosition(Vec3(3.f, 0.25f, 0.f));
	snakeSkeleton.m_bones.push_back(eighthConnector);

	Bone ninthConnector;
	ninthConnector.m_boneName = "NinthCoil";
	ninthConnector.m_parentBoneIndex = 8;
	ninthConnector.SetLocalBonePosition(Vec3(3.f, -0.25f, 0.f));
	snakeSkeleton.m_bones.push_back(ninthConnector);

	Bone tail;
	tail.m_boneName = "Tail";
	tail.m_parentBoneIndex = 9;
	tail.SetLocalBonePosition(Vec3(3.f, 0.25f, 0.f));
	snakeSkeleton.m_bones.push_back(tail);

	snakeSkeleton.UpdateSkeletonPose();
	return snakeSkeleton;
}

void Snake::SetupAnimations()
{
	// Snake's stored anim sequences
	m_snakeTailFlickIdleAnim = new Animation("IdleTailFlick", 30.f, false, [this](Skeleton& skeleton, float time)
	{
		float flickDegrees = SinDegrees(time * 720.f) * 20.f;
		Quat tailRotation = Quat::MakeFromEulerAngles(EulerAngles(0.f, 0.f, flickDegrees)); 

		Bone& tailConnector = skeleton.m_bones[9];
		Bone& tailBone = skeleton.m_bones[10];
		tailConnector.SetLocalBoneRotation(tailRotation);
		tailBone.SetLocalBoneRotation(tailRotation);

		skeleton.UpdateSkeletonPose();
	});

	m_snakeHeadRaiseIdleAnim = new Animation("IdleHeadRaise", 30.f, false, [this](Skeleton& skeleton, float time)
	{
		float raiseAmountDegrees = SinDegrees(time * 180.f) * 15.f;
		Bone& head = skeleton.m_bones[0];
		Quat headRot = Quat::MakeFromEulerAngles(EulerAngles(0.f, raiseAmountDegrees, 0.f));
		head.SetLocalBoneRotation(headRot);

		// Counter rotating the first child
		Bone& firstCoil = skeleton.m_bones[1];
		firstCoil.SetLocalBoneRotation(headRot.QuatInverse());

		skeleton.UpdateSkeletonPose();
	});

	m_snakeSlitherAnim = new Animation("Slither", 30.f, false, [this](Skeleton& skeleton, float time)
	{
		if (m_currentSerpentineMode == SnakeSerpentineTechnique::CURVATURE_DOWN_TAIL)
		{
			float amplitudeDegrees = 10.f;
			float spatialFreq = 1.5f;
			float temporalFreq = 2.f;

			for (int snakeBoneIndex = 0; snakeBoneIndex < static_cast<int>(m_snakeSkeleton.m_bones.size()); ++snakeBoneIndex)
			{
				float leading = static_cast<float>(snakeBoneIndex) / static_cast<float>((m_snakeSkeleton.m_bones.size() - 1.f));
				float phase = (leading * spatialFreq * 2.f * PI) - (time * temporalFreq);
				float angle = sinf(phase) * amplitudeDegrees;

				Quat boneRotation = Quat::MakeFromEulerAngles(EulerAngles(0.f, 0.f, angle));
				skeleton.m_bones[snakeBoneIndex].SetLocalBoneRotation(boneRotation);
			}

			skeleton.UpdateSkeletonPose();
		}
		else if (m_currentSerpentineMode == SnakeSerpentineTechnique::OSCILLATING_JOINTS)
		{
			for (int snakeBoneIndex = 0; snakeBoneIndex < static_cast<int>(skeleton.m_bones.size()); ++snakeBoneIndex)
			{
				Bone& bone = skeleton.m_bones[snakeBoneIndex];
				Vec3 basePosition = m_snakeInitialPositions[snakeBoneIndex];
				float phase = time - snakeBoneIndex * 0.2f;
				float curvature = sinf(phase * 3.f);
				Vec3 offset = m_snakeAnimationDirs[snakeBoneIndex] * curvature;
				bone.SetLocalBonePosition(basePosition + offset);
			}
			skeleton.UpdateSkeletonPose();
		}
	});

	// Snake's state machine
	m_snakeStateMachine.AddAnimationState("IdleTailFlick", m_snakeTailFlickIdleAnim);
	m_snakeStateMachine.AddAnimationState("IdleHeadRaise", m_snakeHeadRaiseIdleAnim);
	m_snakeStateMachine.AddAnimationState("Slither", m_snakeSlitherAnim);
	m_snakeStateMachine.AddTransition("IdleTailFlick", "StartSlither", "Slither");
	m_snakeStateMachine.AddTransition("IdleHeadRaise", "StartSlither", "Slither");
	m_snakeStateMachine.SetAnimationState("Slither");
}

void Snake::DeleteAnimations()
{
	delete m_snakeSlitherAnim;
	m_snakeSlitherAnim = nullptr;

	delete m_snakeTailFlickIdleAnim;
	m_snakeTailFlickIdleAnim = nullptr;

	delete m_snakeHeadRaiseIdleAnim;
	m_snakeHeadRaiseIdleAnim = nullptr;
}

BehaviorTree* Snake::CreateSnakeBehaviorTree(Snake* snake)
{
	auto moveNode = std::make_shared<SnakeMoveNode>(snake);
	auto idleNode = std::make_shared<SnakeIdleNode>(snake, moveNode.get());

	auto root = std::make_shared<SelectorNode>(std::vector<BehaviorNodePtr>{moveNode, idleNode});
	return new BehaviorTree(root);
}
// -----------------------------------------------------------------------------
BehaviorResult SnakeMoveNode::Update(float deltaSeconds)
{
	if (m_doneMoving)
	{
		return BehaviorResult::FAILURE;
	}

	m_directionChangeTimer += deltaSeconds;
	m_moveDuration += deltaSeconds;

	if (m_directionChangeTimer >= m_timeToChangeDir)
	{
		m_directionChangeTimer = 0.f;

		// Picking a new random direction for the snake to go
		// Get current facing angle
		float currentAngle = Atan2Degrees(m_snake->m_moveDirection.y, m_snake->m_moveDirection.x);

		// Pick a delta angle within allowed turn range
		float deltaAngle = g_rng->RollRandomFloatInRange(-SNAKE_MAX_TURN, SNAKE_MAX_TURN);

		// Compute new angle
		float newAngle = currentAngle + deltaAngle;

		// Convert back to direction vector
		Vec2 newDir2D = Vec2(CosDegrees(newAngle), SinDegrees(newAngle));
		Vec3 newDirection = Vec3(newDir2D.x, newDir2D.y, 0.f).GetNormalized();

		m_snake->m_targetMoveDirection = newDirection;
		m_snake->m_directionInterpTime = 0.f;
		m_snake->m_targetMoveDirection = newDirection;
		m_snake->m_directionInterpTime = 0.f;
	}

	if (m_moveDuration >= m_maxMoveDuration)
	{
		m_moveDuration = 0.f;
		m_doneMoving = true;
		return BehaviorResult::FAILURE;
	}

	m_snake->m_isMoving = true;
	return BehaviorResult::SUCCESS;
}

void SnakeMoveNode::Reset()
{
	m_moveDuration = 0.f;
	m_doneMoving   = false;
}
// -----------------------------------------------------------------------------
BehaviorResult SnakeIdleNode::Update(float deltaSeconds)
{
	if (!m_snake->m_enableIdleTransitions)
	{
		return BehaviorResult::FAILURE;
	}

	if (!m_startedIdle)
	{
		PlayRandomIdleAnimation();
		m_startedIdle = true;
	}

	m_idleDuration += deltaSeconds;
	m_snake->m_isMoving = false;

	if (m_idleDuration >= m_maxIdleTime)
	{
		m_idleDuration = 0.f;

		if (m_snakeMoveNode != nullptr)
		{
			m_snakeMoveNode->Reset();
			Reset();
		}

		return BehaviorResult::FAILURE;
	}

	return BehaviorResult::SUCCESS;
}

void SnakeIdleNode::PlayRandomIdleAnimation()
{
	int randomIdle = g_rng->RollRandomIntInRange(0, 1);
	switch (randomIdle)
	{
		case 0: m_snake->m_snakeStateMachine.SetAnimationState("IdleTailFlick"); break;
		case 1: m_snake->m_snakeStateMachine.SetAnimationState("IdleHeadRaise"); break;
	}
}

void SnakeIdleNode::Reset()
{
	m_startedIdle = false;
	m_idleDuration = 0.f; 
}
// -----------------------------------------------------------------------------
