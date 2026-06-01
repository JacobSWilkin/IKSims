#include "Game/Terrain.hpp"
#include "Game/GameCommon.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Math/SmoothNoise.hpp"
#include "Engine/Math/MathUtils.h"

Terrain::Terrain(Vec3 const& position)
	:m_terrainWorldPosition(position)
{
	m_shader = g_theRenderer->CreateOrGetShader("Data/Shaders/BlinnPhong", VertexType::VERTEX_PCUTBN);
	m_constantBuffer = g_theRenderer->CreateConstantBuffer(sizeof(FogConstants));
	m_terrainBounds.m_mins = Vec3::ZERO;
	m_terrainBounds.m_maxs = Vec3(150.f, 150.f, 20.f);
}

Terrain::~Terrain()
{
	delete m_constantBuffer;
	m_constantBuffer = nullptr;

	DeleteBuffers();
}

void Terrain::InitializeTerrainHills()
{
	// Initialize terrain
	GenerateHeightMap();
	GenerateMesh();
	CreateBuffers();
}

void Terrain::Render() const
{
	if (m_vertexBuffer == nullptr || m_indexBuffer == nullptr)
	{
		return;
	}

	if (m_indices.empty())
	{
		return;
	}

	g_theRenderer->SetLightingConstants(m_sunDirection, TERRAIN_SUN_INTENSITY, TERRAIN_AMBIENT_INTENSITY);
	SetFogConstants();
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->BindSampler(SamplerMode::POINT_CLAMP, 0);
	g_theRenderer->BindSampler(SamplerMode::POINT_CLAMP, 1);
	g_theRenderer->BindSampler(SamplerMode::POINT_CLAMP, 2);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(m_shader);
	g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, static_cast<int>(m_indices.size()));
}

float Terrain::GetHeightAtXY(float x, float y) const
{
	float halfSize = static_cast<float>(TERRAIN_SIZE) * TERRAIN_SCALE * 0.5f;

	float terrainX = (x + halfSize) / TERRAIN_SCALE;
	float terrainY = (y + halfSize) / TERRAIN_SCALE;

	int indexX = RoundDownToInt(terrainX);
	int indexY = RoundDownToInt(terrainY);

	if (!IsValidHeightSample(indexX, indexY))
	{
		return 0.f;
	}

	float terrainIndexX = terrainX - indexX;
	float terrainIndexY = terrainY - indexY;

	int indexBottomLeft = indexY * TERRAIN_SIZE + indexX;
	int indexBottomRight = indexY * TERRAIN_SIZE + (indexX + 1);
	int indexTopLeft = (indexY + 1) * TERRAIN_SIZE + indexX;
	int indexTopRight = (indexY + 1) * TERRAIN_SIZE + (indexX + 1);

	float heightBottomLeft = m_terrainHeights[indexBottomLeft];
	float heightBottomRight = m_terrainHeights[indexBottomRight];
	float heightTopLeft = m_terrainHeights[indexTopLeft];
	float heightTopRight = m_terrainHeights[indexTopRight];

	// Bilinear interpolation
	float heightBottom = Interpolate(heightBottomLeft, heightBottomRight, terrainIndexX);
	float heightTop = Interpolate(heightTopLeft, heightTopRight, terrainIndexX);
	float height = Interpolate(heightBottom, heightTop, terrainIndexY);

	return height;
}

bool Terrain::IsInBounds(int x, int y) const
{
	if (x < 0 || y < 0 || x >= m_terrainBounds.m_maxs.x - 1 || y >= m_terrainBounds.m_maxs.y - 1)
	{
		return false;
	}
	return true;
}

bool Terrain::IsValidHeightSample(int x, int y) const
{
	if (x < 0 || y < 0 || x >= TERRAIN_SIZE - 1 || y >= TERRAIN_SIZE - 1)
	{
		return false;
	}
	return true;
}

void Terrain::GenerateHeightMap()
{
	m_terrainHeights.resize(TERRAIN_SIZE * TERRAIN_SIZE);
	m_minTerrainHeight = FLT_MAX;
	m_maxTerrainHeight = FLT_MIN;

	std::vector<Hill> hills;

	for (int hillIndex = 0; hillIndex < TERRAIN_NUM_HILLS; ++hillIndex)
	{
		Hill hill;

		float minX = m_terrainBounds.m_mins.x;
		float maxX = m_terrainBounds.m_maxs.x;
		float minY = m_terrainBounds.m_mins.y;
		float maxY = m_terrainBounds.m_maxs.y;

		hill.m_hillCenter = Vec2(g_rng->RollRandomFloatInRange(minX, maxX), g_rng->RollRandomFloatInRange(minY, maxY));
		hill.m_hillRadius = g_rng->RollRandomFloatInRange(0.2f, 0.5f) * m_terrainBounds.m_maxs.y;
		hill.m_hillStrength = g_rng->RollRandomFloatInRange(0.2f, 0.8f); 

		hills.push_back(hill);
	}

	for (int terrainY = 0; terrainY < TERRAIN_SIZE; ++terrainY)
	{
		for (int terrainX = 0; terrainX < TERRAIN_SIZE; ++terrainX)
		{
			float total = 0.f;

			for (int hillIndex = 0; hillIndex < TERRAIN_NUM_HILLS; ++hillIndex)
			{
				Hill const& hill = hills[hillIndex];
				float halfSize = static_cast<float>(TERRAIN_SIZE) * TERRAIN_SCALE * 0.5f;

				// Converting index to world
				float worldX = static_cast<float>(terrainX) * TERRAIN_SCALE - halfSize;
				float worldY = static_cast<float>(terrainY) * TERRAIN_SCALE - halfSize;

				float dx = (worldX - hill.m_hillCenter.x) / hill.m_hillRadius;
				float dy = (worldY - hill.m_hillCenter.y) / hill.m_hillRadius;
				float distSq = dx * dx + dy * dy;

				if (distSq < 1.f)
				{
					float dist = sqrtf(distSq);
					float t = GetClamped(1.f - dist, 0.f, 1.f);
					SmoothStep5(t);

					// Semi sphere shape
					float height = sqrtf(1.f - distSq);
					total += height * hill.m_hillStrength;
				}
			}

			float terrainHeight = total * 0.2f * HEIGHT_SCALE; 

			m_minTerrainHeight = GetMin(m_minTerrainHeight, terrainHeight);
			m_maxTerrainHeight = GetMax(m_maxTerrainHeight, terrainHeight);

			m_terrainHeights[terrainY * TERRAIN_SIZE + terrainX] = terrainHeight;
		}
	}

	// Smoothing the semi spheres together
	for (int iteration = 0; iteration < 2; ++iteration)
	{
		std::vector<float> temperature = m_terrainHeights;

		for (int terrainY = 1; terrainY < TERRAIN_SIZE - 1; ++terrainY)
		{
			for (int terrainX = 1; terrainX < TERRAIN_SIZE - 1; ++terrainX)
			{
				float sum = 0.f;
				int count = 0;

				for (int smoothY = -1; smoothY <= 1; ++smoothY)
				{
					for (int smoothX = -1; smoothX <= 1; ++smoothX)
					{
						sum += temperature[(terrainY + smoothY) * TERRAIN_SIZE + (terrainX + smoothX)];
						count++;
					}
				}

				m_terrainHeights[terrainY * TERRAIN_SIZE + terrainX] = sum / static_cast<float>(count);
			}
		}
	}
}

