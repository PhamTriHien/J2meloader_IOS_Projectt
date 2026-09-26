#include "math3d.h"

namespace universal_loader {
namespace graphics3d {

Matrix4x4::Matrix4x4() {
    std::memset(m, 0, sizeof(m));
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

Matrix4x4::Matrix4x4(const float elements[16]) {
    std::memcpy(m, elements, sizeof(m));
}

Matrix4x4 Matrix4x4::identity() {
    return Matrix4x4();
}

Matrix4x4 Matrix4x4::zero() {
    Matrix4x4 result;
    std::memset(result.m, 0, sizeof(result.m));
    return result;
}

Matrix4x4 Matrix4x4::translation(float tx, float ty, float tz) {
    Matrix4x4 result;
    result.m[3] = tx;
    result.m[7] = ty;
    result.m[11] = tz;
    return result;
}

Matrix4x4 Matrix4x4::scaling(float sx, float sy, float sz) {
    Matrix4x4 result;
    result.m[0] = sx;
    result.m[5] = sy;
    result.m[10] = sz;
    return result;
}

Matrix4x4 Matrix4x4::rotationX(float angleRad) {
    Matrix4x4 result;
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    result.m[5] = c;
    result.m[6] = -s;
    result.m[9] = s;
    result.m[10] = c;
    return result;
}

Matrix4x4 Matrix4x4::rotationY(float angleRad) {
    Matrix4x4 result;
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    result.m[0] = c;
    result.m[2] = s;
    result.m[8] = -s;
    result.m[10] = c;
    return result;
}

Matrix4x4 Matrix4x4::rotationZ(float angleRad) {
    Matrix4x4 result;
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    result.m[0] = c;
    result.m[1] = -s;
    result.m[4] = s;
    result.m[5] = c;
    return result;
}

Matrix4x4 Matrix4x4::rotationAxis(const Vector3& axis, float angleRad) {
    Vector3 a = axis.normalized();
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);
    float t = 1.0f - c;

    Matrix4x4 result;
    result.m[0] = t * a.x * a.x + c;
    result.m[1] = t * a.x * a.y - s * a.z;
    result.m[2] = t * a.x * a.z + s * a.y;
    result.m[3] = 0.0f;

    result.m[4] = t * a.x * a.y + s * a.z;
    result.m[5] = t * a.y * a.y + c;
    result.m[6] = t * a.y * a.z - s * a.x;
    result.m[7] = 0.0f;

    result.m[8] = t * a.x * a.z - s * a.y;
    result.m[9] = t * a.y * a.z + s * a.x;
    result.m[10] = t * a.z * a.z + c;
    result.m[11] = 0.0f;

    result.m[12] = 0.0f;
    result.m[13] = 0.0f;
    result.m[14] = 0.0f;
    result.m[15] = 1.0f;

    return result;
}

Matrix4x4 Matrix4x4::fromQuaternion(const Quaternion& q) {
    Quaternion n = q.normalized();
    Matrix4x4 result;

    float xx = n.x * n.x;
    float yy = n.y * n.y;
    float zz = n.z * n.z;
    float xy = n.x * n.y;
    float xz = n.x * n.z;
    float yz = n.y * n.z;
    float wx = n.w * n.x;
    float wy = n.w * n.y;
    float wz = n.w * n.z;

    result.m[0] = 1.0f - 2.0f * (yy + zz);
    result.m[1] = 2.0f * (xy - wz);
    result.m[2] = 2.0f * (xz + wy);
    result.m[3] = 0.0f;

    result.m[4] = 2.0f * (xy + wz);
    result.m[5] = 1.0f - 2.0f * (xx + zz);
    result.m[6] = 2.0f * (yz - wx);
    result.m[7] = 0.0f;

    result.m[8] = 2.0f * (xz - wy);
    result.m[9] = 2.0f * (yz + wx);
    result.m[10] = 1.0f - 2.0f * (xx + yy);
    result.m[11] = 0.0f;

    result.m[12] = 0.0f;
    result.m[13] = 0.0f;
    result.m[14] = 0.0f;
    result.m[15] = 1.0f;

    return result;
}

Matrix4x4 Matrix4x4::perspective(float fovYRad, float aspect, float zNear, float zFar) {
    Matrix4x4 result = Matrix4x4::zero();
    float tanHalfFov = std::tan(fovYRad * 0.5f);
    if (std::abs(tanHalfFov) < 1e-6f || std::abs(aspect) < 1e-6f || std::abs(zNear - zFar) < 1e-6f) {
        return Matrix4x4::identity();
    }

    result.m[0] = 1.0f / (aspect * tanHalfFov);
    result.m[5] = 1.0f / tanHalfFov;
    result.m[10] = -(zFar + zNear) / (zFar - zNear);
    result.m[11] = -(2.0f * zFar * zNear) / (zFar - zNear);
    result.m[14] = -1.0f;
    result.m[15] = 0.0f;

    return result;
}

Matrix4x4 Matrix4x4::ortho(float left, float right, float bottom, float top, float zNear, float zFar) {
    Matrix4x4 result = Matrix4x4::zero();
    float rl = right - left;
    float tb = top - bottom;
    float fn = zFar - zNear;

    if (std::abs(rl) < 1e-6f || std::abs(tb) < 1e-6f || std::abs(fn) < 1e-6f) {
        return Matrix4x4::identity();
    }

    result.m[0] = 2.0f / rl;
    result.m[3] = -(right + left) / rl;
    result.m[5] = 2.0f / tb;
    result.m[7] = -(top + bottom) / tb;
    result.m[10] = -2.0f / fn;
    result.m[11] = -(zFar + zNear) / fn;
    result.m[15] = 1.0f;

    return result;
}

Matrix4x4 Matrix4x4::lookAt(const Vector3& eye, const Vector3& target, const Vector3& up) {
    Vector3 forward = (eye - target).normalized();
    Vector3 right = up.cross(forward).normalized();
    Vector3 realUp = forward.cross(right);

    Matrix4x4 result;
    result.m[0] = right.x;
    result.m[1] = right.y;
    result.m[2] = right.z;
    result.m[3] = -right.dot(eye);

    result.m[4] = realUp.x;
    result.m[5] = realUp.y;
    result.m[6] = realUp.z;
    result.m[7] = -realUp.dot(eye);

    result.m[8] = forward.x;
    result.m[9] = forward.y;
    result.m[10] = forward.z;
    result.m[11] = -forward.dot(eye);

    result.m[12] = 0.0f;
    result.m[13] = 0.0f;
    result.m[14] = 0.0f;
    result.m[15] = 1.0f;

    return result;
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& rhs) const {
    Matrix4x4 result = Matrix4x4::zero();
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += m[row * 4 + k] * rhs.m[k * 4 + col];
            }
            result.m[row * 4 + col] = sum;
        }
    }
    return result;
}

