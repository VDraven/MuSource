# Mu Client + Launcher

This repository contains the Windows client, launcher, runtime assets, and everything required to build them.

## Screenshots

![MuTwo gameplay screenshot 1](screenshots/1.png)

![MuTwo gameplay screenshot 2](screenshots/2.png)

![MuTwo gameplay screenshot 3](screenshots/3.png)

![MuTwo gameplay screenshot 4](screenshots/4.png)

## Client features

- Native C++20 client for Windows x86 and x64.
- Runs multiple game sessions in one client workspace.
- Supports single-session and four-session grid views.
- Uses configurable workers for parallel session workloads.
- Renders through SDL3 GPU with Direct3D 12 or Vulkan.
- Supports VSync and a separate FPS limit.
- Uses scalable RmlUi panels and reusable controls.
- Packs named UI sprites into compact shared atlases.
- Separates application, session, network, gameplay, UI, audio, and rendering state.
- Supports selectable UI languages, fonts, and interface scales.
- Supports automatic server, channel, and character selection (pair with launcher).
- ...

## Launcher features

- Ships as a self-contained Avalonia desktop application.
- Shows server availability, latency, channels, and channel load.
- Refreshes server status automatically every 20 seconds.
- Creates, edits, starts, and deletes session presets.
- Supports multiple account slots and controlled launch delays.
- Encrypts saved passwords for the current Windows user.
- Supports automatic server, channel, and character selection.
- Records configurable shortcuts for session control.
- Edits renderer, VSync, FPS limit, and worker count.
- Edits resolution, window mode, language, font, UI scale, and audio volume.
- Includes direct `config.ini` editing for advanced users.
- ...

## Quick start

### Use an existing build

1. Download and Extract V1.0 MuClient.7z in Releases
2. Run `MuLauncher.exe`.
3. Configure the server address and client options.
4. Start the client. The launcher uses `Main.exe` from the same folder.

Example: `bin/x64/Release/MuLauncher.exe`

### Build from source

Requirements:

- Windows 10 or Windows 11
- Visual Studio 2022 or 2026 with **Desktop development with C++**
- CMake 3.25 or newer
- .NET SDK 10
- Ninja only when using the Ninja generator

Open PowerShell in the repository root and run:

```powershell
.\Build.ps1
```

The default build is x64 Release with Visual Studio. It builds both the client and launcher.

Useful examples:

```powershell
# x86 Release
.\Build.ps1 -Architecture x86

# x64 Debug
.\Build.ps1 -Configuration Debug

# Client only with Ninja
.\Build.ps1 -Component Client -Generator Ninja

# Launcher only
.\Build.ps1 -Component Launcher
```

Final ready-to-play files are placed in:

```text
bin/<architecture>/<configuration>/
```

All intermediate files, caches, downloaded packages, and debug symbols stay under `build/`.

## Repository map

```text
MuSource/
├─ src/
│  ├─ client/           Native C++ client
│  ├─ client-library/   Managed networking library
│  ├─ launcher/         Avalonia launcher
│  └─ generated/        Generated C++ and shader source required by release builds
├─ assets/
│  ├─ runtime/          Files copied beside Main.exe
│  ├─ source/           Editable UI source assets
│  └─ localization/     Localization source files
├─ vendor/
│  ├─ third-party/      Required third-party source
│  └─ prebuilt/         Required headers and static libraries
├─ cmake/               CMake modules and toolchains
├─ build/               Intermediates, caches, packages, and symbols
└─ bin/                 Final ready-to-play builds
```

## Direct CMake use

Available configure presets:

- `vs2022-x64`
- `vs2022-x86`
- `vs2026-x64`
- `vs2026-x86`
- `ninja-x64`
- `ninja-x86`

Example:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-release
```

Build parallelism is limited to 8 jobs.

## Package scope

This is a focused release-source package. Project tests, development tools, internal documentation, Git history, IDE caches, logs, and old build folders are intentionally excluded. Generated release source is included so the client builds without the removed project tools.