void Terrain::GenerateMesh()
{
	m_vertices.clear();
	m_indices.clear();

	float texelsPerWorldUnit = 0.01f;
	float halfSize = static_cast<float>(TERRAIN_SIZE) * TERRAIN_SCALE * 0.5f;

	for (int terrainIndexY = 0; terrainIndexY < TERRAIN_SIZE - 1; ++terrainIndexY)
	{
		for (int terrainIndexX = 0; terrainIndexX < TERRAIN_SIZE - 1; ++terrainIndexX)
		{
			int bL = terrainIndexY * TERRAIN_SIZE + terrainIndexX;
			int bR = terrainIndexY * TERRAIN_SIZE + (terrainIndexX + 1);
			int tR = (terrainIndexY + 1) * TERRAIN_SIZE + (terrainIndexX + 1);
			int tL = (terrainIndexY + 1) * TERRAIN_SIZE + terrainIndexX;

			float worldX0 = (float)terrainIndexX * TERRAIN_SCALE - halfSize;
			float worldX1 = (float)(terrainIndexX + 1) * TERRAIN_SCALE - halfSize;
			float worldY0 = (float)terrainIndexY * TERRAIN_SCALE - halfSize;
			float worldY1 = (float)(terrainIndexY + 1) * TERRAIN_SCALE - halfSize;

			Vec3 point0 = Vec3(worldX0, worldY0, m_terrainHeights[bL]);
			Vec3 point1 = Vec3(worldX1, worldY0, m_terrainHeights[bR]);
			Vec3 point2 = Vec3(worldX1, worldY1, m_terrainHeights[tR]);
			Vec3 point3 = Vec3(worldX0, worldY1, m_terrainHeights[tL]);

			Rgba8 color = Rgba8::WHITE;
			if ((terrainIndexX + terrainIndexY) % 2 == 0)
			{
				color = Rgba8::DIMGRAY;
			}
			else
			{
				color = Rgba8::GRAY;
			}
			
			// UVs based on world position
			float x0 = static_cast<float>(terrainIndexX) * TERRAIN_SCALE;
			float x1 = static_cast<float>(terrainIndexX + 1) * TERRAIN_SCALE;
			float y0 = static_cast<float>(terrainIndexY) * TERRAIN_SCALE;
			float y1 = static_cast<float>(terrainIndexY + 1) * TERRAIN_SCALE;
			Vec2  uvMins = Vec2(x0, y0) * texelsPerWorldUnit;
			Vec2  uvMaxs = Vec2(x1, y1) * texelsPerWorldUnit;
			AABB2 uvs = AABB2(uvMins, uvMaxs);

			AddVertsForQuad3D(m_vertices, m_indices, point0, point1, point2, point3, color, uvs);
		}
	}
}

void Terrain::CreateBuffers()
{
	if (m_vertices.empty())
	{
		return;
	}

	if (m_indices.empty())
	{
		return;
	}

	DeleteBuffers();

	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(static_cast<unsigned int>(m_vertices.size()) * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	m_indexBuffer = g_theRenderer->CreateIndexBuffer(static_cast<unsigned int>(m_indices.size()) * sizeof(unsigned int), sizeof(unsigned int));
	g_theRenderer->CopyCPUToGPU(m_vertices.data(), m_vertexBuffer->GetSize(), m_vertexBuffer);
	g_theRenderer->CopyCPUToGPU(m_indices.data(), m_indexBuffer->GetSize(), m_indexBuffer);
}

void Terrain::DeleteBuffers()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;
}

void Terrain::SetFogConstants() const
{
	if (m_constantBuffer == nullptr)
	{
		return;
	}

	FogConstants fogConstants;
	fogConstants.FogColor = Vec3(0.6f, 0.7f, 0.8f);
	fogConstants.FogStart = 50.f;
	fogConstants.FogEnd = 200.f;

	g_theRenderer->CopyCPUToGPU((void*)&fogConstants, sizeof(fogConstants), m_constantBuffer);
	g_theRenderer->BindConstantBuffer(5, m_constantBuffer);
}
