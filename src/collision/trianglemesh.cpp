#include "trianglemesh.hpp"

collision::TriangleMesh::TriangleMesh()
{

}

void collision::TriangleMesh::AddTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
{
	btVector3 bt_v0(v0.x, v0.y, v0.z);
	btVector3 bt_v1(v1.x, v1.y, v1.z);
	btVector3 bt_v2(v2.x, v2.y, v2.z);
	bt_mesh_.addTriangle(bt_v0, bt_v1, bt_v2, true);
}

void collision::TriangleMesh::Build()
{
	bt_shape_ = std::make_unique<btBvhTriangleMeshShape>(&bt_mesh_, true, true);

}
