#include "pack/Pack.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>

#include "data/JsonValue.hpp"
#include "miniz.h"

namespace voxel {

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string normaliseSep(std::string path) {
    for (char& c : path) {
        if (c == '\\') c = '/';
    }
    return path;
}

static std::string readStream(std::ifstream& stream) {
    std::ostringstream buf;
    buf << stream.rdbuf();
    return buf.str();
}

static bool isValidPackId(const std::string& id) {
    if (id.empty()) return false;
    for (const char c : id) {
        const auto ch = static_cast<unsigned char>(c);
        if (std::isalnum(ch) || c == '_' || c == '-' || c == '.') continue;
        return false;
    }
    return true;
}

// ── Pack ──────────────────────────────────────────────────────────────────────

Pack::Pack(std::filesystem::path path)
    : path_(std::move(path))
{
    isZip_ = path_.extension() == ".zip";
    id_    = path_.stem().string();
    loadManifest();
}

void Pack::loadManifest() {
    const auto text = readFile("pack.json");
    if (!text) {
        std::cerr << "Pack '" << id_ << "': missing pack.json\n";
        return;
    }

    const JsonValue root = parseJson(*text);
    if (!root.isObject()) return;

    const auto& obj = root.asObject();

    const auto str = [&](const char* key) -> std::string {
        const auto it = obj.find(key);
        return (it != obj.end() && it->second.isString()) ? it->second.asString() : "";
    };

    const auto integer = [&](const char* key, const int fallback) -> int {
        const auto it = obj.find(key);
        return (it != obj.end() && it->second.isNumber()) ? static_cast<int>(it->second.asNumber()) : fallback;
    };

    const auto stringList = [&](const char* key) -> std::vector<std::string> {
        std::vector<std::string> values;
        const auto it = obj.find(key);
        if (it == obj.end() || !it->second.isArray()) return values;

        for (const auto& value : it->second.asArray()) {
            if (value.isString()) values.push_back(value.asString());
        }
        return values;
    };

    if (const std::string manifestId = str("id"); !manifestId.empty()) {
        if (isValidPackId(manifestId)) {
            manifest_.id = manifestId;
            id_ = manifestId;
        } else {
            std::cerr << "Pack '" << id_ << "': invalid manifest id '" << manifestId
                      << "'; using folder/archive id\n";
        }
    } else {
        std::cerr << "Pack '" << id_ << "': missing manifest id; using folder/archive id\n";
    }

    if (manifest_.id.empty()) manifest_.id = id_;

    manifest_.name        = str("name");
    manifest_.version     = str("version");
    manifest_.description = str("description");
    manifest_.gameVersion = str("gameVersion");
    if (manifest_.gameVersion.empty()) manifest_.gameVersion = str("game_version");
    manifest_.apiVersion = integer("apiVersion", 1);

    if (manifest_.name.empty()) std::cerr << "Pack '" << id_ << "': missing manifest name\n";
    if (manifest_.version.empty()) std::cerr << "Pack '" << id_ << "': missing manifest version\n";
    if (manifest_.apiVersion < 1) {
        std::cerr << "Pack '" << id_ << "': apiVersion must be >= 1; using 1\n";
        manifest_.apiVersion = 1;
    }

    manifest_.dependencies         = stringList("dependencies");
    manifest_.optionalDependencies = stringList("optionalDependencies");

    if (const auto scriptsIt = obj.find("scripts"); scriptsIt != obj.end() && scriptsIt->second.isObject()) {
        const auto& scripts = scriptsIt->second.asObject();
        const auto scriptFlag = [&](const char* key, const bool fallback) {
            const auto it = scripts.find(key);
            return (it != scripts.end() && it->second.isBool()) ? it->second.asBool() : fallback;
        };

        manifest_.scripts.startup = scriptFlag("startup", manifest_.scripts.startup);
        manifest_.scripts.server  = scriptFlag("server",  manifest_.scripts.server);
        manifest_.scripts.client  = scriptFlag("client",  manifest_.scripts.client);
    } else if (const auto scriptsIt = obj.find("scripts"); scriptsIt != obj.end()) {
        std::cerr << "Pack '" << id_ << "': manifest scripts must be an object\n";
    }

    for (const auto& dep : manifest_.dependencies) {
        if (!isValidPackId(dep)) {
            std::cerr << "Pack '" << id_ << "': invalid dependency id '" << dep << "'\n";
        }
    }
    for (const auto& dep : manifest_.optionalDependencies) {
        if (!isValidPackId(dep)) {
            std::cerr << "Pack '" << id_ << "': invalid optional dependency id '" << dep << "'\n";
        }
    }
}

std::optional<std::string> Pack::readFile(const std::string& relativePath) const {
    return isZip_ ? readFileFromZip(relativePath) : readFileFromFolder(relativePath);
}

bool Pack::hasFile(const std::string& relativePath) const {
    return readFile(relativePath).has_value();
}

std::vector<std::string> Pack::listFiles(const std::string& subdir) const {
    return isZip_ ? listFilesFromZip(subdir) : listFilesFromFolder(subdir);
}

// ── Folder implementation ─────────────────────────────────────────────────────

std::optional<std::string> Pack::readFileFromFolder(const std::string& relativePath) const {
    const auto fullPath = path_ / relativePath;
    std::ifstream stream(fullPath);
    if (!stream) return std::nullopt;
    return readStream(stream);
}

std::vector<std::string> Pack::listFilesFromFolder(const std::string& subdir) const {
    const auto dir = path_ / subdir;
    if (!std::filesystem::exists(dir)) return {};

    std::vector<std::string> result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const auto rel = std::filesystem::relative(entry.path(), path_);
        result.push_back(normaliseSep(rel.string()));
    }
    return result;
}

// ── Zip implementation ────────────────────────────────────────────────────────

std::optional<std::string> Pack::readFileFromZip(const std::string& relativePath) const {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, path_.string().c_str(), 0)) return std::nullopt;

    const int index = mz_zip_reader_locate_file(&zip, relativePath.c_str(), nullptr, 0);
    if (index < 0) {
        mz_zip_reader_end(&zip);
        return std::nullopt;
    }

    std::size_t size = 0;
    void* data = mz_zip_reader_extract_to_heap(&zip, static_cast<mz_uint>(index), &size, 0);
    mz_zip_reader_end(&zip);

    if (!data) return std::nullopt;

    std::string content(static_cast<char*>(data), size);
    mz_free(data);
    return content;
}

std::vector<std::string> Pack::listFilesFromZip(const std::string& subdir) const {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, path_.string().c_str(), 0)) return {};

    const std::string prefix = subdir.empty() ? "" : (normaliseSep(subdir) + "/");
    std::vector<std::string> result;

    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        if (stat.m_is_directory) continue;

        std::string name = normaliseSep(stat.m_filename);
        if (name.starts_with(prefix)) result.push_back(std::move(name));
    }

    mz_zip_reader_end(&zip);
    return result;
}

}  // namespace voxel
