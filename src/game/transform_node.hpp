#pragma once

#include "utils/transform.hpp"

namespace game
{
	struct TransformNode
	{
		const TransformNode* parent = nullptr;
		Transform local_transform;
		glm::mat4 matrix = glm::mat4(1.0f); // Global

		TransformNode()
		{
			UpdateMatrix();
		}

		void UpdateMatrix()
		{
			matrix = local_transform.ToMatrix();
			
			if (parent)
			{
				matrix = parent->matrix * matrix;
			}
		}
	};

}