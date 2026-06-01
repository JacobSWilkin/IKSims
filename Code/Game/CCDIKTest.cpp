#include "Game/CCDIKTest.hpp"
#include "Game/App.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Core/DebugRender.hpp"

CCDIKTest::CCDIKTest(App* owner)
	:Game(owner)
{
}

void CCDIKTest::StartUp()
{
	InitializeGrid();
	m_font = g_theRenderer->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");

	// Get world shader
	m_worldShader = g_theRenderer->CreateOrGetShader("Data/Shaders/WorldShader", VertexType::VERTEX_PCU);
	m_world_CBO = g_theRenderer->CreateConstantBuffer(sizeof(WorldConstants));

	m_skeleton = CreateTestChain();
	m_skeleton.AddVertsForSkeleton3D(m_skeletonVerts);
}

void CCDIKTest::Update()
{
	double deltaSeconds = g_theApp->m_gameClock->GetDeltaSeconds();
	double totalTime = g_theApp->m_gameClock->GetTotalSeconds();
	double frameRate = Clock::GetSystemClock().GetFrameRate();

	if (m_isTargetPlayerControlled)
	{
		TargetPositionMovement(static_cast<float>(deltaSeconds));
	}
	else
	{
		m_targetPosition = Vec3(2.0f, sinf((static_cast<float>(totalTime) * 2.f) * 2.f), 0.f);
	}

	std::vector<int> boneChain;
	for (int chainIndex = 0; chainIndex < static_cast<int>(m_skeleton.m_bones.size()); ++chainIndex)
	{
		boneChain.push_back(chainIndex);
	}
	m_skeleton.SolveCCDIK(boneChain, m_targetPosition);
	m_skeletonVerts.clear();
	m_skeleton.AddVertsForSkeleton3D(m_skeletonVerts);

	DebugAddWorldSphere(m_targetPosition, 0.25f, 0.f, Rgba8::GREEN, Rgba8::GREEN);

	// Set text for position, time, FPS, and scale
	std::string timeScaleText = Stringf("Time: %0.2fs FPS: %0.2f", totalTime, GetClamped(static_cast<float>(frameRate), 0.f, 60.f));
	DebugAddScreenText(timeScaleText, AABB2(0.f, 0.f, SCREEN_SIZE_X, SCREEN_SIZE_Y), 15.f, Vec2(0.98f, 0.97f), 0.f);
	DebugAddScreenText("CCDIK Test", m_gameSceneBounds, 25.f, Vec2(0.5f, 0.95f), 0.f);
	DebugAddScreenText("T: Toggle target position control, Move with IJKL, Up/Down: N/M", m_gameSceneBounds, 17.5f, Vec2(0.f, 0.91f), 0.f);
	DebugAddScreenText("Up Arrow  : Add Joint", m_gameSceneBounds, 17.5f, Vec2(0.f, 0.97f), 0.f);
	DebugAddScreenText("Down Arrow: Remove Joint", m_gameSceneBounds, 17.5f, Vec2(0.f, 0.94f), 0.f);

	if (g_theInput->WasKeyJustPressed(KEYCODE_UPARROW))
	{
		AddJoint();
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_DOWNARROW))
	{
		RemoveJoint();
	}
	if (g_theInput->WasKeyJustPressed('G'))
	{
		m_isGridOn = !m_isGridOn;
	}
	if (g_theInput->WasKeyJustPressed('T'))
	{
		m_isTargetPlayerControlled = !m_isTargetPlayerControlled;
	}

	AdjustForPauseAndTimeDistortion(static_cast<float>(deltaSeconds));
	UpdateCameras(static_cast<float>(deltaSeconds));
}

void CCDIKTest::Render() const
{
	g_theRenderer->BeginCamera(m_gameWorldCamera);
	g_theRenderer->ClearScreen(Rgba8(70, 70, 70, 255));
	if (m_isGridOn)
	{
		RenderGrid();
	}
	RenderChain();
	g_theRenderer->EndCamera(m_gameWorldCamera);

	DebugRenderWorld(m_gameWorldCamera);
	DebugRenderScreen(g_theApp->m_screenCamera);
}

