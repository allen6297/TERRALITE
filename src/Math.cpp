#include "Math.hpp"

namespace voxel {
float toRadians(const float degrees) {
    return degrees * (kPi / 180.0f);
}

Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(const Vec3& v, const float scalar) {
    return {v.x * scalar, v.y * scalar, v.z * scalar};
}

float length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    if (len <= 0.0001f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return {v.x / len, v.y / len, v.z / len};
}
}  // namespace voxel
