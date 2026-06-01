#pragma once
#include "Engine/Core/Vertex_PCUTBN.hpp"
#include "Engine/Math/AABB3.hpp"
#include <vector>
// -----------------------------------------------------------------------------
class VertexBuffer;
class IndexBuffer;
class Shader;
class ConstantBuffer;
class Texture;
// -----------------------------------------------------------------------------
struct FogConstants
{
	Vec3 FogColor = Vec3::ZERO;
	float FogStart = 0.f;

	float FogEnd = 0.f;
	Vec3  FogPad = Vec3::ZERO;
};
// -----------------------------------------------------------------------------
struct Hill
{
	Vec2  m_hillCenter;
	float m_hillRadius = 0.f;
	float m_hillStrength = 0.f;
};
// -----------------------------------------------------------------------------
constexpr int   TERRAIN_SIZE = 1000;
constexpr int   TERRAIN_NUM_HILLS = 10;
constexpr float TERRAIN_SCALE = 1.f;
constexpr float TERRAIN_SUN_INTENSITY = 0.85f;
constexpr float TERRAIN_AMBIENT_INTENSITY = 0.55f;
constexpr float HEIGHT_SCALE = 27.5f;
// -----------------------------------------------------------------------------
class Terrain
{
public:
	Terrain() = default;
	Terrain(Vec3 const& position);
	~Terrain();

	void InitializeTerrainHills();
	void Render() const;

	float GetHeightAtXY(float x, float y) const;
	bool  IsInBounds(int x, int y) const;
	bool  IsValidHeightSample(int x, int y) const;
	AABB3 m_terrainBounds;

private:
	void GenerateHeightMap();
	void GenerateMesh();
	void CreateBuffers();
	void DeleteBuffers();

	void SetFogConstants() const;

private:
	// Terrain
	std::vector<float> m_terrainHeights;
	float m_minTerrainHeight = 0.f;
	float m_maxTerrainHeight = 0.f;
	Vec3  m_terrainWorldPosition = Vec3::ZERO;

	// Lighting
	Shader* m_shader = nullptr;
	Vec3 m_sunDirection = Vec3(3.f, 1.f, -2.f);
	ConstantBuffer* m_constantBuffer = nullptr;

	// Buffers
	VertexBuffer* m_vertexBuffer = nullptr;
	IndexBuffer* m_indexBuffer = nullptr;
	std::vector<Vertex_PCUTBN> m_vertices;
	std::vector<unsigned int> m_indices;
};