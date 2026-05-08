#pragma once

// Inline helpers for reading typed values from QuickJS objects.
// Used by ScriptManager.cpp and generated parse bindings.

#include <array>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <quickjs.h>

namespace voxel::js {

inline std::string jsStr(JSContext* ctx, JSValueConst obj, const char* key, const char* def = "") {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) { JS_FreeValue(ctx, v); return def; }
    const char* s = JS_ToCString(ctx, v);
    std::string r = s ? s : def;
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, v);
    return r;
}

inline std::optional<std::string> jsOptStr(JSContext* ctx, JSValueConst obj, const char* key) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) { JS_FreeValue(ctx, v); return std::nullopt; }
    const char* s = JS_ToCString(ctx, v);
    std::optional<std::string> r = s ? std::optional<std::string>(s) : std::nullopt;
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, v);
    return r;
}

inline void jsRequire(JSContext* ctx, JSValueConst obj, const char* key) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    const bool missing = JS_IsUndefined(v) || JS_IsNull(v);
    JS_FreeValue(ctx, v);

    if (missing) {
        throw std::runtime_error(std::string("missing required field '") + key + "'");
    }
}

inline std::string jsEnum(JSContext* ctx, JSValueConst obj, const char* key,
                          std::initializer_list<const char*> values, const char* def = "") {
    std::string value = jsStr(ctx, obj, key, def);
    for (const char* allowed : values) {
        if (value == allowed) return value;
    }

    throw std::runtime_error(std::string("field '") + key + "' has invalid enum value '" +
                             value + "'");
}

inline std::vector<std::string> jsStringArray(JSContext* ctx, JSValueConst obj, const char* key) {
    std::vector<std::string> values;
    JSValue arr = JS_GetPropertyStr(ctx, obj, key);
    if (!JS_IsUndefined(arr) && !JS_IsNull(arr)) {
        JSValue lenVal = JS_GetPropertyStr(ctx, arr, "length");
        uint32_t len = 0;
        JS_ToUint32(ctx, &len, lenVal);
        JS_FreeValue(ctx, lenVal);
        values.reserve(len);
        for (uint32_t i = 0; i < len; ++i) {
            JSValue entry = JS_GetPropertyUint32(ctx, arr, i);
            const char* s = JS_ToCString(ctx, entry);
            if (s) values.emplace_back(s);
            JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, entry);
        }
    }
    JS_FreeValue(ctx, arr);
    return values;
}

inline bool isNamespacedId(const std::string& value) {
    const auto colon = value.find(':');
    return colon != std::string::npos && colon > 0 && colon + 1 < value.size();
}

inline bool isPackAssetPath(const std::string& value) {
    if (value.empty() || value.starts_with("/") || value.starts_with("../")) return false;
    return value.find("://") == std::string::npos;
}

inline void jsValidate(JSContext*, const char* field, const std::string& value, const char* validator) {
    if (value.empty()) return;

    const std::string kind = validator;
    if (kind == "namespaced_id" || kind == "block_id" || kind == "item_id" ||
        kind == "biome_id" || kind == "tag_id" || kind == "recipe_id") {
        if (!isNamespacedId(value)) {
            throw std::runtime_error(std::string("field '") + field +
                                     "' must be a namespaced id, got '" + value + "'");
        }
        return;
    }

    if (kind == "texture_path" || kind == "model_path") {
        if (!isPackAssetPath(value)) {
            throw std::runtime_error(std::string("field '") + field +
                                     "' must be a relative pack asset path, got '" + value + "'");
        }
        return;
    }

    if (kind == "sound_id") return;
}

inline void jsValidate(JSContext*, const char* field, int value, const char* validator) {
    const std::string kind = validator;
    if (kind == "positive_int" && value <= 0) {
        throw std::runtime_error(std::string("field '") + field + "' must be a positive integer");
    }
}

