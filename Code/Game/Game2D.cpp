#include "Game/Game2D.hpp"
#include "Game/App.h"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Window/Window.hpp"
#include <Engine/Core/DebugRender.hpp>

Game2D::Game2D(App* owner)
	:Game(owner)
{
}

Game2D::~Game2D()
{
}

void Game2D::StartUp()
{
	m_font = g_theRenderer->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");
	m_screenCamPos = Vec2(SCREEN_SIZE_X * 0.5f, SCREEN_SIZE_Y * 0.5f);
	m_uiCamera.SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y));
	m_cameraZoom = 1.f;

	// Initialize skeletons
	m_snakeSkeleton = CreateSnakeSkeleton2D();
	m_snakeStyle.m_boneRadius = 5.f;
	m_snakeStyle.m_jointRadius = 10.f;

	m_robotArmSkeleton = CreateRobotArmSkeleton2D();
	m_robotArmStyle.m_boneRadius = 12.f;
	m_robotArmStyle.m_jointRadius = 18.f;
	m_robotTarget = Vec2(600.f, 300.f);

	m_robotArmLengths.clear();
	for (int boneIndex = 0; boneIndex < static_cast<int>(m_robotArmSkeleton.m_bones.size() - 1); ++boneIndex)
	{
		Vec2 a = m_robotArmSkeleton.m_bones[boneIndex].GetWorldBonePosition2D();
		Vec2 b = m_robotArmSkeleton.m_bones[boneIndex + 1].GetWorldBonePosition2D();
		m_robotArmLengths.push_back((b - a).GetLength());
	}

	m_spiderBodyPos = Vec2(SCREEN_SIZE_X * 0.5f, SCREEN_SIZE_Y * 0.5f);

	m_legs.clear();
	static float spiderAngles[8] =
	{
		35.f,   15.f,  -15.f,  -35.f,
		180.f - 35.f, 180.f - 15.f, 180.f + 15.f, 180.f + 35.f
	};

	for (int legIndex = 0; legIndex < 8; ++legIndex)
	{
		m_legAngles[legIndex] = spiderAngles[legIndex];
	}

	for (int spiderLegIndex = 0; spiderLegIndex < 8; ++spiderLegIndex)
	{
		float angleDeg = spiderAngles[spiderLegIndex];
		Vec2 offset = Vec2::MakeFromPolarDegrees(angleDeg, SPIDER_LEG_RADIUS);

		SpiderLeg2D leg;

		leg.footPos = m_spiderBodyPos + offset;
		leg.footTarget = leg.footPos;
		leg.footStart = leg.footPos;

		leg.stepT = 1.f;

		// 3-bone leg
		leg.lengths = { 50.f, 50.f, 50.f };

		Vec2 dir = offset.GetNormalized();

		leg.joints.resize(4);
		leg.joints[0] = m_spiderBodyPos;
		leg.joints[1] = leg.joints[0] + dir * leg.lengths[0];
		leg.joints[2] = leg.joints[1] + dir * leg.lengths[1];
		leg.joints[3] = leg.joints[2] + dir * leg.lengths[2];

		m_legs.push_back(leg);
	}
}

void Game2D::Update()
{
	double deltaSeconds = g_theApp->m_gameClock->GetDeltaSeconds();

	if (m_mode == GameMode2D::SPIDER_2D)
	{
		UpdateSpider(static_cast<float>(deltaSeconds));
	}
	else if (m_mode == GameMode2D::ROBOT_ARM)
	{
		UpdateRobotArm(static_cast<float>(deltaSeconds));
	}
	else if (m_mode == GameMode2D::SNAKE_CHAIN)
	{
		UpdateSnake(static_cast<float>(deltaSeconds));
	}

	CameraKeyPresses(static_cast<float>(deltaSeconds));
	AdjustForPauseAndTimeDistortion(static_cast<float>(deltaSeconds));

	if (g_theInput->WasKeyJustPressed(KEYCODE_F1))
	{
		int cameraMode = static_cast<int>(m_mode);
		cameraMode = (cameraMode + 1) % static_cast<int>(GameMode2D::NUM_2D_MODES);
		m_mode = static_cast<GameMode2D>(cameraMode);
	}

	UpdateCameras();
}

