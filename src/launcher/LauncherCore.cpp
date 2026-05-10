#include "launcher/LauncherCore.hpp"

#include "data/JsonValue.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace terralite::launcher {
namespace {

const voxel::JsonValue* objectValue(const voxel::JsonValue::Object& object, const std::string& key) {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::string stringValue(const voxel::JsonValue::Object& object, const std::string& key, const std::string& fallback = {}) {
    const voxel::JsonValue* value = objectValue(object, key);
    return value != nullptr && value->isString() ? value->asString() : fallback;
}

voxel::JsonValue jsonString(const std::string& value) {
    return voxel::JsonValue {value};
}

voxel::JsonValue accountToJson(const AccountProfile& account) {
    voxel::JsonValue::Object object;
    object["displayName"] = jsonString(account.displayName);
    object["id"] = jsonString(account.id);
    object["type"] = jsonString(account.type);
    return voxel::JsonValue {object};
}

voxel::JsonValue versionToJson(const GameVersion& version) {
    voxel::JsonValue::Object object;
    object["channel"] = jsonString(version.channel);
    object["extraArguments"] = jsonString(version.extraArguments);
    object["gameExecutable"] = jsonString(pathString(version.gameExecutable));
    object["id"] = jsonString(version.id);
    object["name"] = jsonString(version.name);
    object["serverExecutable"] = jsonString(pathString(version.serverExecutable));
    object["source"] = jsonString(version.source);
    object["workingDirectory"] = jsonString(pathString(version.workingDirectory));
    return voxel::JsonValue {object};
}

GameVersion gameVersionFromObject(const voxel::JsonValue::Object& object, const std::filesystem::path& baseDirectory = {}) {
    GameVersion version;
    version.id = stringValue(object, "id");
    version.name = stringValue(object, "name", version.id);
    version.channel = stringValue(object, "channel", "local");
    version.source = stringValue(object, "source", "manual");
    version.gameExecutable = stringValue(object, "gameExecutable");
    version.serverExecutable = stringValue(object, "serverExecutable");
    version.workingDirectory = stringValue(object, "workingDirectory");
    version.extraArguments = stringValue(object, "extraArguments");

    if (!baseDirectory.empty()) {
        if (version.gameExecutable.is_relative()) {
            version.gameExecutable = baseDirectory / version.gameExecutable;
        }
        if (version.serverExecutable.is_relative()) {
            version.serverExecutable = baseDirectory / version.serverExecutable;
        }
        if (version.workingDirectory.empty()) {
            version.workingDirectory = baseDirectory;
        } else if (version.workingDirectory.is_relative()) {
            version.workingDirectory = baseDirectory / version.workingDirectory;
        }
    }

    return version;
}

voxel::JsonValue manifestToJson(const VersionManifest& manifest) {
    voxel::JsonValue::Object object = std::get<voxel::JsonValue::Object>(versionToJson(manifest.version).value);
    object["installed"] = voxel::JsonValue {manifest.installed};
    return voxel::JsonValue {object};
}

std::vector<std::string> splitExtraArguments(const std::string& input) {
    std::istringstream stream(input);
    std::vector<std::string> result;
    std::string part;
    while (stream >> part) {
        result.push_back(part);
    }
    return result;
}

std::string launchProcess(
    const std::filesystem::path& executable,
    const std::filesystem::path& workingDirectory,
    const std::vector<std::string>& arguments) {
    if (executable.empty()) {
        return "No executable is configured.";
    }
    if (!std::filesystem::exists(executable)) {
        return "Executable not found: " + executable.string();
    }
    if (!workingDirectory.empty() && !std::filesystem::exists(workingDirectory)) {
        return "Working directory not found: " + workingDirectory.string();
    }

#ifdef _WIN32
    std::string command = "\"" + executable.string() + "\"";
    for (const std::string& argument : arguments) {
        command += " \"" + argument + "\"";
    }

    STARTUPINFOA startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    std::string working = workingDirectory.empty() ? executable.parent_path().string() : workingDirectory.string();
    if (!CreateProcessA(
            nullptr,
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            working.c_str(),
            &startup,
            &process)) {
        return "Failed to start process.";
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
#else
    std::vector<std::string> ownedArgs;
    ownedArgs.push_back(executable.string());
    ownedArgs.insert(ownedArgs.end(), arguments.begin(), arguments.end());

    std::vector<char*> argv;
    argv.reserve(ownedArgs.size() + 1);
    for (std::string& argument : ownedArgs) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    const std::filesystem::path working = workingDirectory.empty() ? executable.parent_path() : workingDirectory;
    const pid_t pid = fork();
    if (pid == 0) {
        if (chdir(working.c_str()) != 0) {
            _exit(127);
        }
        execv(executable.c_str(), argv.data());
        _exit(127);
    }
    if (pid < 0) {
        return std::string("Failed to start process: ") + std::strerror(errno);
    }
#endif
    return "Started " + executable.filename().string();
}

void copyRequiredFile(const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (source.empty() || !std::filesystem::exists(source)) {
        throw std::runtime_error("Install source file not found: " + source.string());
    }
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::copy_file(
        source,
        destination,
        std::filesystem::copy_options::overwrite_existing);

    std::error_code permissionsError;
    std::filesystem::permissions(
        destination,
        std::filesystem::status(source).permissions(),
        std::filesystem::perm_options::replace,
        permissionsError);
}

void copyOptionalDirectory(const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (source.empty() || !std::filesystem::exists(source)) {
        return;
    }
    if (!std::filesystem::is_directory(source)) {
        throw std::runtime_error("Install source is not a directory: " + source.string());
    }
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::copy(
        source,
        destination,
        std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::overwrite_existing);
}

}  // namespace

std::filesystem::path executablePath(const char* argv0) {
#ifdef _WIN32
    std::array<char, MAX_PATH> path {};
    const DWORD length = GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length > 0) {
        return std::filesystem::path(std::string(path.data(), length));
    }
#elif defined(__APPLE__)
    std::array<char, PATH_MAX> path {};
    uint32_t size = static_cast<uint32_t>(path.size());
    if (_NSGetExecutablePath(path.data(), &size) == 0) {
        return std::filesystem::weakly_canonical(path.data());
    }
#else
    std::array<char, PATH_MAX> path {};
    const ssize_t length = readlink("/proc/self/exe", path.data(), path.size() - 1);
    if (length > 0) {
        path[static_cast<std::size_t>(length)] = '\0';
        return std::filesystem::path(path.data());
    }
#endif
    return std::filesystem::absolute(argv0);
}

std::filesystem::path appDataDirectory() {
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA")) {
        return std::filesystem::path(appdata) / "TERRALITE" / "Launcher";
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "TERRALITE" / "Launcher";
    }
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME")) {
        return std::filesystem::path(xdg) / "TERRALITE" / "Launcher";
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share" / "TERRALITE" / "Launcher";
    }
