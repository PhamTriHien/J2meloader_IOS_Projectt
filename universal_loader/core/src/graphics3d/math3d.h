#ifndef UNIVERSAL_LOADER_MATH3D_H
#define UNIVERSAL_LOADER_MATH3D_H

#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "../../include/j2me_core.h"

namespace universal_loader {
namespace graphics3d {

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

struct Vector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vector3() = default;
    constexpr Vector3(float inX, float inY, float inZ) : x(inX), y(inY), z(inZ) {}

    Vector3 operator+(const Vector3& rhs) const { return Vector3(x + rhs.x, y + rhs.y, z + rhs.z); }
    Vector3 operator-(const Vector3& rhs) const { return Vector3(x - rhs.x, y - rhs.y, z - rhs.z); }
    Vector3 operator*(float scalar) const { return Vector3(x * scalar, y * scalar, z * scalar); }
    Vector3 operator/(float scalar) const {
        float inv = (scalar != 0.0f) ? (1.0f / scalar) : 0.0f;
        return Vector3(x * inv, y * inv, z * inv);
    }

    Vector3& operator+=(const Vector3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    Vector3& operator-=(const Vector3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    Vector3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }

    float dot(const Vector3& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z; }

    Vector3 cross(const Vector3& rhs) const {
        return Vector3(
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        );
    }

    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }

    Vector3 normalized() const {
        float len = length();
        if (len > 1e-6f) {
            float inv = 1.0f / len;
            return Vector3(x * inv, y * inv, z * inv);
        }
        return Vector3(0.0f, 0.0f, 0.0f);
    }
};

struct Vector4 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{1.0f};

    constexpr Vector4() = default;
    constexpr Vector4(float inX, float inY, float inZ, float inW) : x(inX), y(inY), z(inZ), w(inW) {}
    constexpr Vector4(const Vector3& v, float inW) : x(v.x), y(v.y), z(v.z), w(inW) {}

    Vector3 toVector3() const {
        if (std::abs(w) > 1e-6f) {
            float invW = 1.0f / w;
            return Vector3(x * invW, y * invW, z * invW);
        }
        return Vector3(x, y, z);
    }
};

struct Quaternion {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{1.0f};

    constexpr Quaternion() = default;
    constexpr Quaternion(float inX, float inY, float inZ, float inW) : x(inX), y(inY), z(inZ), w(inW) {}

    static Quaternion identity() { return Quaternion(0.0f, 0.0f, 0.0f, 1.0f); }

    static Quaternion fromAxisAngle(const Vector3& axis, float angleRad) {
        Vector3 normAxis = axis.normalized();
        float halfAngle = angleRad * 0.5f;
        float s = std::sin(halfAngle);
        return Quaternion(normAxis.x * s, normAxis.y * s, normAxis.z * s, std::cos(halfAngle));
    }

    Quaternion operator*(const Quaternion& rhs) const {
        return Quaternion(
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z
        );
    }

    Quaternion normalized() const {
        float len = std::sqrt(x * x + y * y + z * z + w * w);
        if (len > 1e-6f) {
            float inv = 1.0f / len;
            return Quaternion(x * inv, y * inv, z * inv, w * inv);
        }
        return identity();
    }

    static Quaternion slerp(const Quaternion& q0, const Quaternion& q1, float t) {
        float cosHalfTheta = q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w;
        Quaternion target = q1;
        if (cosHalfTheta < 0.0f) {
            target = Quaternion(-q1.x, -q1.y, -q1.z, -q1.w);
            cosHalfTheta = -cosHalfTheta;
        }

        if (std::abs(cosHalfTheta) >= 1.0f - 1e-5f) {
            return Quaternion(
                q0.x + t * (target.x - q0.x),
                q0.y + t * (target.y - q0.y),
                q0.z + t * (target.z - q0.z),
                q0.w + t * (target.w - q0.w)
            ).normalized();
        }

        float halfTheta = std::acos(cosHalfTheta);
        float sinHalfTheta = std::sqrt(1.0f - cosHalfTheta * cosHalfTheta);
        if (std::abs(sinHalfTheta) < 1e-5f) {
            return q0;
        }

        float ratioA = std::sin((1.0f - t) * halfTheta) / sinHalfTheta;
        float ratioB = std::sin(t * halfTheta) / sinHalfTheta;

        return Quaternion(
            q0.x * ratioA + target.x * ratioB,
            q0.y * ratioA + target.y * ratioB,
            q0.z * ratioA + target.z * ratioB,
            q0.w * ratioA + target.w * ratioB
        ).normalized();
    }
};

class J2ME_API Matrix4x4 {
public:
    // Row-major indexing: m[row][col] or m[row * 4 + col]
    float m[16]{};

    Matrix4x4();
    explicit Matrix4x4(const float elements[16]);

    static Matrix4x4 identity();
    static Matrix4x4 zero();
    static Matrix4x4 translation(float tx, float ty, float tz);
    static Matrix4x4 scaling(float sx, float sy, float sz);
    static Matrix4x4 rotationX(float angleRad);
    static Matrix4x4 rotationY(float angleRad);
    static Matrix4x4 rotationZ(float angleRad);
    static Matrix4x4 rotationAxis(const Vector3& axis, float angleRad);
    static Matrix4x4 fromQuaternion(const Quaternion& q);

    static Matrix4x4 perspective(float fovYRad, float aspect, float zNear, float zFar);
    static Matrix4x4 ortho(float left, float right, float bottom, float top, float zNear, float zFar);
    static Matrix4x4 lookAt(const Vector3& eye, const Vector3& target, const Vector3& up);

    Matrix4x4 operator*(const Matrix4x4& rhs) const;
    Vector4 operator*(const Vector4& v) const;

    Vector3 transformPoint(const Vector3& p) const;
    Vector3 transformVector(const Vector3& v) const;

    Matrix4x4 transpose() const;
    bool invert(Matrix4x4& outInverse) const;
};

} // namespace graphics3d
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_MATH3D_H