void Game2D::UpdateSpider(float deltaSeconds)
{
	// Move spider body with mouse
	Vec2 mousePos = g_theInput->GetCursorClientPosition();
	Vec2 mouseWorldPos = g_theApp->m_screenCamera.GetClientToWorld(mousePos, g_theWindow->GetClientDimensions());

	static Vec2 prevBodyPos = m_spiderBodyPos;

	Vec2 delta = mouseWorldPos - prevBodyPos;
	float maxSpeed = 600.f;
	float maxMove = maxSpeed * deltaSeconds;

	if (delta.GetLength() > maxMove)
	{
		delta = delta.GetNormalized() * maxMove;
	}

	m_spiderBodyPos = prevBodyPos + delta;
	prevBodyPos = m_spiderBodyPos;

	float stepThreshold = 10.f;
	bool evenPhase = ((int)(g_theApp->m_gameClock->GetTotalSeconds() * 2.f)) % 2 == 0;

	for (int legIndex = 0; legIndex < static_cast<int>(m_legs.size()); ++legIndex)
	{
		float angleDeg = m_legAngles[legIndex];
		Vec2 idealPos = m_spiderBodyPos + Vec2::MakeFromPolarDegrees(angleDeg, SPIDER_LEG_RADIUS);

		SpiderLeg2D& leg = m_legs[legIndex];
		float distToIdeal = (leg.footPos - idealPos).GetLength();
		bool isEven = (legIndex % 2 == 0);

		// Trigger new step
		if (leg.stepT >= 1.f && distToIdeal > stepThreshold)
		{
			if (isEven == evenPhase)
			{
				leg.footStart = leg.footPos;
				leg.footTarget = idealPos;
				leg.stepT = 0.f;
			}
		}

		// Animate step
		// If stepping
		if (leg.stepT < 1.f)
		{
			leg.stepT += deltaSeconds * 3.5f;
			leg.stepT = GetClampedZeroToOne(leg.stepT);

			float t = SmoothStep3(leg.stepT);

			// Base linear movement
			Vec2 basePos = Interpolate(leg.footStart, leg.footTarget, t);

			leg.footPos = basePos;
		}
		else
		{
			// Foot stays planted
			leg.footPos = leg.footPos;
		}

		// Hard clamp reach
		float maxReach = 150.f;
		Vec2 toFoot = leg.footPos - m_spiderBodyPos;

		if (toFoot.GetLength() > maxReach)
			leg.footPos = m_spiderBodyPos + toFoot.GetNormalized() * maxReach;

		for (int iterationIndex = 0; iterationIndex < 3; ++iterationIndex)
		{
			SolveFABRIK2D(leg, m_spiderBodyPos, leg.footPos);
		}
	}

	if (g_theInput->WasKeyJustPressed('T'))
	{
		m_showFootTargets = !m_showFootTargets;
	}
}

void Game2D::UpdateRobotArm(float deltaSeconds)
{
	UNUSED(deltaSeconds);
	Vec2 mousePos = g_theInput->GetCursorClientPosition();
	Vec2 mouseWorld = g_theApp->m_screenCamera.GetClientToWorld(mousePos, g_theWindow->GetClientDimensions());

	if (g_theInput->IsKeyDown(KEYCODE_LEFT_MOUSE))
	{
		m_robotTarget = mouseWorld;
	}

	// Run FABRIK iterations
	for (int iterationIndex = 0; iterationIndex < 10; ++iterationIndex)
	{
		SolveFABRIKRobotArm(m_robotArmSkeleton, m_robotTarget);
	}

	m_robotArmSkeleton.UpdateSkeletonPose();

	m_skeletonVerts.clear();
	m_robotArmSkeleton.AddVertsForSkeleton2D(m_skeletonVerts, m_robotArmStyle);
}

