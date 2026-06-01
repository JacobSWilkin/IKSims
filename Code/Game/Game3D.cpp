#include "Game/Game3D.hpp"
#include "Game/App.h"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Core/NamedProperties.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Input/InputSystem.h"

Game3D::Game3D(App* owner)
	:Game(owner)
{
}

void Game3D::StartUp()
{
	InitializeGrid();

	// Setting initial camera position
	m_cameraPos = Vec3(-6.5f, -0.5f, 3.f);

	// Get world shader
	m_litShader = g_theRenderer->CreateOrGetShader("Data/Shaders/BlinnPhong", VertexType::VERTEX_PCUTBN);
	m_worldShader = g_theRenderer->CreateOrGetShader("Data/Shaders/WorldShader", VertexType::VERTEX_PCU);
	m_world_CBO = g_theRenderer->CreateConstantBuffer(sizeof(WorldConstants));

	// Setup text
	m_font = g_theRenderer->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");
	m_font->AddVertsForTextInBox2D(m_textVerts, "Mode (F6/F7 for Prev/Next): Two-Bone IK Test (3D)", m_gameSceneBounds, 17.5f, Rgba8::ALICEBLUE, 0.8f, Vec2(0.35f, 0.965f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[1] Switch Arms", m_gameSceneBounds, 17.5f, Rgba8::ALICEBLUE, 0.8f, Vec2(0.258f, 0.925f));
	m_font->AddVertsForTextInBox2D(m_textVerts, "[2] Switch IK on/off, off animates freely", m_gameSceneBounds, 17.5f, Rgba8::ALICEBLUE, 0.8f, Vec2(0.35f, 0.9f));

	// Initialize skeleton
	m_skeleton = CreateTestSkeleton();
	m_skeleton.AddVertsForSkeleton3D(m_skeletonVerts);
	float yPosition = SCREEN_SIZE_Y - 30.f;
	m_skeleton.AddVertsForBoneHierarchy(m_textVerts, *m_font, yPosition);
}

void Game3D::Update()
{
	// Setting clock time variables
	double deltaSeconds = g_theApp->m_gameClock->GetDeltaSeconds();
	double totalTime = g_theApp->m_gameClock->GetTotalSeconds();
	double frameRate = Clock::GetSystemClock().GetFrameRate();

	// Toggle free animation
	if (g_theInput->WasKeyJustPressed('2'))
	{
		m_isAnimatingFreely = !m_isAnimatingFreely;
	}

	TargetPosKeyPresses(deltaSeconds);

	if (!m_isAnimatingFreely)
	{
		ToggleArms();
	}
	else
	{
		AnimateSkeleton(static_cast<float>(deltaSeconds));
	}

	m_skeletonVerts.clear();
	m_skeleton.AddVertsForSkeleton3D(m_skeletonVerts);
	DebugAddWorldSphere(m_targetPos, 0.25f, 0.f, Rgba8::GREEN, Rgba8::GREEN);

	// Set text for position, time, FPS, and scale
	std::string timeScaleText = Stringf("Time: %0.2fs FPS: %0.2f", totalTime, GetClamped(static_cast<float>(frameRate), 0.f, 60.f));
	DebugAddScreenText(timeScaleText, AABB2(0.f, 0.f, SCREEN_SIZE_X, SCREEN_SIZE_Y), 15.f, Vec2(0.98f, 0.97f), 0.f);

	// Constraint text
	//if (m_areConstraintsOn)
	//{
	//	DebugAddScreenText("[H] Constraints on/off (On)", m_gameSceneBounds, 15.f, Vec2(0.3f, 0.875f), 0.f);
	//}
	//else
	//{
	//	DebugAddScreenText("[H] Constraints on/off (Off)", m_gameSceneBounds, 15.f, Vec2(0.3f, 0.875f), 0.f);
	//}

	if (g_theInput->WasKeyJustPressed('G'))
	{
		m_isGridOn = !m_isGridOn;
	}

	AdjustForPauseAndTimeDistortion(static_cast<float>(deltaSeconds));
	UpdateCameras(static_cast<float>(deltaSeconds));
}

void Game3D::ToggleArms()
{
	if (m_rightHandSelected)
	{
		if (m_areConstraintsOn)
		{
			m_skeleton.SolveTwoBoneIKConstrained(4, 6, 7, m_targetPos, Vec3::XAXE);
		}
		else
		{
			m_skeleton.SolveTwoBoneIK(4, 6, 7, m_targetPos, Vec3::XAXE);
		}

		DebugAddWorldSphere(m_skeleton.GetBoneByIndex(4)->GetWorldBonePosition3D(), 0.25f, 0.f, Rgba8::RED, Rgba8::RED);
		DebugAddWorldSphere(m_skeleton.GetBoneByIndex(6)->GetWorldBonePosition3D(), 0.25f, 0.f, Rgba8::RED, Rgba8::RED);
		DebugAddWorldSphere(m_skeleton.GetBoneByIndex(7)->GetWorldBonePosition3D(), 0.25f, 0.f, Rgba8::RED, Rgba8::RED);
	}
	else
	{
		if (m_areConstraintsOn)
		{
			m_skeleton.SolveTwoBoneIKConstrained(3, 5, 8, m_targetPos, Vec3::XAXE);
		}
		else
		{
			m_skeleton.SolveTwoBoneIK(3, 5, 8, m_targetPos, Vec3::XAXE);
		}

		DebugAddWorldSphere(m_skeleton.GetBoneByIndex(3)->GetWorldBonePosition3D(), 0.25f, 0.f, Rgba8::RED, Rgba8::RED);
		DebugAddWorldSphere(m_skeleton.GetBoneByIndex(5)->GetWorldBonePosition3D(), 0.25f, 0.f, Rgba8::RED, Rgba8::RED);
		DebugAddWorldSphere(m_skeleton.GetBoneByIndex(8)->GetWorldBonePosition3D(), 0.25f, 0.f, Rgba8::RED, Rgba8::RED);
	}

	if (g_theInput->WasKeyJustPressed('1'))
	{
		m_rightHandSelected = !m_rightHandSelected;
	}

	ToggleConstraints();
}

void Game3D::ToggleConstraints()
{
	if (g_theInput->WasKeyJustPressed('H'))
	{
		m_areConstraintsOn = !m_areConstraintsOn;
	}
}

void Game3D::TargetPosKeyPresses(double deltaSeconds)
{
	if (g_theInput->IsKeyDown('I'))
	{
		m_targetPos.x += 2.f * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('K'))
	{
		m_targetPos.x -= 2.f * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('J'))
	{
		m_targetPos.y += 2.f * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('L'))
	{
		m_targetPos.y -= 2.f * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('N'))
	{
		m_targetPos.z += 2.f * static_cast<float>(deltaSeconds);
	}
	if (g_theInput->IsKeyDown('M'))
	{
		m_targetPos.z -= 2.f * static_cast<float>(deltaSeconds);
	}
}

void Game3D::Render() const
{
	g_theRenderer->BeginCamera(m_gameWorldCamera);
	g_theRenderer->ClearScreen(Rgba8(70, 70, 70, 255));
	if (m_isGridOn)
	{
		RenderGrid();
	}
	RenderSkeleton();
	g_theRenderer->EndCamera(m_gameWorldCamera);

	g_theRenderer->BeginCamera(g_theApp->m_screenCamera);
	PrintBoneHierarchy();
	g_theRenderer->EndCamera(g_theApp->m_screenCamera);

	DebugRenderWorld(m_gameWorldCamera);
	DebugRenderScreen(g_theApp->m_screenCamera);
}

void Game3D::Shutdown()
{
	delete m_world_CBO;
	m_world_CBO = nullptr;
}

Skeleton Game3D::CreateTestSkeleton()
{
	Skeleton skeleton;
	skeleton.m_bones.clear();

	Bone root;
	root.m_boneName = "Root/Hip";
	root.SetLocalBonePosition(Vec3::ZERO);
	skeleton.m_bones.push_back(root);

	Bone childBone4;
	childBone4.m_boneName = "Body";
	childBone4.m_parentBoneIndex = 0;
	childBone4.SetLocalBonePosition(Vec3(0.f, 0.f, 3.f));
	skeleton.m_bones.push_back(childBone4);

	Bone childBone6;
	childBone6.m_boneName = "Head";
	childBone6.m_parentBoneIndex = 1;
	childBone6.SetLocalBonePosition(Vec3::ZAXE);
	skeleton.m_bones.push_back(childBone6);

	Bone childBone7;
	childBone7.m_boneName = "LShoulder";
	childBone7.m_parentBoneIndex = 1;
	childBone7.SetLocalBonePosition(Vec3::YAXE);
	childBone7.m_boneConstraint.m_rotationConstraints[0] = CONSTRAINT_TYPE::LIMITED;
	childBone7.m_boneConstraint.m_rotationConstraints[1] = CONSTRAINT_TYPE::LIMITED;
	childBone7.m_boneConstraint.m_rotationConstraints[2] = CONSTRAINT_TYPE::LOCKED;
	childBone7.m_boneConstraint.m_minRotationDegrees = EulerAngles(-45.f, -45.f, 0.f);
	childBone7.m_boneConstraint.m_maxRotationDegrees = EulerAngles(45.f, 45.f, 0.f);
	skeleton.m_bones.push_back(childBone7);

	Bone childBone8;
	childBone8.m_boneName = "RShoulder";
	childBone8.m_parentBoneIndex = 1;
	childBone8.SetLocalBonePosition(-Vec3::YAXE);
	childBone8.m_boneConstraint.m_rotationConstraints[0] = CONSTRAINT_TYPE::LIMITED;
	childBone8.m_boneConstraint.m_rotationConstraints[1] = CONSTRAINT_TYPE::LIMITED;
	childBone8.m_boneConstraint.m_rotationConstraints[2] = CONSTRAINT_TYPE::LOCKED;
	childBone8.m_boneConstraint.m_minRotationDegrees = EulerAngles(-45.f, -45.f, 0.f);
	childBone8.m_boneConstraint.m_maxRotationDegrees = EulerAngles(45.f, 45.f, 0.f);
	skeleton.m_bones.push_back(childBone8);

	Bone childBone9;
	childBone9.m_boneName = "LArm";
	childBone9.m_parentBoneIndex = 3;
	childBone9.SetLocalBonePosition(Vec3(0.f, 0.5f, -1.f));
	childBone9.m_boneConstraint.m_rotationConstraints[0] = CONSTRAINT_TYPE::LIMITED;
	childBone9.m_boneConstraint.m_rotationConstraints[1] = CONSTRAINT_TYPE::LIMITED;
	childBone9.m_boneConstraint.m_rotationConstraints[2] = CONSTRAINT_TYPE::LOCKED;
	childBone9.m_boneConstraint.m_minRotationDegrees = EulerAngles(-90.f, 0.f, 0.f);
	childBone9.m_boneConstraint.m_maxRotationDegrees = EulerAngles(90.f, 0.f, 0.f);
	skeleton.m_bones.push_back(childBone9);

	Bone childBone10;
	childBone10.m_boneName = "RArm";
	childBone10.m_parentBoneIndex = 4;
	childBone10.SetLocalBonePosition(Vec3(0.f, -0.5f, -1.f));
	childBone10.m_boneConstraint.m_rotationConstraints[0] = CONSTRAINT_TYPE::LIMITED;
	childBone10.m_boneConstraint.m_rotationConstraints[1] = CONSTRAINT_TYPE::LIMITED;
	childBone10.m_boneConstraint.m_rotationConstraints[2] = CONSTRAINT_TYPE::LOCKED;
	childBone10.m_boneConstraint.m_minRotationDegrees = EulerAngles(-90.f, 0.f, 0.f);
	childBone10.m_boneConstraint.m_maxRotationDegrees = EulerAngles(90.f, 0.f, 0.f);
	skeleton.m_bones.push_back(childBone10);

	Bone childBone11;
	childBone11.m_boneName = "RHand";
	childBone11.m_parentBoneIndex = 6;
	childBone11.SetLocalBonePosition(Vec3(0.f, -0.5f, -0.5f));
	skeleton.m_bones.push_back(childBone11);

	Bone childBone12;
	childBone12.m_boneName = "LHand";
	childBone12.m_parentBoneIndex = 5;
	childBone12.SetLocalBonePosition(Vec3(0.f, 0.5f, -0.5f));
	skeleton.m_bones.push_back(childBone12);

	skeleton.UpdateSkeletonPose();

	return skeleton;
}

void Game3D::AnimateSkeleton(float deltaSeconds)
{
	static float elapsedTime = 0.f;
	elapsedTime += deltaSeconds;

	Bone* leftShoulder = m_skeleton.GetBoneByName("LShoulder");
	Bone* rightShoulder = m_skeleton.GetBoneByName("RShoulder");

	float swingAngleDegrees = 2.f * sinf(elapsedTime);

	Quat leftArmRotation = Quat::MakeFromAxisAngle(Vec3::XAXE, swingAngleDegrees);
	Quat rightArmRotation = Quat::MakeFromAxisAngle(Vec3::XAXE, -swingAngleDegrees);

	leftShoulder->SetLocalBoneRotation(leftArmRotation);
	rightShoulder->SetLocalBoneRotation(rightArmRotation);

	m_skeleton.UpdateSkeletonPose();

	m_skeletonVerts.clear();
	m_skeleton.AddVertsForSkeleton3D(m_skeletonVerts);
}

void Game3D::UpdateCameras(float deltaSeconds)
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

void Game3D::RenderSkeleton() const
{
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(m_skeletonVerts);
}

void Game3D::PrintBoneHierarchy() const
{
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->BindTexture(&m_font->GetTexture());
	g_theRenderer->DrawVertexArray(m_textVerts);

	std::vector<Vertex_PCU> quadVerts;
	AddVertsForAABB2D(quadVerts, AABB2(Vec2(0.f, 575.f), Vec2(250.f, 800.f)), Rgba8(0, 0, 0, 80));
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->DrawVertexArray(quadVerts);
}

void Game3D::SetWorldConstants() const
{
	WorldConstants worldConstants;
	worldConstants.CameraPosition = Vec4(m_cameraPos, 1.f);
	worldConstants.FogFarDistance = 20.f;
	worldConstants.FogNearDistance = 10.f;

	g_theRenderer->CopyCPUToGPU((void*)&worldConstants, sizeof(worldConstants), m_world_CBO);
	g_theRenderer->BindConstantBuffer(4, m_world_CBO);
}

void Game3D::RenderGrid() const
{
	g_theRenderer->SetModelConstants();
	SetWorldConstants();
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_ONLY_LESS_EQUAL);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(m_worldShader);
	g_theRenderer->DrawVertexArray(m_gridVerts);
}
