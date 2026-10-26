[CmdletBinding()]
param(
    [ValidateSet('All', 'Client', 'Launcher')]
    [string]$Component = 'All',

    [ValidateSet('x86', 'x64')]
    [string]$Architecture = 'x64',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('Ninja', 'VisualStudio')]
    [string]$Generator = 'VisualStudio'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = $PSScriptRoot

function Get-VisualStudioPresetPrefix {
    # Prefer the newest installed Visual Studio that has the C++ workload.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        return 'vs2022'
    }

    $hasCpp = 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
    $vs2026 = & $vswhere -latest -products * -version '[18.0,19.0)' -requires $hasCpp -property installationPath
    if ($LASTEXITCODE -eq 0 -and $vs2026) {
        return 'vs2026'
    }

    return 'vs2022'
}

$dotnet = Get-Command dotnet -ErrorAction Stop
$sdkList = & $dotnet.Source --list-sdks
if ($LASTEXITCODE -ne 0 -or -not ($sdkList -match '(?m)^10\.')) {
    throw '.NET SDK 10 is required to build the client and launcher.'
}

$env:NUGET_PACKAGES = Join-Path $root 'build\packages\nuget'
$env:DOTNET_CLI_HOME = Join-Path $root 'build\dotnet-home'
$configName = $Configuration.ToLowerInvariant()

if ($Component -in @('All', 'Client')) {
    $cmake = Get-Command cmake -ErrorAction Stop
    $presetPrefix = if ($Generator -eq 'Ninja') { 'ninja' } else { Get-VisualStudioPresetPrefix }
    $configurePreset = "$presetPrefix-$Architecture"
    $buildPreset = "$configurePreset-$configName"

    & $cmake.Source --preset $configurePreset
    if ($LASTEXITCODE -ne 0) { throw "Client configure failed: $configurePreset" }

    & $cmake.Source --build --preset $buildPreset
    if ($LASTEXITCODE -ne 0) { throw "Client build failed: $buildPreset" }
}

if ($Component -in @('All', 'Launcher')) {
    $launcherOutput = Join-Path $root "bin\$Architecture\$Configuration"
    & $dotnet.Source publish (Join-Path $root 'src\launcher\Launcher.csproj') `
        --configuration $Configuration `
        --runtime "win-$Architecture" `
        --output $launcherOutput `
        --nologo
    if ($LASTEXITCODE -ne 0) { throw 'Launcher build failed.' }
}

$runtimeOutput = Join-Path $root "bin\$Architecture\$Configuration"
$symbolFiles = Get-ChildItem -LiteralPath $runtimeOutput -File -Filter '*.pdb' -ErrorAction SilentlyContinue
if ($symbolFiles) {
    $symbolOutput = Join-Path $root "build\symbols\$Architecture\$Configuration"
    New-Item -ItemType Directory -Path $symbolOutput -Force | Out-Null
    $symbolFiles | Move-Item -Destination $symbolOutput -Force
}

Write-Host "Ready-to-play output: $runtimeOutput"
