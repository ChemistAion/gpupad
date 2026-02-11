# Copilot instructions for GPUpad

## Build (CMake)
For this workstatin env, this will work:
Remove-Item -Recurse -Force .build
cmake -B .build -G "Visual Studio 18 2026" -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_PREFIX_PATH="X:/.tools/qt6/6.10.2/msvc2022_64"
## High-level architecture
- Qt Widgets app entry in `src/main.cpp`; main window/docking UI lives under `src/windows` and editor views under `src/editors`.
- Session data is a tree of `Item` types (`src/session/Item.h`) managed by `SessionModel/SessionModelCore`, with undo/redo and per-item IDs.
- Rendering is split by backend: `RenderSessionBase` selects OpenGL or Vulkan implementations (`src/render/opengl`, `src/render/vulkan`) and runs evaluations against a session copy.
- JavaScript scripting is handled by `ScriptEngine`/`ScriptSession` (`src/scripting`) which loads `:/scripting/ScriptEngine.js` and exposes `app`/`console` globals.

## Key conventions
- Use `Singletons` for app-wide services (settings, file cache, session model, renderers, editors).
- Session item properties are often stored as strings/expressions and evaluated during render/script passes; preserve this pattern when adding new item fields.
- UI and scripting assets are Qt resources; add files to `src/resources.qrc` and reference via `:/` paths.
- Rendering backend decisions use `RenderAPI` and `RenderSessionBase::create`; keep OpenGL/Vulkan-specific logic in their respective subfolders.
- Custom actions are built from `extra/actions/*` CMake subprojects with no shared library prefix.
