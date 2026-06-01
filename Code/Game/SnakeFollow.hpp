#pragma once
#include "Game/Entity.hpp"
#include "Engine/Skeleton/Skeleton.hpp"
// -----------------------------------------------------------------------------
class AnimalMode;
class VertexBuffer;
class ConstantBuffer;
class Shader;
struct Vertex_PCUTBN;
// -----------------------------------------------------------------------------
constexpr float SNAKE_SEGMENT_LENGTH = 1.f;
constexpr int   SNAKE_SEGMENT_COUNT = 75;
constexpr int   MAX_SNAKE_VERTS = 2000;
constexpr float SNAKE_SUN_INTENSITY = 0.65f;
constexpr float SNAKE_AMBIENT_INTENSITY = 0.45f;
// -----------------------------------------------------------------------------
class SnakeFollow : public Entity
{
public:
	SnakeFollow(AnimalMode* animalMode, Vec3 position, Rgba8 color);
	~SnakeFollow();

	virtual void Initialize() override;
	virtual void Update(float deltaSeconds) override;
	virtual void Render() const override;

private:
	Skeleton CreateSkeleton();
	void DrawSnake() const;
	void UpdateSnakeFollowPose(float deltaSeconds);
	void SolveTerrainAlignment();
	void UpdateVerts();

	void UpdateGPUBuffer();
	void DeleteBuffers();

private:
	Skeleton m_snakeFollowSkeleton;
	float m_timeUntilTurn = 0.f;
	float m_targetTurnAngle = 0.f;
	Rgba8 m_snakeColor = Rgba8::WHITE;

	VertexBuffer* m_vertexBuffer = nullptr;
	ConstantBuffer* m_skinningBuffer = nullptr;
	std::vector<Vertex_PCUTBN> m_snakeTBNVerts;
	Shader* m_snakeLit = nullptr;
	Vec3 m_sunDirection = Vec3(3.f, 1.f, -2.f);

	Texture* m_snakeTexture = nullptr;
	std::vector<Vertex_PCU> m_snakeVertsFollowUntextured;
};