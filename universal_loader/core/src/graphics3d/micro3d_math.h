#ifndef UNIVERSAL_LOADER_MICRO3D_MATH_H
#define UNIVERSAL_LOADER_MICRO3D_MATH_H

#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "math3d.h"

namespace universal_loader {
namespace micro3d {

// Fixed-point scaling constants matching Mascot Capsule Micro3D v3
constexpr int32_t ONE = 4096;               // 1.0 in 12-bit fixed point (1 << 12)
constexpr int32_t HALF = 2048;              // 0.5 in 12-bit fixed point (used for rounding)
constexpr float TO_FLOAT = 1.0f / 4096.0f;  // 2.44140625e-4f
constexpr double PI_VAL = 3.14159265358979323846;

class Util3D {
public:
    static int32_t sin(int32_t p) {
        double radian = static_cast<double>(p) * PI_VAL / 2048.0;
        return static_cast<int32_t>(std::round(std::sin(radian) * 4096.0));
    }

    static int32_t cos(int32_t p) {
        return sin(p + 1024);
    }

    static int32_t sqrt(int32_t p) {
        if (p == 0) return 0;
        double a;
        if (p < 0) {
            uint32_t u = static_cast<uint32_t>(p);
            if (u > 0xfffd0002u) return 0xffff;
            a = static_cast<double>(u);
        } else {
            a = static_cast<double>(p);
        }
        return static_cast<int32_t>(std::round(std::sqrt(a)));
    }
};

struct Vector3D {
    int32_t x{0};
    int32_t y{0};
    int32_t z{0};

    constexpr Vector3D() = default;
    constexpr Vector3D(int32_t inX, int32_t inY, int32_t inZ) : x(inX), y(inY), z(inZ) {}

    Vector3D operator+(const Vector3D& rhs) const { return Vector3D(x + rhs.x, y + rhs.y, z + rhs.z); }
    Vector3D operator-(const Vector3D& rhs) const { return Vector3D(x - rhs.x, y - rhs.y, z - rhs.z); }

    int32_t dot(const Vector3D& rhs) const {
        return static_cast<int32_t>((static_cast<int64_t>(x) * rhs.x +
                                     static_cast<int64_t>(y) * rhs.y +
                                     static_cast<int64_t>(z) * rhs.z + HALF) >> 12);
    }

    Vector3D cross(const Vector3D& rhs) const {
        int64_t cx = static_cast<int64_t>(y) * rhs.z - static_cast<int64_t>(z) * rhs.y;
        int64_t cy = static_cast<int64_t>(z) * rhs.x - static_cast<int64_t>(x) * rhs.z;
        int64_t cz = static_cast<int64_t>(x) * rhs.y - static_cast<int64_t>(y) * rhs.x;
        return Vector3D(
            static_cast<int32_t>((cx + HALF) >> 12),
            static_cast<int32_t>((cy + HALF) >> 12),
            static_cast<int32_t>((cz + HALF) >> 12)
        );
    }

    graphics3d::Vector3 toFloatVector() const {
        return graphics3d::Vector3(x * TO_FLOAT, y * TO_FLOAT, z * TO_FLOAT);
    }
};

class AffineTrans {
public:
    int32_t m00{ONE}, m01{0}, m02{0}, m03{0};
    int32_t m10{0},   m11{ONE}, m12{0}, m13{0};
    int32_t m20{0},   m21{0},   m22{ONE}, m23{0};

    AffineTrans() = default;

    AffineTrans(int32_t in00, int32_t in01, int32_t in02, int32_t in03,
                int32_t in10, int32_t in11, int32_t in12, int32_t in13,
                int32_t in20, int32_t in21, int32_t in22, int32_t in23)
        : m00(in00), m01(in01), m02(in02), m03(in03),
          m10(in10), m11(in11), m12(in12), m13(in13),
          m20(in20), m21(in21), m22(in22), m23(in23) {}

    void setIdentity() {
        m00 = ONE; m01 = 0;   m02 = 0;   m03 = 0;
        m10 = 0;   m11 = ONE; m12 = 0;   m13 = 0;
        m20 = 0;   m21 = 0;   m22 = ONE; m23 = 0;
    }

