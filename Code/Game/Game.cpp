#include "Game/Game.h"
#include "Game/App.h"
#include "Engine/Input/InputSystem.h"

void Game::InitializeGrid()
{
	int gridSize = 1000;
	float halfSize = gridSize * 0.5f;

	for (int gridIndex = 0; gridIndex <= gridSize; ++gridIndex)
	{
		float offset = -halfSize + gridIndex;

		// Horizontal lines
		AddVertsForAABB3D(m_gridVerts,AABB3(-halfSize, offset - 0.01f, -0.005f, halfSize, offset + 0.01f, -0.075f), Rgba8::DARKGRAY);

		// Vertical lines
		AddVertsForAABB3D(m_gridVerts, AABB3(offset - 0.01f, -halfSize, -0.005f, offset + 0.01f, halfSize, -0.075f), Rgba8::DARKGRAY);
	}
}

void Game::AdjustForPauseAndTimeDistortion(float deltaSeconds) 
{
	UNUSED(deltaSeconds);

	if (g_theInput->IsKeyDown('T'))
	{
		g_theApp->m_gameClock->SetTimeScale(0.1);
	}
	else
	{
		g_theApp->m_gameClock->SetTimeScale(1.0);
	}

	if (g_theInput->WasKeyJustPressed('P'))
	{
		g_theApp->m_gameClock->TogglePause();
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
	{
		g_theEventSystem->FireEvent("Quit");
	}
}

void Game::FreeFlyControls(float deltaSeconds)
{
	// Yaw and Pitch with mouse
	m_cameraOrientation.m_yawDegrees += 0.08f * g_theInput->GetCursorClientDelta().x;
	m_cameraOrientation.m_pitchDegrees -= 0.08f * g_theInput->GetCursorClientDelta().y;

	float movementSpeed = 3.f;

	// Increase speed by a factor of 10
	if (g_theInput->IsKeyDown(KEYCODE_SHIFT))
	{
		movementSpeed *= 20.f;
	}

	// Move left or right
	if (g_theInput->IsKeyDown('A'))
	{
		m_cameraPos += movementSpeed * m_cameraOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetJBasis3D() * static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	}
	if (g_theInput->IsKeyDown('D'))
	{
		m_cameraPos += -movementSpeed * m_cameraOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetJBasis3D() * static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	}

	// Move Forward and Backward
	if (g_theInput->IsKeyDown('W'))
	{
		m_cameraPos += movementSpeed * m_cameraOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D() * static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	}
	if (g_theInput->IsKeyDown('S'))
	{
		m_cameraPos += -movementSpeed * m_cameraOrientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D() * static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	}

	// Move Up and Down
	if (g_theInput->IsKeyDown('Z'))
	{
		m_cameraPos += -movementSpeed * Vec3::ZAXE * deltaSeconds;
	}
	if (g_theInput->IsKeyDown('C'))
	{
		m_cameraPos += movementSpeed * Vec3::ZAXE * deltaSeconds;
	}
}

Vec3 Game::GetCameraFwdNormal() const
{
	return Vec3::MakeFromPolarDegrees(m_cameraOrientation.m_pitchDegrees, m_cameraOrientation.m_yawDegrees);
}