#endif
    return std::filesystem::current_path() / "launcher-data";
}

std::filesystem::path siblingExecutable(const std::filesystem::path& launcherPath, const std::string& name) {
#ifdef _WIN32
    return launcherPath.parent_path() / (name + ".exe");
#else
    return launcherPath.parent_path() / name;
#endif
}

std::string pathString(const std::filesystem::path& path) {
    return path.lexically_normal().string();
}

std::string slugFromName(const std::string& name) {
    std::string slug;
    for (const char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            slug.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (!slug.empty() && slug.back() != '-') {
            slug.push_back('-');
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug.empty() ? "player" : slug;
}

LauncherState defaultState(const std::filesystem::path& launcherPath, const std::filesystem::path& sourceDirectory) {
    LauncherState state;
    state.accounts.push_back(AccountProfile {
        .id = "offline-player",
        .displayName = "Player",
        .type = "offline",
    });

    state.versions.push_back(GameVersion {
        .id = "local-dev",
        .name = "Local development build",
        .channel = "dev",
        .source = "local-dev",
        .gameExecutable = siblingExecutable(launcherPath, "Terralite"),
        .serverExecutable = siblingExecutable(launcherPath, "TerraliteServer"),
        .workingDirectory = sourceDirectory,
        .extraArguments = "",
    });

    state.selectedAccountId = state.accounts.front().id;
    state.selectedVersionId = state.versions.front().id;
    return state;
}

void saveState(const LauncherState& state, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());

    voxel::JsonValue::Array accounts;
    for (const AccountProfile& account : state.accounts) {
        accounts.push_back(accountToJson(account));
    }

    voxel::JsonValue::Array versions;
    for (const GameVersion& version : state.versions) {
        versions.push_back(versionToJson(version));
    }

    voxel::JsonValue::Object root;
    root["accounts"] = voxel::JsonValue {accounts};
    root["selectedAccountId"] = jsonString(state.selectedAccountId);
    root["selectedVersionId"] = jsonString(state.selectedVersionId);
    root["versions"] = voxel::JsonValue {versions};

    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Could not write launcher state: " + path.string());
    }
    file << voxel::serializeJson(voxel::JsonValue {root}) << '\n';
}

