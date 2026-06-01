#pragma once
#include "Engine/Math/Vec3.h"
// -----------------------------------------------------------------------------
class AnimalMode;
// -----------------------------------------------------------------------------
enum class AnimalType
{
	OCTOPUS,
	SPIDER,
	SNAKE
};
// -----------------------------------------------------------------------------
class Entity
{
public:
	Entity(AnimalMode* mode, Vec3 position);
	virtual ~Entity() {}

	virtual void Initialize() = 0;
	virtual void Update(float deltaSeconds) = 0;
	virtual void Render() const = 0;

	Vec3 GetWorldPosition() const;
	void SetWorldPosition(Vec3 const& worldPosition);
	void SetSpeed(float speed);
	void SetIsStationary(bool isStationary);

public:
	AnimalMode* m_animalMode = nullptr;
	Vec3        m_worldPosition = Vec3::ZERO;
	bool        m_isMoving = true;
	bool        m_isStationary = false;
	Vec3        m_moveDirection = Vec3::XAXE;
	float       m_speed = 1.5f;
	AnimalType  m_type;
};
// -----------------------------------------------------------------------------