Vector4 Matrix4x4::operator*(const Vector4& v) const {
    return Vector4(
        m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3] * v.w,
        m[4] * v.x + m[5] * v.y + m[6] * v.z + m[7] * v.w,
        m[8] * v.x + m[9] * v.y + m[10] * v.z + m[11] * v.w,
        m[12] * v.x + m[13] * v.y + m[14] * v.z + m[15] * v.w
    );
}

Vector3 Matrix4x4::transformPoint(const Vector3& p) const {
    Vector4 v(p, 1.0f);
    Vector4 res = (*this) * v;
    return res.toVector3();
}

Vector3 Matrix4x4::transformVector(const Vector3& v) const {
    return Vector3(
        m[0] * v.x + m[1] * v.y + m[2] * v.z,
        m[4] * v.x + m[5] * v.y + m[6] * v.z,
        m[8] * v.x + m[9] * v.y + m[10] * v.z
    );
}

Matrix4x4 Matrix4x4::transpose() const {
    Matrix4x4 result;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result.m[row * 4 + col] = m[col * 4 + row];
        }
    }
    return result;
}

bool Matrix4x4::invert(Matrix4x4& outInverse) const {
    float inv[16];

    inv[0] = m[5]  * m[10] * m[15] - 
             m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + 
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - 
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] + 
              m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - 
              m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + 
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] - 
             m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + 
             m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - 
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] + 
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - 
               m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + 
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] + 
              m[1]  * m[11] * m[14] + 
              m[9]  * m[2] * m[15] - 
              m[9]  * m[3] * m[14] - 
              m[13] * m[2] * m[11] + 
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] - 
             m[0]  * m[11] * m[14] - 
             m[8]  * m[2] * m[15] + 
             m[8]  * m[3] * m[14] + 
             m[12] * m[2] * m[11] - 
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] + 
              m[0]  * m[11] * m[13] + 
              m[8]  * m[1] * m[15] - 
              m[8]  * m[3] * m[13] - 
              m[12] * m[1] * m[11] + 
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] - 
              m[0]  * m[10] * m[13] - 
              m[8]  * m[1] * m[14] + 
              m[8]  * m[2] * m[13] + 
              m[12] * m[1] * m[10] - 
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] - 
             m[1]  * m[7] * m[14] - 
             m[5]  * m[2] * m[15] + 
             m[5]  * m[3] * m[14] + 
             m[13] * m[2] * m[7] - 
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] + 
              m[0]  * m[7] * m[14] + 
              m[4]  * m[2] * m[15] - 
              m[4]  * m[3] * m[14] - 
              m[12] * m[2] * m[7] + 
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] - 
              m[0]  * m[7] * m[13] - 
              m[4]  * m[1] * m[15] + 
              m[4]  * m[3] * m[13] + 
              m[12] * m[1] * m[7] - 
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] + 
               m[0]  * m[6] * m[13] + 
               m[4]  * m[1] * m[14] - 
               m[4]  * m[2] * m[13] - 
               m[12] * m[1] * m[6] + 
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + 
              m[1] * m[7] * m[10] + 
              m[5] * m[2] * m[11] - 
              m[5] * m[3] * m[10] - 
              m[9] * m[2] * m[7] + 
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - 
             m[0] * m[7] * m[10] - 
             m[4] * m[2] * m[11] + 
             m[4] * m[3] * m[10] + 
             m[8] * m[2] * m[7] - 
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + 
               m[0] * m[7] * m[9] + 
               m[4] * m[1] * m[11] - 
               m[4] * m[3] * m[9] - 
               m[8] * m[1] * m[7] + 
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - 
              m[0] * m[6] * m[9] - 
              m[4] * m[1] * m[10] + 
              m[4] * m[2] * m[9] + 
              m[8] * m[1] * m[6] - 
              m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (std::abs(det) < 1e-8f) {
        return false;
    }

    float invDet = 1.0f / det;
    for (int i = 0; i < 16; ++i) {
        outInverse.m[i] = inv[i] * invDet;
    }

    return true;
}

} // namespace graphics3d
} // namespace universal_loader