void Game2D::UpdateSnake(float deltaSeconds)
{
	Vec2 moveDirection = Vec2::ZERO;

	if (g_theInput->IsKeyDown('I')) moveDirection.y += 1.f;
	if (g_theInput->IsKeyDown('K')) moveDirection.y -= 1.f;
	if (g_theInput->IsKeyDown('J')) moveDirection.x -= 1.f;
	if (g_theInput->IsKeyDown('L')) moveDirection.x += 1.f;

	if (moveDirection.GetLengthSquared() > 0.f)
	{
		moveDirection.Normalize();

		// Direction of first body segment
		Vec2 head = m_snakeSkeleton.m_bones[0].m_localPosition.GetXY();
		Vec2 neck = m_snakeSkeleton.m_bones[1].m_localPosition.GetXY();

		Vec2 bodyDirection = (neck - head).GetNormalized();

		// Check for reversal or trying to move into ourself
		if (DotProduct2D(moveDirection, bodyDirection) > 0.9f)
		{
			moveDirection = Vec2::ZERO;
		}
	}

	Vec3& rootPositionAsVec3 = m_snakeSkeleton.m_bones[0].m_localPosition;
	Vec2 headPosition = rootPositionAsVec3.GetXY();

	headPosition += moveDirection * SNAKE_HEAD_SPEED * deltaSeconds;
	rootPositionAsVec3.x = headPosition.x;
	rootPositionAsVec3.y = headPosition.y;

	// Ensuring snake stays at fixed segment length
	for (int jointIndex = 1; jointIndex < static_cast<int>(m_snakeSkeleton.m_bones.size()); ++jointIndex)
	{
		Vec3& previousPosAsVec3 = m_snakeSkeleton.m_bones[jointIndex - 1].m_localPosition;
		Vec3& currentPosAsVec3 = m_snakeSkeleton.m_bones[jointIndex].m_localPosition;
		Vec2 previousPosition = previousPosAsVec3.GetXY();
		Vec2 currentPosition = currentPosAsVec3.GetXY();

		Vec2 snakeDirection = currentPosition - previousPosition;
		if (snakeDirection.GetLengthSquared() == 0.f)
		{
			snakeDirection = Vec2::ONE_TO_ZERO;
		}
		snakeDirection.Normalize();

		currentPosition = previousPosition + snakeDirection * SNAKE_SEGMENT_SPACING;
		currentPosAsVec3.x = currentPosition.x;
		currentPosAsVec3.y = currentPosition.y;
	}

	// Body collision push out
	float bodyRadius = SNAKE_SEGMENT_SPACING * 0.6f;
	for (int posAIndex = 0; posAIndex < static_cast<int>(m_snakeSkeleton.m_bones.size()); ++posAIndex)
	{
		Vec2 positionA = m_snakeSkeleton.m_bones[posAIndex].m_localPosition.GetXY();
		for (int posBIndex = posAIndex + 2; posBIndex < static_cast<int>(m_snakeSkeleton.m_bones.size()); ++posBIndex)
		{
			Vec2 positionB = m_snakeSkeleton.m_bones[posBIndex].m_localPosition.GetXY();
			Vec2 delta = positionB - positionA;
			float dist = delta.GetLength();

			if (dist < bodyRadius && dist > 0.f)
			{
				Vec2 pushDir = delta / dist;
				Vec2 push = pushDir * (bodyRadius - dist) * 0.5f;

				m_snakeSkeleton.m_bones[posAIndex].m_localPosition -= Vec3(push.x, push.y, 0.f);
				m_snakeSkeleton.m_bones[posBIndex].m_localPosition += Vec3(push.x, push.y, 0.f);
			}
		}
	}

	// Update skeleton
	m_snakeSkeleton.UpdateSkeletonPose();
	m_skeletonVerts.clear();
	m_snakeSkeleton.AddVertsForSkeleton2D(m_skeletonVerts, m_snakeStyle);
}

void Game2D::UpdateCameras()
{
	float halfWidth = (SCREEN_SIZE_X * 0.5f) * m_cameraZoom;
	float halfHeight = (SCREEN_SIZE_Y * 0.5f) * m_cameraZoom;

	Vec2 mins = m_screenCamPos - Vec2(halfWidth, halfHeight);
	Vec2 maxs = m_screenCamPos + Vec2(halfWidth, halfHeight);

	g_theApp->m_screenCamera.SetOrthoView(mins, maxs);
}

