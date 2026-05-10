#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace terralite::launcher {

struct AccountProfile {
    std::string id;
    std::string displayName;
    std::string type = "offline";
};

struct GameVersion {
    std::string id;
    std::string name;
    std::string channel = "local";
    std::string source = "manual";
    std::filesystem::path gameExecutable;
    std::filesystem::path serverExecutable;
    std::filesystem::path workingDirectory;
    std::string extraArguments;
};

enum class VersionStatus {
    LocalDev,
    Installed,
    MissingExecutable,
    InvalidManifest,
};

struct VersionManifest {
    GameVersion version;
    bool installed = true;
};

struct LocalBuildInstallRequest {
    std::string id;
    std::string name;
    std::string channel = "local";
    std::filesystem::path versionRoot;
    std::filesystem::path gameExecutable;
    std::filesystem::path serverExecutable;
    std::filesystem::path sourcePacksDirectory;
    std::string extraArguments;
    bool overwrite = false;
};

struct LauncherState {
    std::vector<AccountProfile> accounts;
    std::vector<GameVersion> versions;
    std::string selectedAccountId;
    std::string selectedVersionId;
};

enum class PlayMode {
    Offline,
    Host,
    Connect,
};

struct LaunchOptions {
    PlayMode playMode = PlayMode::Offline;
    std::string hostName = "127.0.0.1";
    std::string port = "27015";
};

std::filesystem::path executablePath(const char* argv0);
std::filesystem::path appDataDirectory();
std::filesystem::path siblingExecutable(const std::filesystem::path& launcherPath, const std::string& name);
std::string pathString(const std::filesystem::path& path);
std::string slugFromName(const std::string& name);
std::string versionStatusLabel(VersionStatus status);
VersionStatus versionStatus(const GameVersion& version);

LauncherState defaultState(const std::filesystem::path& launcherPath, const std::filesystem::path& sourceDirectory);
LauncherState loadState(
    const std::filesystem::path& path,
    const std::filesystem::path& launcherPath,
    const std::filesystem::path& sourceDirectory);
void saveState(const LauncherState& state, const std::filesystem::path& path);

AccountProfile* selectedAccount(LauncherState& state);
const AccountProfile* selectedAccount(const LauncherState& state);
GameVersion* selectedVersion(LauncherState& state);
const GameVersion* selectedVersion(const LauncherState& state);

AccountProfile addAccount(LauncherState& state);
void removeSelectedAccount(LauncherState& state);
GameVersion addVersion(LauncherState& state);
void removeSelectedVersion(LauncherState& state);
void renameSelectedAccount(LauncherState& state, const std::string& displayName);
void updateSelectedVersion(LauncherState& state, const GameVersion& updatedVersion);

VersionManifest loadVersionManifest(const std::filesystem::path& manifestPath);
void saveVersionManifest(const VersionManifest& manifest, const std::filesystem::path& manifestPath);
std::vector<GameVersion> discoverInstalledVersions(const std::filesystem::path& versionRoot);
void mergeDiscoveredVersions(LauncherState& state, const std::vector<GameVersion>& discoveredVersions);
GameVersion installLocalBuildVersion(const LocalBuildInstallRequest& request);

std::vector<std::string> gameArguments(
    const AccountProfile& account,
    const GameVersion& version,
    const LaunchOptions& options);
std::string launchGame(const AccountProfile& account, const GameVersion& version, const LaunchOptions& options);
std::string launchServer(const GameVersion& version, const std::string& port);

}  // namespace terralite::launcher