    void rotationX(int32_t r) {
        int32_t c = Util3D::cos(r);
        int32_t s = Util3D::sin(r);
        m00 = ONE; m01 = 0;  m02 = 0;  m03 = 0;
        m10 = 0;   m11 = c;  m12 = -s; m13 = 0;
        m20 = 0;   m21 = s;  m22 = c;  m23 = 0;
    }

    void rotationY(int32_t r) {
        int32_t c = Util3D::cos(r);
        int32_t s = Util3D::sin(r);
        m00 = c;   m01 = 0;   m02 = s;  m03 = 0;
        m10 = 0;   m11 = ONE; m12 = 0;  m13 = 0;
        m20 = -s;  m21 = 0;   m22 = c;  m23 = 0;
    }

    void rotationZ(int32_t r) {
        int32_t c = Util3D::cos(r);
        int32_t s = Util3D::sin(r);
        m00 = c;   m01 = -s;  m02 = 0;   m03 = 0;
        m10 = s;   m11 = c;   m12 = 0;   m13 = 0;
        m20 = 0;   m21 = 0;   m22 = ONE; m23 = 0;
    }

    void rotationV(const Vector3D& v, int32_t r) {
        int64_t lenSq = static_cast<int64_t>(v.x) * v.x +
                        static_cast<int64_t>(v.y) * v.y +
                        static_cast<int64_t>(v.z) * v.z;
        int32_t len = Util3D::sqrt(static_cast<int32_t>(lenSq));
        if (len == 0) {
            setIdentity();
            return;
        }

        int32_t nx = static_cast<int32_t>((static_cast<int64_t>(v.x) * ONE) / len);
        int32_t ny = static_cast<int32_t>((static_cast<int64_t>(v.y) * ONE) / len);
        int32_t nz = static_cast<int32_t>((static_cast<int64_t>(v.z) * ONE) / len);

        int32_t c = Util3D::cos(r);
        int32_t s = Util3D::sin(r);
        int32_t t = ONE - c;

        m00 = static_cast<int32_t>((static_cast<int64_t>(t) * nx * nx + HALF) >> 24) + c;
        m01 = static_cast<int32_t>((static_cast<int64_t>(t) * nx * ny - static_cast<int64_t>(s) * nz * ONE + HALF) >> 24);
        m02 = static_cast<int32_t>((static_cast<int64_t>(t) * nx * nz + static_cast<int64_t>(s) * ny * ONE + HALF) >> 24);
        m03 = 0;

        m10 = static_cast<int32_t>((static_cast<int64_t>(t) * nx * ny + static_cast<int64_t>(s) * nz * ONE + HALF) >> 24);
        m11 = static_cast<int32_t>((static_cast<int64_t>(t) * ny * ny + HALF) >> 24) + c;
        m12 = static_cast<int32_t>((static_cast<int64_t>(t) * ny * nz - static_cast<int64_t>(s) * nx * ONE + HALF) >> 24);
        m13 = 0;

        m20 = static_cast<int32_t>((static_cast<int64_t>(t) * nx * nz - static_cast<int64_t>(s) * ny * ONE + HALF) >> 24);
        m21 = static_cast<int32_t>((static_cast<int64_t>(t) * ny * nz + static_cast<int64_t>(s) * nx * ONE + HALF) >> 24);
        m22 = static_cast<int32_t>((static_cast<int64_t>(t) * nz * nz + HALF) >> 24) + c;
        m23 = 0;
    }

    Vector3D transPoint(const Vector3D& v) const {
        int64_t rx = static_cast<int64_t>(v.x) * m00 + static_cast<int64_t>(v.y) * m01 + static_cast<int64_t>(v.z) * m02;
        int64_t ry = static_cast<int64_t>(v.x) * m10 + static_cast<int64_t>(v.y) * m11 + static_cast<int64_t>(v.z) * m12;
        int64_t rz = static_cast<int64_t>(v.x) * m20 + static_cast<int64_t>(v.y) * m21 + static_cast<int64_t>(v.z) * m22;

        return Vector3D(
            static_cast<int32_t>(((rx + HALF) >> 12) + m03),
            static_cast<int32_t>(((ry + HALF) >> 12) + m13),
            static_cast<int32_t>(((rz + HALF) >> 12) + m23)
        );
    }