inline void jsValidate(JSContext*, const char* field, float value, const char* validator) {
    const std::string kind = validator;
    if (kind == "non_negative_float" && value < 0.0f) {
        throw std::runtime_error(std::string("field '") + field + "' must be non-negative");
    }
    if (kind == "opacity_0_1" && (value < 0.0f || value > 1.0f)) {
        throw std::runtime_error(std::string("field '") + field + "' must be between 0 and 1");
    }
}

inline void jsValidate(JSContext*, const char* field, const std::array<float, 3>& value,
                       const char* validator) {
    if (std::string(validator) != "rgb_0_1") return;
    for (const float channel : value) {
        if (channel < 0.0f || channel > 1.0f) {
            throw std::runtime_error(std::string("field '") + field +
                                     "' RGB channels must be between 0 and 1");
        }
    }
}

inline void jsValidate(JSContext* ctx, const char* field, const std::optional<std::string>& value,
                       const char* validator) {
    if (value) jsValidate(ctx, field, *value, validator);
}

inline void jsValidate(JSContext* ctx, const char* field, const std::vector<std::string>& values,
                       const char* validator) {
    for (const auto& value : values) jsValidate(ctx, field, value, validator);
}

inline bool jsBool(JSContext* ctx, JSValueConst obj, const char* key, bool def = false) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) { JS_FreeValue(ctx, v); return def; }
    bool r = JS_ToBool(ctx, v) > 0;
    JS_FreeValue(ctx, v);
    return r;
}

inline int jsInt(JSContext* ctx, JSValueConst obj, const char* key, int def = 0) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) { JS_FreeValue(ctx, v); return def; }
    int32_t i = 0;
    JS_ToInt32(ctx, &i, v);
    JS_FreeValue(ctx, v);
    return i;
}

inline float jsFloat(JSContext* ctx, JSValueConst obj, const char* key, float def = 0.0f) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(v) || JS_IsNull(v)) { JS_FreeValue(ctx, v); return def; }
    double d = 0.0;
    JS_ToFloat64(ctx, &d, v);
    JS_FreeValue(ctx, v);
    return static_cast<float>(d);
}

inline std::array<float, 3> jsColor(JSContext* ctx, JSValueConst obj, const char* key,
                                     std::array<float, 3> def = {1.0f, 1.0f, 1.0f}) {
    JSValue arr = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsUndefined(arr) || JS_IsNull(arr)) { JS_FreeValue(ctx, arr); return def; }
    std::array<float, 3> r = def;
    for (uint32_t i = 0; i < 3; ++i) {
        JSValue e = JS_GetPropertyUint32(ctx, arr, i);
        double d = 0.0;
        JS_ToFloat64(ctx, &d, e);
        r[i] = static_cast<float>(d);
        JS_FreeValue(ctx, e);
    }
    JS_FreeValue(ctx, arr);
    return r;
}

template<typename Fn>
inline void jsForEachProp(JSContext* ctx, JSValueConst obj, Fn fn) {
    if (JS_IsUndefined(obj) || JS_IsNull(obj)) return;
    JSPropertyEnum* ptab = nullptr;
    uint32_t plen = 0;
    if (JS_GetOwnPropertyNames(ctx, &ptab, &plen, obj,
                               JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) return;
    for (uint32_t i = 0; i < plen; ++i) {
        JSValue val = JS_GetProperty(ctx, obj, ptab[i].atom);
        const char* key = JS_AtomToCString(ctx, ptab[i].atom);
        fn(key, val);
        JS_FreeCString(ctx, key);
        JS_FreeValue(ctx, val);
        JS_FreeAtom(ctx, ptab[i].atom);
    }
    js_free(ctx, ptab);
}

inline uint32_t jsArrayLen(JSContext* ctx, JSValueConst arr) {
    JSValue lenVal = JS_GetPropertyStr(ctx, arr, "length");
    uint32_t len = 0;
    JS_ToUint32(ctx, &len, lenVal);
    JS_FreeValue(ctx, lenVal);
    return len;
}

} // namespace voxel::js