LauncherState loadState(
    const std::filesystem::path& path,
    const std::filesystem::path& launcherPath,
    const std::filesystem::path& sourceDirectory) {
    if (!std::filesystem::exists(path)) {
        return defaultState(launcherPath, sourceDirectory);
    }

    try {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();

        LauncherState state;
        const voxel::JsonValue root = voxel::parseJson(buffer.str());
        const auto& object = root.asObject();

        if (const voxel::JsonValue* accounts = objectValue(object, "accounts");
            accounts != nullptr && accounts->isArray()) {
            for (const voxel::JsonValue& value : accounts->asArray()) {
                if (!value.isObject()) continue;
                const auto& accountObject = value.asObject();
                AccountProfile account;
                account.id = stringValue(accountObject, "id");
                account.displayName = stringValue(accountObject, "displayName", account.id);
                account.type = stringValue(accountObject, "type", "offline");
                if (!account.id.empty()) {
                    state.accounts.push_back(account);
                }
            }
        }

        if (const voxel::JsonValue* versions = objectValue(object, "versions");
            versions != nullptr && versions->isArray()) {
            for (const voxel::JsonValue& value : versions->asArray()) {
                if (!value.isObject()) continue;
                const auto& versionObject = value.asObject();
                GameVersion version = gameVersionFromObject(versionObject);
                if (!version.id.empty()) {
                    state.versions.push_back(version);
                }
            }
        }

        if (state.accounts.empty() || state.versions.empty()) {
            return defaultState(launcherPath, sourceDirectory);
        }

        state.selectedAccountId = stringValue(object, "selectedAccountId", state.accounts.front().id);
        state.selectedVersionId = stringValue(object, "selectedVersionId", state.versions.front().id);
        return state;
    } catch (const std::exception&) {
        return defaultState(launcherPath, sourceDirectory);
    }
}

AccountProfile* selectedAccount(LauncherState& state) {
    auto it = std::find_if(state.accounts.begin(), state.accounts.end(), [&](const AccountProfile& account) {
        return account.id == state.selectedAccountId;
    });
    return it == state.accounts.end() ? nullptr : &*it;
}

const AccountProfile* selectedAccount(const LauncherState& state) {
    auto it = std::find_if(state.accounts.begin(), state.accounts.end(), [&](const AccountProfile& account) {
        return account.id == state.selectedAccountId;
    });
    return it == state.accounts.end() ? nullptr : &*it;
}

GameVersion* selectedVersion(LauncherState& state) {
    auto it = std::find_if(state.versions.begin(), state.versions.end(), [&](const GameVersion& version) {
        return version.id == state.selectedVersionId;
    });
    return it == state.versions.end() ? nullptr : &*it;
}

const GameVersion* selectedVersion(const LauncherState& state) {
    auto it = std::find_if(state.versions.begin(), state.versions.end(), [&](const GameVersion& version) {
        return version.id == state.selectedVersionId;
    });
    return it == state.versions.end() ? nullptr : &*it;
}

AccountProfile addAccount(LauncherState& state) {
    const std::string id = "offline-" + std::to_string(state.accounts.size() + 1);
    AccountProfile account {
        .id = id,
        .displayName = "Player " + std::to_string(state.accounts.size() + 1),
        .type = "offline",
    };
    state.accounts.push_back(account);
    state.selectedAccountId = id;
    return account;
}