    static void multiplyMM(AffineTrans& out, const AffineTrans& lm, const AffineTrans& rm) {
        int64_t l00 = lm.m00, l01 = lm.m01, l02 = lm.m02;
        int64_t l10 = lm.m10, l11 = lm.m11, l12 = lm.m12;
        int64_t l20 = lm.m20, l21 = lm.m21, l22 = lm.m22;

        int64_t r00 = rm.m00, r01 = rm.m01, r02 = rm.m02, r03 = rm.m03;
        int64_t r10 = rm.m10, r11 = rm.m11, r12 = rm.m12, r13 = rm.m13;
        int64_t r20 = rm.m20, r21 = rm.m21, r22 = rm.m22, r23 = rm.m23;

        out.m00 = static_cast<int32_t>((l00 * r00 + l01 * r10 + l02 * r20 + HALF) >> 12);
        out.m01 = static_cast<int32_t>((l00 * r01 + l01 * r11 + l02 * r21 + HALF) >> 12);
        out.m02 = static_cast<int32_t>((l00 * r02 + l01 * r12 + l02 * r22 + HALF) >> 12);
        out.m03 = static_cast<int32_t>(((l00 * r03 + l01 * r13 + l02 * r23 + HALF) >> 12) + lm.m03);

        out.m10 = static_cast<int32_t>((l10 * r00 + l11 * r10 + l12 * r20 + HALF) >> 12);
        out.m11 = static_cast<int32_t>((l10 * r01 + l11 * r11 + l12 * r21 + HALF) >> 12);
        out.m12 = static_cast<int32_t>((l10 * r02 + l11 * r12 + l12 * r22 + HALF) >> 12);
        out.m13 = static_cast<int32_t>(((l10 * r03 + l11 * r13 + l12 * r23 + HALF) >> 12) + lm.m13);

        out.m20 = static_cast<int32_t>((l20 * r00 + l21 * r10 + l22 * r20 + HALF) >> 12);
        out.m21 = static_cast<int32_t>((l20 * r01 + l21 * r11 + l22 * r21 + HALF) >> 12);
        out.m22 = static_cast<int32_t>((l20 * r02 + l21 * r12 + l22 * r22 + HALF) >> 12);
        out.m23 = static_cast<int32_t>(((l20 * r03 + l21 * r13 + l22 * r23 + HALF) >> 12) + lm.m23);
    }

    graphics3d::Matrix4x4 toMatrix4x4() const {
        graphics3d::Matrix4x4 res = graphics3d::Matrix4x4::identity();
        res.m[0]  = m00 * TO_FLOAT; res.m[1]  = m01 * TO_FLOAT; res.m[2]  = m02 * TO_FLOAT; res.m[3]  = m03 * TO_FLOAT;
        res.m[4]  = m10 * TO_FLOAT; res.m[5]  = m11 * TO_FLOAT; res.m[6]  = m12 * TO_FLOAT; res.m[7]  = m13 * TO_FLOAT;
        res.m[8]  = m20 * TO_FLOAT; res.m[9]  = m21 * TO_FLOAT; res.m[10] = m22 * TO_FLOAT; res.m[11] = m23 * TO_FLOAT;
        res.m[12] = 0.0f;           res.m[13] = 0.0f;           res.m[14] = 0.0f;           res.m[15] = 1.0f;
        return res;
    }
};

enum ProjectionMode {
    COMMAND_PARALLEL_SCALE = 0,
    COMMAND_PARALLEL_SIZE = 1,
    COMMAND_PERSPECTIVE_FOV = 2,
    COMMAND_PERSPECTIVE_WH = 3
};

struct FigureLayout {
    AffineTrans affine;
    int32_t scaleX{512};
    int32_t scaleY{512};
    int32_t centerX{0};
    int32_t centerY{0};
    int32_t parallelWidth{0};
    int32_t parallelHeight{0};
    int32_t nearZ{1};
    int32_t farZ{4096};
    int32_t angle{1024}; // FOV angle in 4096 units (1024 = 90 deg)
    int32_t perspectiveWidth{0};
    int32_t perspectiveHeight{0};
    ProjectionMode projectionMode{COMMAND_PARALLEL_SCALE};