void Game2D::Render() const
{
	g_theRenderer->ClearScreen(Rgba8::BLACK);
	g_theRenderer->BeginCamera(g_theApp->m_screenCamera);
	if (m_mode == GameMode2D::SPIDER_2D)
	{
		RenderSpider();
	}
	else if (m_mode == GameMode2D::ROBOT_ARM)
	{
		RenderRobotArm();
	}
	else if (m_mode == GameMode2D::SNAKE_CHAIN)
	{
		RenderSnake();
	}
	g_theRenderer->EndCamera(g_theApp->m_screenCamera);

	g_theRenderer->BeginCamera(m_uiCamera);
	GameModeAndControlsText();
	g_theRenderer->EndCamera(m_uiCamera);

	DebugRenderScreen(g_theApp->m_screenCamera);
}

void Game2D::RenderRobotArm() const
{
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(m_skeletonVerts);

	std::vector<Vertex_PCU> verts;

	AddVertsForDisc2D(verts, m_robotTarget, 10.f, Rgba8::RED);

	g_theRenderer->DrawVertexArray(verts);
}

void Game2D::RenderSnake() const
{
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(m_skeletonVerts);

	// Eye drawing
	std::vector<Vertex_PCU> verts;
	Vec2 head = m_snakeSkeleton.m_bones[0].m_localPosition.GetXY();

	AddVertsForDisc2D(verts, head + Vec2(-4.f, 3.f), 3.f, Rgba8::RED);
	AddVertsForDisc2D(verts, head + Vec2(4.f, 3.f), 3.f, Rgba8::RED);

	g_theRenderer->DrawVertexArray(verts);
}

void Game2D::RenderSpider() const
{
	std::vector<Vertex_PCU> verts;

	// Draw body
	AddVertsForDisc2D(verts, m_spiderBodyPos, 20.f, Rgba8::WHITE);

	for (int legIndex = 0; legIndex < static_cast<int>(m_legs.size()); ++legIndex)
	{
		SpiderLeg2D const& leg = m_legs[legIndex];

		// Draw leg line
		for (int legJointIndex = 0; legJointIndex < static_cast<int>(leg.joints.size() - 1); ++legJointIndex)
		{
			AddVertsForLineSegment2D(verts, leg.joints[legJointIndex], leg.joints[legJointIndex + 1], 6.f, Rgba8(150, 75, 0));
			AddVertsForDisc2D(verts, leg.joints[legJointIndex], 6.f, Rgba8(180, 120, 60));
		}

		// Draw foot
		AddVertsForDisc2D(verts, leg.footPos, 12.f, Rgba8(200, 100, 50));

		// Debug foot targets
		if (m_showFootTargets)
		{
			Vec2 idealPos = leg.footTarget;
			float dist = (leg.footPos - idealPos).GetLength();

			Rgba8 color = (dist < 1.0f) ? Rgba8::GREEN : Rgba8::RED;
			AddVertsForDisc2D(verts, idealPos, 6.f, color);
		}
	}

	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(verts);
}

void Game2D::Shutdown()
{
}

Skeleton Game2D::CreateRobotArmSkeleton2D()
{
	Skeleton skeleton;
	float spacing = 240.f;

	Bone base;
	base.m_boneName = "Base";
	base.m_localPosition = Vec3(400.f, 200.f, 0.f);
	skeleton.m_bones.push_back(base);

	Bone shoulder;
	shoulder.m_boneName = "Shoulder";
	shoulder.m_parentBoneIndex = 0;
	shoulder.m_localPosition = Vec3(spacing, 0.f, 0.f);
	skeleton.m_bones.push_back(shoulder);

	Bone elbow;
	elbow.m_boneName = "Elbow";
	elbow.m_parentBoneIndex = 1;
	elbow.m_localPosition = Vec3(spacing, 0.f, 0.f);
	skeleton.m_bones.push_back(elbow);

	Bone wrist;
	wrist.m_boneName = "Wrist";
	wrist.m_parentBoneIndex = 2;
	wrist.m_localPosition = Vec3(spacing, 0.f, 0.f);
	skeleton.m_bones.push_back(wrist);

	skeleton.UpdateSkeletonPose();
	return skeleton;
}