void removeSelectedAccount(LauncherState& state) {
    if (state.accounts.size() <= 1) return;
    state.accounts.erase(std::remove_if(state.accounts.begin(), state.accounts.end(), [&](const AccountProfile& account) {
        return account.id == state.selectedAccountId;
    }), state.accounts.end());
    state.selectedAccountId = state.accounts.front().id;
}

GameVersion addVersion(LauncherState& state) {
    const std::string id = "version-" + std::to_string(state.versions.size() + 1);
    const GameVersion* current = selectedVersion(state);
    GameVersion version = current != nullptr ? *current : GameVersion {};
    version.id = id;
    version.name = "New Version";
    state.versions.push_back(version);
    state.selectedVersionId = id;
    return version;
}

void removeSelectedVersion(LauncherState& state) {
    if (state.versions.size() <= 1) return;
    state.versions.erase(std::remove_if(state.versions.begin(), state.versions.end(), [&](const GameVersion& version) {
        return version.id == state.selectedVersionId;
    }), state.versions.end());
    state.selectedVersionId = state.versions.front().id;
}

void renameSelectedAccount(LauncherState& state, const std::string& displayName) {
    AccountProfile* account = selectedAccount(state);
    if (account == nullptr) return;
    account->displayName = displayName;
    account->id = slugFromName(account->displayName);
    state.selectedAccountId = account->id;
}

void updateSelectedVersion(LauncherState& state, const GameVersion& updatedVersion) {
    GameVersion* version = selectedVersion(state);
    if (version == nullptr) return;
    *version = updatedVersion;
    state.selectedVersionId = version->id;
}

std::string versionStatusLabel(const VersionStatus status) {
    switch (status) {
        case VersionStatus::LocalDev: return "Local dev";
        case VersionStatus::Installed: return "Installed";
        case VersionStatus::MissingExecutable: return "Missing executable";
        case VersionStatus::InvalidManifest: return "Invalid manifest";
    }
    return "Unknown";
}

VersionStatus versionStatus(const GameVersion& version) {
    if (version.source == "invalid-manifest") {
        return VersionStatus::InvalidManifest;
    }
    if (version.id.empty() || version.name.empty()) {
        return VersionStatus::InvalidManifest;
    }
    if (version.source == "local-dev") {
        return VersionStatus::LocalDev;
    }
    if (version.gameExecutable.empty() || !std::filesystem::exists(version.gameExecutable)) {
        return VersionStatus::MissingExecutable;
    }
    if (version.serverExecutable.empty() || !std::filesystem::exists(version.serverExecutable)) {
        return VersionStatus::MissingExecutable;
    }
    return VersionStatus::Installed;
}

