#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <btBulletCollisionCommon.h>

struct Transform
{
	glm::vec3 position = glm::vec3(0.0f);
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
	float scale = 1.0f;

	glm::mat4 ToMatrix() const
	{
        float x = rotation.x;
        float y = rotation.y;
        float z = rotation.z;
        float w = rotation.w;

        float x2 = x + x;
        float y2 = y + y;
        float z2 = z + z;
        float xx = x * x2;
        float xy = x * y2;
        float xz = x * z2;
        float yy = y * y2;
        float yz = y * z2;
        float zz = z * z2;
        float wx = w * x2;
        float wy = w * y2;
        float wz = w * z2;

        float sx = scale;
        float sy = scale;
        float sz = scale;

        return glm::mat4(
            (1.0f - (yy + zz)) * sx,    (xy + wz) * sx,             (xz - wy) * sx,             0.0f,
            (xy - wz) * sy,             (1.0f - (xx + zz)) * sy,    (yz + wx) * sy,             0.0f,
            (xz + wy) * sz,             (yz - wx) * sz,             (1.0f - (xx + yy)) * sz,    0.0f,
            position.x,                 position.y,                 position.z,                 1.0f
		);
	}

    btTransform ToBtTransform() const 
    {
        btQuaternion bt_rotation(rotation.x, rotation.y, rotation.z, rotation.w);
		btVector3 bt_position(position.x, position.y, position.z);
		return btTransform(bt_rotation, bt_position);    
    }

	void SetAngles(const glm::vec3& angles_deg)
	{
		glm::vec3 angles_rad = glm::radians(angles_deg);
		rotation = glm::quat(angles_rad);
	}

    void SetBtTransform(const btTransform& bt_trans)
    {
        btVector3 bt_position = bt_trans.getOrigin();
        btQuaternion bt_rotation = bt_trans.getRotation();
        position = glm::vec3(bt_position.x(), bt_position.y(), bt_position.z());
        rotation = glm::quat(bt_rotation.w(), bt_rotation.x(), bt_rotation.y(), bt_rotation.z());
    }

	static Transform Lerp(const Transform& a, const Transform& b, float t)
	{
		Transform result;
		result.position = glm::mix(a.position, b.position, t);
		result.rotation = glm::slerp(a.rotation, b.rotation, t);
		result.scale = glm::mix(a.scale, b.scale, t);
		return result;
	}
};