void CCDIKTest::Shutdown()
{
	delete m_world_CBO;
	m_world_CBO = nullptr;
}

Skeleton CCDIKTest::CreateTestChain()
{
	Skeleton skeleton;
	skeleton.m_bones.clear();

	Bone first;
	first.m_boneName = "first";
	first.SetLocalBonePosition(Vec3::ZERO);
	skeleton.m_bones.push_back(first);

	Bone second;
	second.m_boneName = "second";
	second.m_parentBoneIndex = 0;
	second.SetLocalBonePosition(Vec3::ZAXE);
	skeleton.m_bones.push_back(second);

	Bone third;
	third.m_boneName = "third";
	third.m_parentBoneIndex = 1;
	third.SetLocalBonePosition(Vec3::ZAXE);
	skeleton.m_bones.push_back(third);

	Bone fourth;
	fourth.m_boneName = "fourth";
	fourth.m_parentBoneIndex = 2;
	fourth.SetLocalBonePosition(Vec3::ZAXE);
	skeleton.m_bones.push_back(fourth);

	skeleton.UpdateSkeletonPose();
	return skeleton;
}

void CCDIKTest::UpdateCameras(float deltaSeconds)
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

void CCDIKTest::TargetPositionMovement(float deltaSeconds)
{
	if (g_theInput->IsKeyDown('I'))
	{
		m_targetPosition.x += TARGETPOS_MOVE_SPEED * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('K'))
	{
		m_targetPosition.x -= TARGETPOS_MOVE_SPEED * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('J'))
	{
		m_targetPosition.y += TARGETPOS_MOVE_SPEED * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('L'))
	{
		m_targetPosition.y -= TARGETPOS_MOVE_SPEED * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('N'))
	{
		m_targetPosition.z += TARGETPOS_MOVE_SPEED * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('M'))
	{
		m_targetPosition.z -= TARGETPOS_MOVE_SPEED * static_cast<float>(deltaSeconds);
	}
}

void CCDIKTest::AddJoint()
{
	int parentIndex = static_cast<int>(m_skeleton.m_bones.size()) - 1;
	Bone newBone;
	newBone.m_parentBoneIndex = parentIndex;
	newBone.SetLocalBonePosition(Vec3::ZAXE);
	newBone.m_boneName = Stringf("bone_%d", parentIndex + 1);
	m_skeleton.m_bones.push_back(newBone);
	m_skeleton.m_bones[parentIndex].m_childBoneIndices.push_back(static_cast<int>(m_skeleton.m_bones.size()) - 1);

	m_skeleton.UpdateSkeletonPose();
}

void CCDIKTest::RemoveJoint()
{
	int boneCount = static_cast<int>(m_skeleton.m_bones.size());
	if (boneCount <= 1)
	{
		return;
	}

	int removeIndex = boneCount - 1;
	int parentIndex = m_skeleton.m_bones[removeIndex].m_parentBoneIndex;

	std::vector<unsigned int>& siblings = m_skeleton.m_bones[parentIndex].m_childBoneIndices;
	siblings.erase(std::remove(siblings.begin(), siblings.end(), static_cast<unsigned int>(removeIndex)), siblings.end());

	m_skeleton.m_bones.pop_back();
	m_skeleton.UpdateSkeletonPose();
}

void CCDIKTest::RenderChain() const
{
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(m_skeletonVerts);
}

void CCDIKTest::RenderGrid() const
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

void CCDIKTest::SetWorldConstants() const
{
	WorldConstants worldConstants;
	worldConstants.CameraPosition = Vec4(m_cameraPos, 1.f);
	worldConstants.FogFarDistance = 20.f;
	worldConstants.FogNearDistance = 10.f;

	g_theRenderer->CopyCPUToGPU((void*)&worldConstants, sizeof(worldConstants), m_world_CBO);
	g_theRenderer->BindConstantBuffer(4, m_world_CBO);
}
