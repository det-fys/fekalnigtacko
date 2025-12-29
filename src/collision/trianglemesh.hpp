#pragma once

#include <vector>
#include <span>
#include <glm/glm.hpp>
#include "aabb.hpp"

#include <memory>

#include <btBulletCollisionCommon.h>

namespace collision
{
	class TriangleMesh
	{
		btTriangleMesh bt_mesh_;
		std::unique_ptr<btBvhTriangleMeshShape> bt_shape_;

	public:
		TriangleMesh();

		void AddTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
		void Build();

		btBvhTriangleMeshShape* GetShape() { return bt_shape_.get(); }
	};
}