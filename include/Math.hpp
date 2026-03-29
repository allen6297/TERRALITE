#pragma once

#include <cmath>

namespace voxel {
constexpr float kPi = 3.14159265358979323846f;

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Int3 {
    int x;
    int y;
    int z;
};

float toRadians(float degrees);
Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& v, float scalar);
float length(const Vec3& v);
float dot(const Vec3& a, const Vec3& b);
Vec3 cross(const Vec3& a, const Vec3& b);
Vec3 normalize(const Vec3& v);
}  // namespace voxel
