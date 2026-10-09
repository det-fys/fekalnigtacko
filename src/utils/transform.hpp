#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <btBulletCollisionCommon.h>

inline glm::mat3 GetBasis(const glm::quat& rotation, float scale)
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

    return glm::mat3(
        (1.0f - (yy + zz)) * scale,    (xy + wz) * scale,             (xz - wy) * scale,
        (xy - wz) * scale,             (1.0f - (xx + zz)) * scale,    (yz + wx) * scale,
        (xz + wy) * scale,             (yz - wx) * scale,             (1.0f - (xx + yy)) * scale
    );
}

struct Transform
{
	glm::vec3 position = glm::vec3(0.0f);
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
	float scale = 1.0f;

	glm::mat4 ToMatrix() const
	{
        auto basis = GetBasis(rotation, scale);
        return glm::mat4(
            glm::vec4(basis[0], 0.0f),
            glm::vec4(basis[1], 0.0f),
            glm::vec4(basis[2], 0.0f),
            glm::vec4(position, 1.0f)
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

struct FarMatrix
{
    glm::dvec3 position;
    glm::mat3 basis;

    FarMatrix() = default;
    FarMatrix(const glm::dvec3& pos, const glm::mat3& basis) : position(pos), basis(basis) {}
    FarMatrix(float v) : position(0.0f), basis(glm::mat3(v)) {}

    static FarMatrix FromNearMatrix(const glm::mat4& near_matrix, const glm::dvec3& eye)
    {
        glm::dvec3 pos = glm::dvec3(near_matrix[3]) + eye;
        glm::mat3 basis = glm::mat3(near_matrix);
        return FarMatrix(pos, basis);
    }

    FarMatrix operator*(const FarMatrix& other) const
    {
        FarMatrix result;
        result.basis = basis * other.basis;
        result.position = position + glm::dmat3(basis) * other.position; // TODO: optimize
        return result;
    }

    FarMatrix TransformNear(const glm::mat4& near_matrix) const
    {
        // TODO: fix
        glm::dvec3 new_position = position + glm::dmat3(basis) * glm::dvec3(near_matrix[3]);
        glm::mat3 new_basis = basis * glm::mat3(near_matrix);
        return FarMatrix(new_position, new_basis);
    }

    glm::mat4 ToNearMatrix(const glm::dvec3& eye) const
    {
        return glm::mat4(
            glm::vec4(basis[0], 0.0f),
            glm::vec4(basis[1], 0.0f),
            glm::vec4(basis[2], 0.0f),
            glm::vec4(position - eye, 1.0f)
        );
    }

    glm::dvec3 TransformPoint(const glm::dvec3& point) const
    {
        return position + glm::dmat3(basis) * point; // TODO: optimize
    }

    glm::dvec3 TransformPoint(const glm::vec3& point) const
    {
        return position + glm::dvec3(basis * point);
    }

    glm::vec3 TransformDir(const glm::vec3& dir) const
    {
        return basis * dir;
    }

    glm::dmat4 ToDMat4() const
    {
        return glm::dmat4(
            glm::dvec4(basis[0], 0.0),
            glm::dvec4(basis[1], 0.0),
            glm::dvec4(basis[2], 0.0),
            glm::dvec4(position, 1.0)
        );
    }
};

struct FarTransform
{
    glm::dvec3 position{0.0};
    glm::quat rotation{1.0, 0.0, 0.0, 0.0};
    float scale = 1.0f;

    static FarTransform FromNearTransform(const Transform& trans)
    {
        FarTransform result;
        result.position = glm::dvec3(trans.position);
        result.rotation = trans.rotation;
        result.scale = trans.scale;
        return result;
    }

    static FarTransform FromNearTransform(const Transform& trans, const glm::dvec3& offset)
    {
        auto result = FromNearTransform(trans);
        result.position += offset;
        return result;
    }

    FarMatrix ToFarMatrix() const
    {
        auto basis = GetBasis(rotation, scale);
        return FarMatrix{position, basis};
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
        position = glm::vec<3, btScalar>(bt_position.x(), bt_position.y(), bt_position.z());
        rotation = glm::qua<btScalar>(bt_rotation.w(), bt_rotation.x(), bt_rotation.y(), bt_rotation.z());
    }

    btTransform ToBtTransform() const 
    {
        btQuaternion bt_rotation(rotation.x, rotation.y, rotation.z, rotation.w);
        btVector3 bt_position(position.x, position.y, position.z);
        return btTransform(bt_rotation, bt_position);
    }

    static FarTransform Lerp(const FarTransform& a, const FarTransform& b, float t)
    {
        FarTransform result;
        result.position = glm::mix(a.position, b.position, t);
        result.rotation = glm::slerp(a.rotation, b.rotation, t);
        result.scale = glm::mix(a.scale, b.scale, t);
        return result;
    }
};

