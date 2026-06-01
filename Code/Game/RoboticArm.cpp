#include "Game/RoboticArm.hpp"
#include "Game/App.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Core/DebugRender.hpp"

RoboticArmMode::RoboticArmMode(App* owner)
	:Game(owner)
{
}

void RoboticArmMode::StartUp()
{
	InitializeGrid();
	m_font = g_theRenderer->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");
	m_shader = g_theRenderer->CreateOrGetShader("Data/Shaders/BlinnPhong_RB", VertexType::VERTEX_PCUTBN);
	m_testUVTexture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/TestUV.png");

	// Get world shader
	m_worldShader = g_theRenderer->CreateOrGetShader("Data/Shaders/WorldShader", VertexType::VERTEX_PCU);
	m_world_CBO = g_theRenderer->CreateConstantBuffer(sizeof(WorldConstants));
	
	// Create robotic arm
	m_roboticArm = InitializeRoboticArm();
	if (m_isSkeletonBeingDrawn)
	{
		m_roboticArm.AddVertsForSkeleton3D(m_roboSkeletonDebugVerts);
	}
	m_clawTip1BindRotation = m_roboticArm.m_bones[5].m_localRotation;
	m_clawTip2BindRotation = m_roboticArm.m_bones[7].m_localRotation;
	UpdateVerts();
}

void RoboticArmMode::Update()
{
	double deltaSeconds = g_theApp->m_gameClock->GetDeltaSeconds();

	// Target Position
	TargetPositionMovement(static_cast<float>(deltaSeconds));
	if (m_targetPosition.z < 0.f)
	{
		m_targetPosition.z = 0.f;
	}
	DebugAddWorldSphere(m_targetPosition, 0.25f, 0.f, Rgba8::GREEN, Rgba8::GREEN);

	// Setting PerFrame constants
	g_theRenderer->SetPerFrameConstants(m_debugInt, 0.f);

	if (!m_isArmConstrained)
	{
		SolveCCDIK({ 0, 1, 2, 3, 8 }, m_targetPosition, 8, 15);
	}
	else
	{
		SolveCCDIKConstrained({ 0, 1, 2, 3, 8 }, m_targetPosition, 8, 15);
	}
	UpdateClaw();
	UpdateVerts();

	AdjustForPauseAndTimeDistortion(static_cast<float>(deltaSeconds));
	ToggleConstraints();
	DebugVisuals();
	UpdateCameras(static_cast<float>(deltaSeconds));
}

void RoboticArmMode::Render() const
{
	g_theRenderer->BeginCamera(m_gameWorldCamera);
	g_theRenderer->ClearScreen(Rgba8::DARKSLATEGRAY);
	if (m_isGridOn)
	{
		RenderGrid();
	}
	RenderRoboticArm();
	g_theRenderer->EndCamera(m_gameWorldCamera);

	g_theRenderer->BeginCamera(g_theApp->m_screenCamera);
	GameModeAndControlsText();
	g_theRenderer->EndCamera(g_theApp->m_screenCamera);

	DebugRenderWorld(m_gameWorldCamera);
	DebugRenderScreen(g_theApp->m_screenCamera);
}

void RoboticArmMode::Shutdown()
{
	DeleteBuffers();

	delete m_world_CBO;
	m_world_CBO = nullptr;
}

