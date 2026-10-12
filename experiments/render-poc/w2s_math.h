#pragma once

// ISOLATED EXPERIMENT — not part of the `architect` target, not shipped.
// Pure world->screen math for the DX Present-hook overlay PoC (approach 2b).
// No engine headers, no hook code here: this header lets us unit-test the
// projection independently of the game, so that when the PoC runs, any
// misalignment points at the camera-input capture, not the math.
//
// Coordinate conventions used HERE (internal to the experiment):
//   world: +X east, +Y up, +Z south (Minecraft axes).
//   camera: position (eye), forward unit vector F, world-up (0,1,0).
//   NDC: DirectX convention, x in [-1,1] left->right, y in [-1,1]
//        bottom->top, z in [0,1] near->far. Screen: y down.
//
// UNVERIFIED UNTIL MEASURED IN-GAME (PoC tasks P1-P3 in README.md):
//   (a) how the game's yaw/pitch degrees map to a forward vector
//       (sign conventions, degree/radian, yaw zero direction);
//   (b) whether the effective FOV is vertical, and its exact value source;
//   (c) aspect ratio source on resize (swap-chain buffers vs window rect);
//   (d) DirectX vs OpenGL NDC z-range actually in use by Bedrock GDK build.
// Until (a)-(d) are measured, this math is correct-but-unplugged.

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace architect::experiment {

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Mat4 {
    // Row-major.
    std::array<std::array<float, 4>, 4> m{};

    static Mat4 identity() {
        Mat4 out;
        out.m[0][0] = out.m[1][1] = out.m[2][2] = out.m[3][3] = 1.0F;
        return out;
    }
};

inline Vec3 operator-(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline Vec3 normalize(const Vec3& v) {
    const float len = std::sqrt(dot(v, v));
    return len > 0.0F ? Vec3{v.x / len, v.y / len, v.z / len} : v;
}

// Left-handed look-at (DirectX style): camera looks along +Z in view space.
inline Mat4 lookAtLH(const Vec3& eye, const Vec3& forward, const Vec3& up) {
    const Vec3 f = normalize(forward);
    const Vec3 r = normalize(cross(up, f));
    const Vec3 u = cross(f, r);
    Mat4 v            = Mat4::identity();
    v.m[0][0] = r.x; v.m[0][1] = u.x; v.m[0][2] = f.x; v.m[0][3] = 0.0F;
    v.m[1][0] = r.y; v.m[1][1] = u.y; v.m[1][2] = f.y; v.m[1][3] = 0.0F;
    v.m[2][0] = r.z; v.m[2][1] = u.z; v.m[2][2] = f.z; v.m[2][3] = 0.0F;
    v.m[3][0] = -dot(r, eye);
    v.m[3][1] = -dot(u, eye);
    v.m[3][2] = -dot(f, eye);
    v.m[3][3] = 1.0F;
    return v;
}

// Left-handed perspective (DirectX depth 0..1). fovYRad is the VERTICAL
// field of view in radians; aspect = width/height.
inline Mat4 perspectiveLH(float fovYRad, float aspect, float nearZ, float farZ) {
    const float yScale = 1.0F / std::tan(fovYRad * 0.5F);
    const float xScale = yScale / aspect;
    Mat4 p;
    p.m[0][0] = xScale;
    p.m[1][1] = yScale;
    p.m[2][2] = farZ / (farZ - nearZ);
    p.m[2][3] = 1.0F;
    p.m[3][2] = -nearZ * farZ / (farZ - nearZ);
    return p;
}

struct ScreenPoint {
    float x = 0.0F; // pixels from left
    float y = 0.0F; // pixels from top
    float depth = 0.0F; // 0 (near) .. 1 (far)
    bool behindCamera = false;
};

// viewProj = view * projection (row-major, row-vector convention:
// clip = world * view * projection).
inline ScreenPoint projectPoint(const Vec3& world, const Mat4& viewProj, float widthPx, float heightPx) {
    const float x = world.x * viewProj.m[0][0] + world.y * viewProj.m[1][0] + world.z * viewProj.m[2][0] +
                    viewProj.m[3][0];
    const float y = world.x * viewProj.m[0][1] + world.y * viewProj.m[1][1] + world.z * viewProj.m[2][1] +
                    viewProj.m[3][1];
    const float z = world.x * viewProj.m[0][2] + world.y * viewProj.m[1][2] + world.z * viewProj.m[2][2] +
                    viewProj.m[3][2];
    const float w = world.x * viewProj.m[0][3] + world.y * viewProj.m[1][3] + world.z * viewProj.m[2][3] +
                    viewProj.m[3][3];
    ScreenPoint out;
    if (w <= 0.0F) {
        out.behindCamera = true;
        return out;
    }
    const float ndcX = x / w;
    const float ndcY = y / w;
    out.depth   = z / w;
    out.x       = (ndcX * 0.5F + 0.5F) * widthPx;
    out.y       = (0.5F - ndcY * 0.5F) * heightPx; // screen y down
    return out;
}

// Eight corners of the unit cube [min, max] in world space.
inline std::array<Vec3, 8> cubeCorners(const Vec3& min, const Vec3& max) {
    return std::array<Vec3, 8>{
        Vec3{min.x, min.y, min.z},
        Vec3{max.x, min.y, min.z},
        Vec3{min.x, max.y, min.z},
        Vec3{max.x, max.y, min.z},
        Vec3{min.x, min.y, max.z},
        Vec3{max.x, min.y, max.z},
        Vec3{min.x, max.y, max.z},
        Vec3{max.x, max.y, max.z},
    };
}

} // namespace architect::experiment