Skeleton Game2D::CreateSnakeSkeleton2D()
{
	Skeleton skeleton;
	float spacing = 20.f;

	for (int boneIndex = 0; boneIndex < SNAKE_BONE_COUNT; ++boneIndex)
	{
		Bone bone;
		bone.m_boneName = Stringf("Bone%d", boneIndex);
		bone.m_parentBoneIndex = -1;
		bone.m_localPosition = Vec3(400.f - spacing * boneIndex, 300.f, 0.f);
		skeleton.m_bones.push_back(bone);
	}

	skeleton.UpdateSkeletonPose();
	return skeleton;
}

void Game2D::CameraKeyPresses(float deltaSeconds)
{
	float movementSpeed = 500.f;
	float zoomSpeed = 1.5f;

	if (g_theInput->IsKeyDown('A'))
	{
		m_screenCamPos.x -= movementSpeed * deltaSeconds;
	}
	if (g_theInput->IsKeyDown('D'))
	{
		m_screenCamPos.x += movementSpeed * deltaSeconds;
	}
	if (g_theInput->IsKeyDown('W'))
	{
		m_screenCamPos.y += movementSpeed * deltaSeconds;
	}
	if (g_theInput->IsKeyDown('S'))
	{
		m_screenCamPos.y -= movementSpeed * deltaSeconds;
	}

	// Zoom
	if (g_theInput->IsKeyDown('Q'))
	{
		// Zoom in
		m_cameraZoom -= zoomSpeed * deltaSeconds;
	}

	if (g_theInput->IsKeyDown('E'))
	{
		// Zoom out
		m_cameraZoom += zoomSpeed * deltaSeconds;
	}

	m_cameraZoom = GetClamped(m_cameraZoom, CAMERA_MIN_ZOOM, CAMERA_MAX_ZOOM);
}

void Game2D::GameModeAndControlsText() const
{
	std::vector<Vertex_PCU> textVerts;
	m_font->AddVertsForTextInBox2D(textVerts, "Mode (F6/F7 for Prev/Next): Game2D, (F1) Cycle 2D modes", m_gameSceneBounds, 20.f, Rgba8::GOLD, 0.8f, Vec2(0.f, 0.965f));
	m_font->AddVertsForTextInBox2D(textVerts, "WASD: Move 2D camera around, Q/E: Zoom In/Out", m_gameSceneBounds, 15.f, Rgba8::GOLD, 0.8f, Vec2(0.f, 0.935f));
	if (m_mode == GameMode2D::ROBOT_ARM)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "Mode: Interactive 2D FABRIK chain, Hold LMB to drag target", m_gameSceneBounds, 15.f, Rgba8::GOLD, 0.8f, Vec2(0.f, 0.905f));
	}
	else if (m_mode == GameMode2D::SPIDER_2D)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "Mode: 2D Spider, Move body with cursor, T: Toggle foot targets", m_gameSceneBounds, 15.f, Rgba8::GOLD, 0.8f, Vec2(0.f, 0.905f));
	}
	else if (m_mode == GameMode2D::SNAKE_CHAIN)
	{
		m_font->AddVertsForTextInBox2D(textVerts, "Mode: 2D Snake chain, IJKL: Move Snake N/W/S/E", m_gameSceneBounds, 15.f, Rgba8::GOLD, 0.8f, Vec2(0.f, 0.905f));
	}
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	g_theRenderer->BindTexture(&m_font->GetTexture());
	g_theRenderer->DrawVertexArray(textVerts);
}