Skeleton RoboticArmMode::InitializeRoboticArm()
{
	Skeleton roboticArm;
	roboticArm.m_bones.clear();

	// Ball and socket root rotator
	Bone rootRotator;
	rootRotator.SetLocalBonePosition(Vec3::ZERO);
	rootRotator.m_boneConstraint.m_rotationConstraints[0] = CONSTRAINT_TYPE::LIMITED;
	rootRotator.m_boneConstraint.m_rotationConstraints[1] = CONSTRAINT_TYPE::LIMITED;
	rootRotator.m_boneConstraint.m_rotationConstraints[2] = CONSTRAINT_TYPE::LOCKED;  
	rootRotator.m_boneConstraint.m_minRotationDegrees = EulerAngles(-45.f, -45.f, 0.f);
	rootRotator.m_boneConstraint.m_maxRotationDegrees = EulerAngles(45.f, 45.f, 0.f);
	roboticArm.m_bones.push_back(rootRotator);

	// Hinge lower extender of arm
	Bone lowerExtender;
	lowerExtender.m_parentBoneIndex = 0;
	lowerExtender.SetLocalBonePosition(Vec3(0.f, 0.f, 3.f));
	lowerExtender.m_boneConstraint.m_rotationConstraints[0] = CONSTRAINT_TYPE::LIMITED;
	lowerExtender.m_boneConstraint.m_rotationConstraints[1] = CONSTRAINT_TYPE::LIMITED;
	lowerExtender.m_boneConstraint.m_rotationConstraints[2] = CONSTRAINT_TYPE::LOCKED;
	lowerExtender.m_boneConstraint.m_minRotationDegrees = EulerAngles(-90.f, 0.f, 0.f);
	lowerExtender.m_boneConstraint.m_maxRotationDegrees = EulerAngles(90.f, 0.f, 0.f);
	roboticArm.m_bones.push_back(lowerExtender);

	// Hinge upper extender of arm
	Bone upperExtender;
	upperExtender.m_parentBoneIndex = 1;
	upperExtender.SetLocalBonePosition(Vec3(0.f, 0.f, 2.f));
	upperExtender.m_boneConstraint.m_rotationConstraints[0] = CONSTRAINT_TYPE::LIMITED;
	upperExtender.m_boneConstraint.m_rotationConstraints[1] = CONSTRAINT_TYPE::LOCKED;
	upperExtender.m_boneConstraint.m_rotationConstraints[2] = CONSTRAINT_TYPE::LOCKED;
	upperExtender.m_boneConstraint.m_minRotationDegrees = EulerAngles(-135.f, 0.f, 0.f);
	upperExtender.m_boneConstraint.m_maxRotationDegrees = EulerAngles(135.f, 0.f, 0.f);
	roboticArm.m_bones.push_back(upperExtender);

	Bone endEffector;
	endEffector.m_parentBoneIndex = 2;
	endEffector.SetLocalBonePosition(Vec3::ZAXE);
	endEffector.m_boneConstraint.m_rotationConstraints[0] = CONSTRAINT_TYPE::LIMITED;
	endEffector.m_boneConstraint.m_rotationConstraints[1] = CONSTRAINT_TYPE::LIMITED;
	endEffector.m_boneConstraint.m_rotationConstraints[2] = CONSTRAINT_TYPE::LOCKED;
	endEffector.m_boneConstraint.m_minRotationDegrees = EulerAngles(-30.f, -45.f, 0.f);
	endEffector.m_boneConstraint.m_maxRotationDegrees = EulerAngles(30.f, 45.f, 0.f);
	roboticArm.m_bones.push_back(endEffector);

	// ---- CLAW ----
	Bone clawBase1;
	clawBase1.m_parentBoneIndex = 3;
	clawBase1.SetLocalBonePosition(Vec3(0.f, 0.5f, 0.5f));
	roboticArm.m_bones.push_back(clawBase1);

	Bone clawTip1;
	clawTip1.m_parentBoneIndex = 4;
	clawTip1.SetLocalBonePosition(Vec3(0.f, -0.25f, 0.5f));
	roboticArm.m_bones.push_back(clawTip1);

	Bone clawBase2;
	clawBase2.m_parentBoneIndex = 3;
	clawBase2.SetLocalBonePosition(Vec3(0.f, -0.5f, 0.5f));
	roboticArm.m_bones.push_back(clawBase2);

	Bone clawTip2;
	clawTip2.m_parentBoneIndex = 6;
	clawTip2.SetLocalBonePosition(Vec3(0.f, 0.25f, 0.5f));
	roboticArm.m_bones.push_back(clawTip2);

	// Virtual midpoint bone
	Bone clawMidpoint;
	clawMidpoint.m_parentBoneIndex = 3; 
	clawMidpoint.SetLocalBonePosition(Vec3::ZAXE);
	clawMidpoint.m_isRenderable = false;
	roboticArm.m_bones.push_back(clawMidpoint);

	roboticArm.UpdateSkeletonPose();
	return roboticArm;
}

