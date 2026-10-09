#pragma once

#include "utils/transform.hpp"

namespace game
{

struct TransformNode
{
    const TransformNode* parent = nullptr;
    Transform local;
    glm::mat4 matrix = glm::mat4(1.0f); // Global

    TransformNode() { UpdateMatrix(); }

    void UpdateMatrix()
    {
        matrix = local.ToMatrix();

        if (parent)
        {
            matrix = parent->matrix * matrix;
        }
    }

    glm::vec3 GetGlobalPosition() const { return matrix[3]; }
};

struct FarTransformNode
{
    const FarTransformNode* parent = nullptr;
	const TransformNode* near_parent = nullptr;
    FarTransform local;
    FarMatrix matrix{1.0f}; // Global

    FarTransformNode() { UpdateMatrix(); }

    void UpdateMatrix()
    {
        matrix = local.ToFarMatrix();

		if (near_parent)
		{
			matrix = matrix.TransformNear(near_parent->matrix);
		}

        if (parent)
        {
            matrix = parent->matrix * matrix;
        }
    }

    glm::dvec3 GetGlobalPosition() const { return matrix.position; }
};

} // namespace game