void Game2D::SolveFABRIK2D(SpiderLeg2D& leg, Vec2 const& root, Vec2 const& target)
{
	int jointCount = static_cast<int>(leg.joints.size());

	// If target unreachable to stretch toward it
	float totalLength = 0.f;
	for (float legLength : leg.lengths)
	{
		totalLength += legLength;
	}

	if ((target - root).GetLength() > totalLength)
	{
		Vec2 dir = (target - root).GetNormalized();

		leg.joints[0] = root;
		for (int jointIndex = 1; jointIndex < jointCount; ++jointIndex)
		{
			leg.joints[jointIndex] = leg.joints[jointIndex - 1] + dir * leg.lengths[jointIndex - 1];
		}
		return;
	}

	// BACKWARD PASS
	leg.joints[jointCount - 1] = target;

	for (int jointIndex = jointCount - 2; jointIndex >= 0; --jointIndex)
	{
		Vec2 dir = (leg.joints[jointIndex] - leg.joints[jointIndex + 1]).GetNormalized();
		leg.joints[jointIndex] = leg.joints[jointIndex + 1] + dir * leg.lengths[jointIndex];
	}

	// FORWARD PASS
	leg.joints[0] = root;

	for (int jointIndex = 1; jointIndex < jointCount; ++jointIndex)
	{
		Vec2 dir = (leg.joints[jointIndex] - leg.joints[jointIndex - 1]).GetNormalized();
		leg.joints[jointIndex] = leg.joints[jointIndex - 1] + dir * leg.lengths[jointIndex - 1];
	}
}

void Game2D::SolveFABRIKRobotArm(Skeleton& skeleton, Vec2 const& target)
{
	int jointCount = static_cast<int>(skeleton.m_bones.size());
	if (jointCount < 2)
	{
		return;
	}

	std::vector<Vec2> joints;
	joints.resize(jointCount);
	for (int jointIndex = 0; jointIndex < jointCount; ++jointIndex)
	{
		joints[jointIndex] = skeleton.m_bones[jointIndex].GetWorldBonePosition2D();
	}

	float totalLength = 0.f;
	for (float len : m_robotArmLengths)
	{
		totalLength += len;
	}

	Vec2 root = skeleton.m_bones[0].GetWorldBonePosition2D();

	// Unreachable target
	if ((target - root).GetLength() > totalLength)
	{
		Vec2 dir = (target - root).GetNormalized();

		for (int jointIndex = 1; jointIndex < jointCount; ++jointIndex)
		{
			joints[jointIndex] = joints[jointIndex - 1] + dir * m_robotArmLengths[jointIndex - 1];
		}
	}
	else
	{
		// BACKWARD PASS
		joints[jointCount - 1] = target;

		for (int jointIndex = jointCount - 2; jointIndex >= 0; --jointIndex)
		{
			Vec2 dir = (joints[jointIndex] - joints[jointIndex + 1]).GetNormalized();
			joints[jointIndex] = joints[jointIndex + 1] + dir * m_robotArmLengths[jointIndex];
		}

		// FORWARD PASS
		joints[0] = root;

		for (int jointIndex = 1; jointIndex < jointCount; ++jointIndex)
		{
			Vec2 dir = (joints[jointIndex] - joints[jointIndex - 1]).GetNormalized();
			joints[jointIndex] = joints[jointIndex - 1] + dir * m_robotArmLengths[jointIndex - 1];
		}
	}

	// Write results back into skeleton
	for (int jointIndex = 0; jointIndex < jointCount; ++jointIndex)
	{
		if (skeleton.m_bones[jointIndex].m_parentBoneIndex < 0)
		{
			skeleton.m_bones[jointIndex].m_localPosition.x = joints[jointIndex].x;
			skeleton.m_bones[jointIndex].m_localPosition.y = joints[jointIndex].y;
		}
		else
		{
			int parent = skeleton.m_bones[jointIndex].m_parentBoneIndex;

			Vec2 parentWorld = skeleton.m_bones[parent].GetWorldBonePosition2D();
			Vec2 local = joints[jointIndex] - parentWorld;

			skeleton.m_bones[jointIndex].m_localPosition.x = local.x;
			skeleton.m_bones[jointIndex].m_localPosition.y = local.y;
		}
	}
}