void RoboticArmMode::CreateBuffers()
{
	if (m_roboArmVerts.empty())
	{
		return;
	}

	DeleteBuffers();

	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(static_cast<unsigned int>(m_roboArmVerts.size()) * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	g_theRenderer->CopyCPUToGPU(m_roboArmVerts.data(), m_vertexBuffer->GetSize(), m_vertexBuffer);
}

void RoboticArmMode::UpdateCameras(float deltaSeconds)
{
	// Game Camera
	Mat44 cameraToRender(Vec3::ZAXE, -Vec3::XAXE, Vec3::YAXE, Vec3::ZERO);
	m_gameWorldCamera.SetCameraToRenderTransform(cameraToRender);

	FreeFlyControls(deltaSeconds);
	m_cameraOrientation.m_pitchDegrees = GetClamped(m_cameraOrientation.m_pitchDegrees, -85.f, 85.f);
	m_cameraOrientation.m_rollDegrees = GetClamped(m_cameraOrientation.m_rollDegrees, -45.f, 45.f);

	m_gameWorldCamera.SetPositionAndOrientation(m_cameraPos, m_cameraOrientation);
	m_gameWorldCamera.SetPerspectiveView(2.f, 60.f, 0.1f, 100.f);
}

void RoboticArmMode::UpdateClaw()
{
	int tip1 = 4;
	int tip2 = 6;
	int mid = 8;

	Vec3 midPos = m_roboticArm.m_bones[mid].GetWorldBonePosition3D();
	float dist = (m_targetPosition - midPos).GetLength();

	float t = RangeMapClamped(dist, 0.f, 3.f, 0.f, 1.f);

	Vec3 hingeAxis = Vec3::XAXE;

	Quat openRot1 = Quat::MakeFromAxisAngle(hingeAxis, 5.75f);
	Quat openRot2 = Quat::MakeFromAxisAngle(hingeAxis, -5.75f);

	Quat target1 = openRot1 * m_clawTip1BindRotation;
	Quat target2 = openRot2 * m_clawTip2BindRotation;

	Quat final1 = SLerp(m_clawTip1BindRotation, target1, t);
	Quat final2 = SLerp(m_clawTip2BindRotation, target2, t);

	m_roboticArm.m_bones[tip1].SetLocalBoneRotation(final1);
	m_roboticArm.m_bones[tip2].SetLocalBoneRotation(final2);

	m_roboticArm.UpdateSkeletonPose();
}

void RoboticArmMode::UpdateVerts()
{
	m_roboArmVerts.clear();
	if (m_isSkeletonBeingDrawn)
	{
		m_roboSkeletonDebugVerts.clear();
	}

	if (m_isSkeletonBeingDrawn)
	{
		m_roboticArm.AddVertsForSkeleton3D(m_roboSkeletonDebugVerts);
	}

	AddVertsForAABB3D(m_roboArmVerts, AABB3(Vec3(-1.f, -1.f, -0.25f), Vec3(1.f, 1.f, 0.f)), Rgba8::GRAY);

	for (int boneIndex = 0; boneIndex < static_cast<int>(m_roboticArm.m_bones.size()); ++boneIndex)
	{
		Bone const& bone = m_roboticArm.m_bones[boneIndex];
		if (!bone.m_isRenderable)
		{
			continue;
		}
		Vec3 worldPosition = bone.m_worldBoneTransform.GetTranslation3D();

		float jointRadius = (boneIndex >= 4) ? 0.1f : 0.235f;
		AddVertsForSphere3D(m_roboArmVerts, worldPosition, jointRadius, Rgba8::BLACK);

		if (bone.m_parentBoneIndex == -1)
		{
			continue;
		}

		Vec3 start = m_roboticArm.m_bones[bone.m_parentBoneIndex].m_worldBoneTransform.GetTranslation3D();
		Vec3 end = bone.m_worldBoneTransform.GetTranslation3D();

		float boneRadius = (boneIndex >= 4) ? 0.1f : 0.235f;
		AddVertsForCylinder3D(m_roboArmVerts, start, end, boneRadius, Rgba8::GRAY, AABB2::ZERO_TO_ONE, 32);
	}

	CreateBuffers();
}

void RoboticArmMode::TargetPositionMovement(float deltaSeconds)
{
	if (g_theInput->IsKeyDown('I'))
	{
		m_targetPosition.x += 2.f * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('K'))
	{
		m_targetPosition.x -= 2.f * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('J'))
	{
		m_targetPosition.y += 2.f * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('L'))
	{
		m_targetPosition.y -= 2.f * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('N'))
	{
		m_targetPosition.z += 2.f * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('M'))
	{
		m_targetPosition.z -= 2.f * static_cast<float>(deltaSeconds);
	}
}

void RoboticArmMode::DebugVisuals()
{
	if (g_theInput->WasKeyJustPressed('0'))
	{
		m_debugInt = 0;
	}
	if (g_theInput->WasKeyJustPressed('1'))
	{
		m_debugInt = 1;
	}
	if (g_theInput->WasKeyJustPressed('2'))
	{
		m_debugInt = 2;
	}
	if (g_theInput->WasKeyJustPressed('3'))
	{
		m_debugInt = 3;
	}
	if (g_theInput->WasKeyJustPressed('7'))
	{
		m_debugInt = 7;
	}
	if (g_theInput->WasKeyJustPressed('8'))
	{
		m_debugInt = 8;
	}
	// Debug Tangent/Bitangent/Normal
	if (g_theInput->WasKeyJustPressed('7'))
	{
		m_debugInt = 4;
	}
	if (g_theInput->WasKeyJustPressed('8'))
	{
		m_debugInt = 5;
	}
	if (g_theInput->WasKeyJustPressed('9'))
	{
		m_debugInt = 9;
	}

	// Debug draw skeleton
	if (g_theInput->WasKeyJustPressed('G'))
	{
		m_isSkeletonBeingDrawn = !m_isSkeletonBeingDrawn;
	}
	if (g_theInput->WasKeyJustPressed('V'))
	{
		m_isDrawingVerts = !m_isDrawingVerts;
	}
}

void RoboticArmMode::ToggleConstraints()
{
	if (g_theInput->WasKeyJustPressed('H'))
	{
		m_isArmConstrained = !m_isArmConstrained;
	}
}

void RoboticArmMode::SolveCCDIK(std::vector<int> const& chainIndices, Vec3 const& targetPosition, int endEffector, int maxIterations, float threshold)
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
		Vec3 chainIndexAPosition = m_roboticArm.m_bones[chainIndexA].GetWorldBonePosition3D();
		Vec3 chainIndexBPosition = m_roboticArm.m_bones[chainIndexB].GetWorldBonePosition3D();
		totalChainLength += (chainIndexBPosition - chainIndexAPosition).GetLength();
	}

	// Clamp if target is unreachable
	Vec3  rootPosition = m_roboticArm.m_bones[chainIndices[0]].GetWorldBonePosition3D();
	float distToTarget = (targetPosition - rootPosition).GetLength();

	Vec3 clampedTargetPos = targetPosition;
	if (distToTarget > totalChainLength)
	{
		Vec3 direction = (targetPosition - rootPosition).GetNormalized();
		clampedTargetPos = rootPosition + direction * totalChainLength;
	}

	distToTarget = (clampedTargetPos - rootPosition).GetLength();

	for (int iterationIndex = 0; iterationIndex < maxIterations; ++iterationIndex)
	{
		for (int chainIndex = static_cast<int>(chainIndices.size()) - 2; chainIndex >= 0; --chainIndex)
		{
			int jointIndex = chainIndices[chainIndex];
			int endEffectorIndex = endEffector;

			Vec3 jointPos = m_roboticArm.m_bones[jointIndex].GetWorldBonePosition3D();
			Vec3 endEffectorPos = m_roboticArm.m_bones[endEffectorIndex].GetWorldBonePosition3D();

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

			// Rotate to target if there is an angle greater than our threshold
			if (angle > 0.001f)
			{
				Vec3 rotationAxis = CrossProduct3D(toEndEffector, toTarget);

				rotationAxis.Normalize();
				Quat rotationQuat = Quat::MakeFromAxisAngle(rotationAxis, angle);

				Quat parentWorldRot = Quat::DEFAULT;
				int parentIndex = m_roboticArm.m_bones[jointIndex].m_parentBoneIndex;
				if (parentIndex != -1)
				{
					parentWorldRot = m_roboticArm.m_bones[parentIndex].GetWorldBoneRotation3D();
				}

				// Converting world delta to local delta
				Quat localDelta = parentWorldRot.QuatInverse() * rotationQuat * parentWorldRot;

				// Apply to local rotation
				Quat newLocalRot = localDelta * m_roboticArm.m_bones[jointIndex].m_localRotation;
				m_roboticArm.m_bones[jointIndex].SetLocalBoneRotation(newLocalRot);

				m_roboticArm.UpdateSkeletonPose();
			}
		}

		// Convergence check
		Vec3 newEffectorPos = m_roboticArm.m_bones[endEffector].GetWorldBonePosition3D();
		if ((newEffectorPos - clampedTargetPos).GetLength() <= threshold)
		{
			break;
		}
	}
}