VersionManifest loadVersionManifest(const std::filesystem::path& manifestPath) {
    std::ifstream file(manifestPath);
    if (!file) {
        throw std::runtime_error("Could not read version manifest: " + manifestPath.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const voxel::JsonValue root = voxel::parseJson(buffer.str());
    const auto& object = root.asObject();

    VersionManifest manifest;
    manifest.version = gameVersionFromObject(object, manifestPath.parent_path());
    if (const voxel::JsonValue* installed = objectValue(object, "installed");
        installed != nullptr && installed->isBool()) {
        manifest.installed = installed->asBool();
    }
    manifest.version.source = stringValue(object, "source", "manifest");
    return manifest;
}

void saveVersionManifest(const VersionManifest& manifest, const std::filesystem::path& manifestPath) {
    std::filesystem::create_directories(manifestPath.parent_path());
    std::ofstream file(manifestPath);
    if (!file) {
        throw std::runtime_error("Could not write version manifest: " + manifestPath.string());
    }
    file << voxel::serializeJson(manifestToJson(manifest)) << '\n';
}

std::vector<GameVersion> discoverInstalledVersions(const std::filesystem::path& versionRoot) {
    std::vector<GameVersion> versions;
    if (!std::filesystem::exists(versionRoot)) {
        return versions;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(versionRoot)) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::filesystem::path manifestPath = entry.path() / "manifest.json";
        if (!std::filesystem::exists(manifestPath)) {
            continue;
        }

        try {
            VersionManifest manifest = loadVersionManifest(manifestPath);
            if (manifest.installed && !manifest.version.id.empty()) {
                versions.push_back(manifest.version);
            }
        } catch (const std::exception&) {
            GameVersion invalid;
            invalid.id = entry.path().filename().string();
            invalid.name = invalid.id;
            invalid.source = "invalid-manifest";
            invalid.workingDirectory = entry.path();
            versions.push_back(invalid);
        }
    }

    std::sort(versions.begin(), versions.end(), [](const GameVersion& lhs, const GameVersion& rhs) {
        return lhs.id < rhs.id;
    });
    return versions;
}

void mergeDiscoveredVersions(LauncherState& state, const std::vector<GameVersion>& discoveredVersions) {
    for (const GameVersion& discovered : discoveredVersions) {
        auto it = std::find_if(state.versions.begin(), state.versions.end(), [&](const GameVersion& version) {
            return version.id == discovered.id;
        });
        if (it == state.versions.end()) {
            state.versions.push_back(discovered);
        } else if (it->source == "manifest" || it->source == "invalid-manifest") {
            *it = discovered;
        }
    }
}

GameVersion installLocalBuildVersion(const LocalBuildInstallRequest& request) {
    if (request.id.empty()) {
        throw std::runtime_error("Version id is required.");
    }
    if (request.name.empty()) {
        throw std::runtime_error("Version name is required.");
    }
    if (request.versionRoot.empty()) {
        throw std::runtime_error("Version root is required.");
    }

    const std::filesystem::path installDirectory = request.versionRoot / request.id;
    if (std::filesystem::exists(installDirectory)) {
        if (!request.overwrite) {
            throw std::runtime_error("Version already exists: " + installDirectory.string());
        }
        std::filesystem::remove_all(installDirectory);
    }

    std::filesystem::create_directories(installDirectory);
    const std::filesystem::path gameFileName =
        request.gameExecutable.empty() ? std::filesystem::path("Terralite") : request.gameExecutable.filename();
    const std::filesystem::path serverFileName =
        request.serverExecutable.empty() ? std::filesystem::path("TerraliteServer") : request.serverExecutable.filename();

    copyRequiredFile(request.gameExecutable, installDirectory / gameFileName);
    copyRequiredFile(request.serverExecutable, installDirectory / serverFileName);
    copyOptionalDirectory(request.sourcePacksDirectory, installDirectory / "packs");

    VersionManifest manifest;
    manifest.version.id = request.id;
    manifest.version.name = request.name;
    manifest.version.channel = request.channel.empty() ? "local" : request.channel;
    manifest.version.source = "manifest";
    manifest.version.gameExecutable = gameFileName;
    manifest.version.serverExecutable = serverFileName;
    manifest.version.workingDirectory = ".";
    manifest.version.extraArguments = request.extraArguments;
    manifest.installed = true;
    saveVersionManifest(manifest, installDirectory / "manifest.json");

    return loadVersionManifest(installDirectory / "manifest.json").version;
}

std::vector<std::string> gameArguments(
    const AccountProfile& account,
    const GameVersion& version,
    const LaunchOptions& options) {
    std::vector<std::string> arguments = splitExtraArguments(version.extraArguments);
    arguments.push_back("--name");
    arguments.push_back(account.displayName);

    if (options.playMode == PlayMode::Host) {
        arguments.push_back("--host");
        arguments.push_back(options.port);
    } else if (options.playMode == PlayMode::Connect) {
        arguments.push_back("--connect");
        arguments.push_back(options.hostName);
        arguments.push_back(options.port);
    }

    return arguments;
}

std::string launchGame(const AccountProfile& account, const GameVersion& version, const LaunchOptions& options) {
    return launchProcess(
        version.gameExecutable,
        version.workingDirectory,
        gameArguments(account, version, options));
}

std::string launchServer(const GameVersion& version, const std::string& port) {
    return launchProcess(
        version.serverExecutable,
        version.workingDirectory,
        {"--port", port});
}

}  // namespace terralite::launcher
