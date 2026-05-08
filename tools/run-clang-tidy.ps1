param(
    [string[]]$Path = @("src", "include"),
    [string]$BuildDir = "cmake-build-debug",
    [string]$ClangTidy = ""
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

function Resolve-ProjectPath([string]$InputPath) {
    if ([System.IO.Path]::IsPathRooted($InputPath)) {
        return $InputPath
    }

    return Join-Path $ProjectRoot $InputPath
}

if (-not $ClangTidy) {
    $cmd = Get-Command clang-tidy -ErrorAction SilentlyContinue
    if ($cmd) {
        $ClangTidy = $cmd.Source
    }
}

if (-not $ClangTidy) {
    $candidatePaths = @(
        "/opt/homebrew/opt/llvm/bin/clang-tidy",
        "/usr/local/opt/llvm/bin/clang-tidy",
        "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
    )

    foreach ($candidatePath in $candidatePaths) {
        if (Test-Path $candidatePath) {
            $ClangTidy = $candidatePath
            break
        }
    }
}

if (-not $ClangTidy -or -not (Test-Path $ClangTidy)) {
    throw "clang-tidy was not found. Install LLVM (`brew install llvm` on macOS) or pass -ClangTidy <path-to-clang-tidy>."
}

$extraArgs = @()
if ($IsMacOS) {
    $sdkPath = (& xcrun --show-sdk-path 2>$null)
    if ($LASTEXITCODE -eq 0 -and $sdkPath -and (Test-Path $sdkPath)) {
        $extraArgs += @("--extra-arg-before=-isysroot", "--extra-arg-before=$sdkPath")

        $libcxxPath = Join-Path $sdkPath "usr/include/c++/v1"
        if (Test-Path $libcxxPath) {
            $extraArgs += @("--extra-arg-before=-isystem", "--extra-arg-before=$libcxxPath")
        }
    }
}

$ResolvedBuildDir = Resolve-ProjectPath $BuildDir
$compileCommands = Join-Path $ResolvedBuildDir "compile_commands.json"
if (-not (Test-Path $compileCommands)) {
    throw "Missing $compileCommands. Reconfigure CMake first so CMAKE_EXPORT_COMPILE_COMMANDS is generated."
}

$files = foreach ($entry in $Path) {
    $resolvedEntry = Resolve-ProjectPath $entry
    if (Test-Path $resolvedEntry -PathType Leaf) {
        Get-Item $resolvedEntry
    } elseif (Test-Path $resolvedEntry -PathType Container) {
        Get-ChildItem $resolvedEntry -Recurse -File -Include *.cpp,*.cxx,*.cc
    } else {
        throw "Path not found: $resolvedEntry"
    }
}

foreach ($file in $files) {
    Write-Host "clang-tidy $($file.FullName)"
    & $ClangTidy $file.FullName -p $ResolvedBuildDir @extraArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