void RoboticArmMode::SolveCCDIKConstrained(std::vector<int> const& chainIndices, Vec3 const& targetPosition, int endEffector, int maxIterations, float threshold)
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
		Vec3 chainIndexAPosition = m_roboticArm.m_bones[chainIndexA].GetWorldBonePosition3D();
		Vec3 chainIndexBPosition = m_roboticArm.m_bones[chainIndexB].GetWorldBonePosition3D();
		totalChainLength += (chainIndexBPosition - chainIndexAPosition).GetLength();
	}

	// Clamp if target is unreachable
	Vec3  rootPosition = m_roboticArm.m_bones[chainIndices[0]].GetWorldBonePosition3D();
	float distToTarget = (targetPosition - rootPosition).GetLength();

	Vec3 clampedTargetPos = targetPosition;
	if (distToTarget > totalChainLength)
	{
		Vec3 direction = (targetPosition - rootPosition).GetNormalized();
		float chainAdjust = 0.01f;
		clampedTargetPos = rootPosition + direction * (totalChainLength - chainAdjust);
	}

	distToTarget = (clampedTargetPos - rootPosition).GetLength();

	for (int iterationIndex = 0; iterationIndex < maxIterations; ++iterationIndex)
	{
		for (int chainIndex = static_cast<int>(chainIndices.size()) - 2; chainIndex >= 0; --chainIndex)
		{
			int jointIndex = chainIndices[chainIndex];
			int endEffectorIndex = endEffector;

			Vec3 jointPos = m_roboticArm.m_bones[jointIndex].GetWorldBonePosition3D();
			Vec3 endEffectorPos = m_roboticArm.m_bones[endEffectorIndex].GetWorldBonePosition3D();

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

			// Rotate to target if there is an angle greater than our threshold
			if (angle > 0.001f)
			{
				Vec3 rotationAxis = CrossProduct3D(toEndEffector, toTarget);

				rotationAxis.Normalize();
				Quat rotationQuat = Quat::MakeFromAxisAngle(rotationAxis, angle);

				Quat parentWorldRot = Quat::DEFAULT;
				int parentIndex = m_roboticArm.m_bones[jointIndex].m_parentBoneIndex;
				if (parentIndex != -1)
				{
					parentWorldRot = m_roboticArm.m_bones[parentIndex].GetWorldBoneRotation3D();
				}

				// Converting world delta to local delta
				Quat localDelta = parentWorldRot.QuatInverse() * rotationQuat * parentWorldRot;

				// Apply to local rotation
				Quat newLocalRot = localDelta * m_roboticArm.m_bones[jointIndex].m_localRotation;
				newLocalRot = m_roboticArm.m_bones[jointIndex].m_boneConstraint.ApplyRotationConstraint(newLocalRot);
				m_roboticArm.m_bones[jointIndex].SetLocalBoneRotation(newLocalRot);

				m_roboticArm.UpdateSkeletonPose();
			}
		}

		// Convergence check
		Vec3 newEffectorPos = m_roboticArm.m_bones[endEffector].GetWorldBonePosition3D();
		if ((newEffectorPos - clampedTargetPos).GetLength() <= threshold)
		{
			break;
		}
	}
}

