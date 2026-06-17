# Nexus Log Uploader

[Nexus](https://github.com/RaidcoreGG/Nexus) addon to automatically parse new arcdps logs locally and optionally upload them to [dps.report](https://dps.report) or [Wingman](https://gw2wingman.nevermindcreations.de).

## Screenshots

![context-menu](screenshots/context-menu.png)
![display-options](screenshots/display-options.png)
![parser-options](screenshots/parser-options.png)
![dps-report-options](screenshots/dps-report-options.png)
![wingman-options](screenshots/wingman-options.png)

## Building

**Prerequisites:** Visual Studio 2022 with C++ workload, [vcpkg](https://github.com/microsoft/vcpkg) installed and integrated (`vcpkg integrate install`).

```powershell
git clone --recurse-submodules https://github.com/eioz/nexus-log-uploader.git
cd nexus-log-uploader

.\build.ps1                # Release x64
.\build.ps1 -Config Debug  # Debug x64
.\build.ps1 -Rebuild       # Clean + rebuild
.\build.ps1 -Clean         # Remove build artifacts
.\build.ps1 -Mock          # Build and launch in nexus-mock
```

Output: `build\x64\Release\log_uploader.dll`

### macOS native dev mock

The native macOS workflow lives in the `nexus-mock` submodule so this addon stays Windows-DLL first.
It builds a local mock host for UI and core workflow development, but it does not produce `log_uploader.dll`; use the Windows build or GitHub Actions artifact for the Nexus addon DLL.

```sh
brew install cmake ninja
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh -disableMetrics

git submodule update --init --recursive
cd nexus-mock
cmake --preset mac-mock-debug
cmake --build --preset mac-mock-debug
../build/mac-mock-debug/nexus_log_uploader_mac_mock
```

The mock uses deterministic local parser and upload results. It never uploads to dps.report or Wingman.