    void computeProjectionMatrix(float pm[16], int32_t posX, int32_t posY, float vw, float vh) const {
        switch (projectionMode) {
            case COMMAND_PARALLEL_SCALE: {
                float w = vw * (4096.0f / static_cast<float>(scaleX > 0 ? scaleX : 512));
                float h = vh * (4096.0f / static_cast<float>(scaleY > 0 ? scaleY : 512));
                float sx = 2.0f / w;
                float sy = 2.0f / h;
                float sz = 1.0f / 65536.0f;
                float tx = 2.0f * (centerX + posX) / vw - 1.0f;
                float ty = 2.0f * (centerY + posY) / vh - 1.0f;

                std::memset(pm, 0, 16 * sizeof(float));
                pm[0] = sx;  pm[3] = tx;
                pm[5] = sy;  pm[7] = ty;
                pm[10] = sz; pm[15] = 1.0f;
                break;
            }
            case COMMAND_PARALLEL_SIZE: {
                float w = (parallelWidth == 0) ? (400.0f * 4.0f) : static_cast<float>(parallelWidth);
                float h = (parallelHeight == 0) ? (w * (vh / vw)) : static_cast<float>(parallelHeight);
                float sx = 2.0f / w;
                float sy = 2.0f / h;
                float sz = 1.0f / 65536.0f;
                float tx = 2.0f * (centerX + posX) / vw - 1.0f;
                float ty = 2.0f * (centerY + posY) / vh - 1.0f;

                std::memset(pm, 0, 16 * sizeof(float));
                pm[0] = sx;  pm[3] = tx;
                pm[5] = sy;  pm[7] = ty;
                pm[10] = sz; pm[15] = 1.0f;
                break;
            }
            case COMMAND_PERSPECTIVE_FOV: {
                float nearVal = static_cast<float>(nearZ);
                float farVal = static_cast<float>(farZ);
                float rd = 1.0f / (nearVal - farVal);
                float angleRad = angle * TO_FLOAT * static_cast<float>(PI_VAL);
                float sx = 1.0f / std::tan(angleRad);
                float sy = sx * (vw / vh);
                float sz = -(farVal + nearVal) * rd;
                float tx = 2.0f * (centerX + posX) / vw - 1.0f;
                float ty = 2.0f * (centerY + posY) / vh - 1.0f;
                float tz = 2.0f * farVal * nearVal * rd;

                std::memset(pm, 0, 16 * sizeof(float));
                pm[0] = sx;  pm[2] = tx;
                pm[5] = sy;  pm[6] = ty;
                pm[10] = sz; pm[11] = tz;
                pm[14] = 1.0f;
                break;
            }
            case COMMAND_PERSPECTIVE_WH: {
                float zNear = static_cast<float>(nearZ);
                float zFar = static_cast<float>(farZ);
                float width = (perspectiveWidth == 0) ? vw : (perspectiveWidth * TO_FLOAT);
                float height = (perspectiveHeight == 0) ? vh : (perspectiveHeight * TO_FLOAT);
                float rd = 1.0f / (zNear - zFar);
                float sx = 2.0f * zNear / width;
                float sy = 2.0f * zNear / height;
                float sz = -(zNear + zFar) * rd;
                float tx = 2.0f * (centerX + posX) / vw - 1.0f;
                float ty = 2.0f * (centerY + posY) / vh - 1.0f;
                float tz = 2.0f * zFar * zNear * rd;

                std::memset(pm, 0, 16 * sizeof(float));
                pm[0] = sx;  pm[2] = tx;
                pm[5] = sy;  pm[6] = ty;
                pm[10] = sz; pm[11] = tz;
                pm[14] = 1.0f;
                break;
            }
        }
    }
};

} // namespace micro3d
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_MICRO3D_MATH_H