void RoboticArmMode::RenderRoboticArm() const
{
	if (m_isDrawingVerts)
	{
		g_theRenderer->SetLightingConstants(m_sunDirection, SUN_INTENSITY, AMBIENT_INTENSITY);
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
		g_theRenderer->BindSampler(SamplerMode::POINT_CLAMP, 0);
		g_theRenderer->BindSampler(SamplerMode::BILINEAR_WRAP, 1);
		g_theRenderer->BindSampler(SamplerMode::BILINEAR_WRAP, 2);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->BindShader(m_shader);
		g_theRenderer->DrawVertexArray(m_roboArmVerts);
	}

	if (m_isSkeletonBeingDrawn)
	{
		g_theRenderer->SetModelConstants();
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::DISABLED);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->DrawVertexArray(m_roboSkeletonDebugVerts);
	}
}

void RoboticArmMode::GameModeAndControlsText() const
{
	std::vector<Vertex_PCU> textVerts;
	m_font->AddVertsForTextInBox2D(textVerts, "Mode (F6/F7 for Prev/Next): Robotic Arm (3D)", m_gameSceneBounds, 20.f, Rgba8::GOLD, 0.8f, Vec2(0.f, 0.965f));
	m_font->AddVertsForTextInBox2D(textVerts, "I/K: Fwd/Back, J/L: Left/Right, N/M: Up/Down", m_gameSceneBounds, 18.f, Rgba8::GOLD, 0.8f, Vec2(0.f, 0.935f));
	if (m_isArmConstrained)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "G: Toggle Skeleton, V: Toggle RoboticArm verts, H: Toggle Constraints (ON), ", m_gameSceneBounds, 18.f, Rgba8::GOLD, 0.8f, Vec2(0.f, 0.905f));
	}
	else
	{
		m_font->AddVertsForTextInBox2D(textVerts, "G: Toggle Skeleton, V: Toggle RoboticArm verts, H: Toggle Constraints (OFF), ", m_gameSceneBounds, 18.f, Rgba8::GOLD, 0.8f, Vec2(0.f, 0.905f));
	}
	m_font->AddVertsForTextInBox2D(textVerts, "1: Tex only, 2: Verts only, 3: UVs, 7/8/9: Tangent/Bitangent/Normal", m_gameSceneBounds, 18.f, Rgba8::GOLD, 0.8f, Vec2(0.f, 0.875f));
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	g_theRenderer->BindTexture(&m_font->GetTexture());
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(textVerts);
}

void RoboticArmMode::RenderGrid() const
{
	g_theRenderer->SetModelConstants();
	SetWorldConstants();
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(m_worldShader);
	g_theRenderer->DrawVertexArray(m_gridVerts);
}

void RoboticArmMode::SetWorldConstants() const
{
	WorldConstants worldConstants;
	worldConstants.CameraPosition = Vec4(m_cameraPos, 1.f);
	worldConstants.FogFarDistance = 20.f;
	worldConstants.FogNearDistance = 10.f;

	g_theRenderer->CopyCPUToGPU((void*)&worldConstants, sizeof(worldConstants), m_world_CBO);
	g_theRenderer->BindConstantBuffer(4, m_world_CBO);
}

void RoboticArmMode::DeleteBuffers()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;
}
