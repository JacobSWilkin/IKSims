#include "Game/Entity.hpp"

Entity::Entity(AnimalMode* mode, Vec3 position)
	:m_animalMode(mode),
	 m_worldPosition(position)
{
}

Vec3 Entity::GetWorldPosition() const
{
	return m_worldPosition;
}

void Entity::SetWorldPosition(Vec3 const& worldPosition)
{
	m_worldPosition = worldPosition;
}

void Entity::SetSpeed(float speed)
{
	m_speed = speed;
}

void Entity::SetIsStationary(bool isStationary)
{
	m_isStationary = isStationary;
}