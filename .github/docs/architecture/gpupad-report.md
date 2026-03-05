# GPUpad — Comprehensive Codebase Architecture Report

> **Generated**: 2026-02-27  
> **Scope**: `src/` directory only (excludes `extra/` samples/actions)  
> **Language**: C++20, Qt 6, GLSL/HLSL/Slang  
> **Build**: CMake 3.21+, MSVC / GCC / Clang

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Build System & Dependencies](#2-build-system--dependencies)
3. [Application Bootstrap & Lifecycle](#3-application-bootstrap--lifecycle)
4. [Singleton Registry](#4-singleton-registry)
5. [Session Data Model](#5-session-data-model)
6. [Rendering Pipeline](#6-rendering-pipeline)
7. [Backend-Specific Implementations](#7-backend-specific-implementations)
8. [Shader Compilation & Reflection](#8-shader-compilation--reflection)
9. [Editor Subsystem](#9-editor-subsystem)
10. [Scripting Engine](#10-scripting-engine)
11. [Synchronization Logic (Evaluation Loop)](#11-synchronization-logic-evaluation-loop)
12. [File Cache & I/O](#12-file-cache--io)
13. [UI Windows & Widgets](#13-ui-windows--widgets)
14. [Theming & Styling](#14-theming--styling)
15. [Input & Video](#15-input--video)
16. [Cross-Subsystem Data Flow](#16-cross-subsystem-data-flow)
17. [Threading Model](#17-threading-model)
18. [Key Design Patterns](#18-key-design-patterns)
19. [File/Module Index](#19-filemodule-index)

---

## 1. Executive Summary

**GPUpad** is a desktop IDE for authoring, editing, and live-previewing GPU shader programs (GLSL, HLSL, Slang) and their associated render pipelines. It provides:

- A **session tree** describing a complete render pipeline (buffers, textures, programs, shaders, bindings, draw/compute/raytrace calls, render targets, acceleration structures).
- **Three GPU backends**: OpenGL 4.5, Vulkan (via KDGpu), and Direct3D 12 (Windows-only).
- **Four editor types**: source code (syntax-highlighted), binary/hex, texture viewer (with GL preview), and QML views.
- A **JavaScript scripting engine** (Qt QML/QJSEngine) that can drive expression evaluation, per-frame animation, and custom actions.
- **Live evaluation** with automatic, steady (continuous), and manual modes, including time queries and shader printf.

The application is built on **Qt 6** (Widgets, OpenGL, Qml, Quick, Multimedia), uses C++20 throughout, and targets Windows, Linux, and macOS.

---

## 2. Build System & Dependencies

### CMakeLists.txt (root)

| Feature | Details |
|---------|---------|
| Standard | C++20, CMake 3.21+ |
| Unity Build | Optional (`ENABLE_UNITY_BUILD`) |
| Qt Modules | Core, Widgets, OpenGLWidgets, OpenGL, Qml; optional Quick, QuickWidgets, Multimedia |
| GPU Libraries | KDGpu (Vulkan abstraction, built from `libs/KDGpu`), Vulkan SDK, VulkanMemoryAllocator |
| Shader Toolchain | glslang (GLSL→SPIRV), SPIRV-Cross (SPIRV→GLSL/HLSL cross-compilation), SPIRV-Tools (optimization), spirv-reflect (reflection) |
| Optional | OpenImageIO (extended image formats), Slang (shader language), DXC (DirectX Shader Compiler, Win32) |
| Bundled libs | SingleApplication (single-instance enforcement), spirv-reflect, stb, d3d12 helpers, dllreflect |
| Version | Git-tag based; `version.h.in` → `_version.h` |
| Packaging | CPack: WIX (Windows), TGZ (Linux), DragNDrop (macOS) |

### Compile Definitions (Qt hardening)

```
QT_NO_CAST_TO_ASCII, QT_NO_URL_CAST_FROM_STRING, QT_NO_CAST_FROM_BYTEARRAY,
QT_NO_SIGNALS_SLOTS_KEYWORDS, QT_USE_QSTRINGBUILDER,
QT_NO_NARROWING_CONVERSIONS_IN_CONNECT, QT_NO_KEYWORDS,
QT_DISABLE_DEPRECATED_BEFORE=0x060500, QT_NO_FOREACH
```

These enforce strict Qt6 coding practices — no implicit casts, no deprecated APIs, signal/slot macros replaced with `Q_SIGNALS`/`Q_SLOTS`.

---

## 3. Application Bootstrap & Lifecycle

**Entry**: `src/main.cpp`

### Startup Sequence

1. **Process priority**: Raised to `HIGH_PRIORITY_CLASS` during startup (Windows).
2. **Single instance**: Uses `SingleApplication` to detect and forward file arguments to the running instance. Non-session files are forwarded; session files start a new instance.
3. **GL context**: Default `QSurfaceFormat` → OpenGL 4.5 Core Profile, debug context, VSync.
4. **QApplication**: Created with `AA_ShareOpenGLContexts` (required for texture preview sharing between render thread and editor GL widgets).
5. **Locale**: Forced to `QLocale::c()` for deterministic float formatting.
6. **Style**: Custom `Style` class applied globally.
7. **MainWindow**: Constructed, shown. Owns `Singletons` (see below).
8. **CLI args**: Remaining arguments opened as files.
9. **Event loop**: `app.exec()`.

### Windows specifics

- `NvOptimusEnablement` / `AmdPowerXpressRequestHighPerformance` exported to prefer dedicated GPU.
- `SetForegroundWindowInternal` uses ALT-key trick to reliably raise the window.
- `QSettings::IniFormat` used instead of registry.
- `MESA_GL_VERSION_OVERRIDE` on Linux for wider Mesa compatibility.

---

## 4. Singleton Registry

**Files**: `Singletons.h`, `Singletons.cpp`

Central service locator holding all application-wide objects. Constructed on the stack inside `MainWindow` and exposed via static accessors.

| Singleton | Type | Purpose |
|-----------|------|---------|
| `settings()` | `Settings` | Persistent settings (font, tab size, themes, etc.) |
| `fileCache()` | `FileCache` | Caches source text, binary data, textures; watches filesystem |
| `fileDialog()` | `FileDialog` | File open/save dialog helpers; untitled file naming |
| `editorManager()` | `EditorManager` | Manages all open editor dock widgets |
| `sessionModel()` | `SessionModel` | The session tree data model (`QAbstractItemModel`) |
| `synchronizeLogic()` | `SynchronizeLogic` | Orchestrates evaluation loop, bridges model↔render↔editors |
| `videoManager()` | `VideoManager` | Video file playback (optional Qt Multimedia) |
| `inputState()` | `InputState` | Mouse/keyboard state for shader input |
| `customActions()` | `CustomActions` | Discovers and runs user-defined JS action plugins |
| `defaultScriptEngine()` | `ScriptEngine` | Default JS engine for expression evaluation |
| `glRenderer()` | `GLRenderer` | OpenGL 4.5 renderer (lazy-init) |
| `vkRenderer()` | `VKRenderer` | Vulkan renderer via KDGpu (lazy-init) |
| `d3dRenderer()` | `D3DRenderer` | Direct3D 12 renderer (lazy-init, Windows-only) |
| `sessionRenderer()` | `RendererPtr` | Returns the renderer matching the session's chosen API |

**Thread safety**: Most singletons assert `onMainThread()`. `FileCache` uses a `QMutex` for thread-safe access to its data maps. Renderers are lazily created.

---

## 5. Session Data Model

### Item Hierarchy

**Files**: `session/Item.h`, `session/ItemEnums.h`, `session/SessionModelCore.h`, `session/SessionModel.h`, `session/SessionModelPriv.h`

The session is a **tree of typed items**. The hierarchy models a complete render pipeline description:

```
Root
 └─ Session (renderer, shader language, compiler settings, preamble)
     ├─ Group (iterations, inline scope, dynamic)
     │   ├─ Buffer → Block → Field (typed data layout)
     │   ├─ Texture (target, format, dimensions)
     │   ├─ Program → Shader (type, entry point, preamble)
     │   ├─ Binding (uniform, sampler, image, buffer, bufferBlock, textureBuffer, subroutine)
     │   ├─ Stream → Attribute (vertex attribute mapping)
     │   ├─ Target → Attachment (render target with blend/depth/stencil state)
     │   ├─ Call (draw, compute, trace, clear, copy, swap)
     │   ├─ Script (JS, execute-on policy)
     │   └─ AccelerationStructure → Instance → Geometry (raytracing BVH)
     └─ ...
```

### Item Struct Design

Each item type is a plain C++ struct deriving from `Item`:

```cpp
struct Item {
    ItemId id{};           // unique int ID
    Type type{};           // enum discriminator
    Item *parent{};        // parent pointer
    QList<Item *> items;   // children (raw pointers, owned by model)
    QString name;
};
```

Specialized items add typed fields: `FileItem` (has `fileName`), `ScopeItem` (hierarchy scope), `Session`, `Group`, `Buffer`, `Block`, `Field`, `Texture`, `Program`, `Shader`, `Binding`, `Stream`, `Attribute`, `Target`, `Attachment`, `Call`, `Script`, `AccelerationStructure`, `Instance`, `Geometry`.

### Type-Safe Casting

Template function `castItem<T>(const Item&)` performs discriminated-union-style casting using `getItemType<T>()`. This avoids `dynamic_cast` entirely (RTTI is disabled in release builds via `/GR-` or `-fno-rtti`).

### SessionModelCore (QAbstractItemModel)

The core model wraps the item tree into Qt's model/view framework:

- **ColumnType enum**: ~140 columns mapping 1:1 to every field of every item struct (e.g., `TextureWidth`, `BindingMinFilter`, `CallWorkGroupsX`).
- **data()/setData()**: Get/set any item field via `QModelIndex` + column, with full undo support.
- **Undo**: All mutations go through `undoableAssignment` / `undoableInsertItem` / `undoableRemoveItem`, backed by a `QUndoStack`.
- **SessionModelPriv.h** (`ADD_EACH_COLUMN_TYPE` macro): X-macro that generates the column↔struct-field mapping for all ~140 columns, used for data(), setData(), serialization, etc.

### SessionModel (final subclass)

Extends `SessionModelCore` with:

- **Drag & drop** (JSON-based MIME data, URL import).
- **JSON serialization**: `getJson()` / `dropJson()` / `save()` / `load()` for `.gpjs` session files.
- **Active items** tracking (visual highlighting of items used by current render session).
- **Scoped iteration**: `forEachItemScoped()` traverses items visible from a given scope position (respects `Group::inlineScope`).

### Item Enums

`ItemEnums.h` and `ItemEnums2.h` use `Q_NAMESPACE` / `Q_ENUM_NS` for all GPU-pipeline enums. Values are intentionally set to their OpenGL constants (e.g., `Triangles = GL_TRIANGLES`, `Float = GL_FLOAT`) for zero-cost translation in the GL backend.

---

## 6. Rendering Pipeline

### Architecture Overview

```
SessionModel (data) ──► SynchronizeLogic (orchestrator)
                              │
                              ├── RenderSessionBase::prepare()   [main thread]
                              ├── RenderSessionBase::configure()  [render thread]
                              ├── RenderSessionBase::configured() [main thread]
                              ├── RenderSessionBase::render()     [render thread]
                              └── RenderSessionBase::finish()     [main thread]
```

### Renderer (Abstract Base)

**File**: `render/Renderer.h`

```cpp
class Renderer {
    RenderAPI mApi;   // OpenGL | Vulkan | Direct3D
    virtual QThread *renderThread() = 0;
    virtual void render(RenderTask *task) = 0;
    virtual void release(RenderTask *task) = 0;
};
```

Each renderer owns a dedicated `QThread` and a task queue. Tasks are submitted from the main thread, configured and rendered on the render thread, then finished on the main thread.

### RenderTask

**File**: `render/RenderTask.h`

Base class for any GPU work unit. Lifecycle:

1. `prepare()` — main thread, snapshot session state.
2. `configure()` — render thread, create/update GPU resources.
3. `configured()` — main thread.
4. `render()` — render thread, issue GPU commands.
5. `finish()` — main thread, read back results, update editors.
6. `release()` — render thread, destroy GPU resources.

Provides `dispatchToRenderThread()` helper using `QMetaObject::invokeMethod` with `Qt::BlockingQueuedConnection`.

### RenderSessionBase

**File**: `render/RenderSessionBase.h`, `render/RenderSessionBase_CommandQueue.h`

The heart of rendering. Inherits both `RenderTask` and `IScriptRenderSession`. Contains:

- **SessionModel copy** (`mSessionModelCopy`): A snapshot of the session model taken on the main thread, used safely on the render thread.
- **ScriptSession**: Manages a JS engine with per-frame globals (time, frame, mouse, keyboard, viewport size).
- **Command queue** building: Template method `buildCommandQueue<RenderSession, CommandQueue>()` traverses the session tree and generates a list of `Command = std::function<void(BindingState&)>`.
- **Binding scope stack**: `BindingState = QStack<Bindings>` — each `ScopeItem` pushes/pops a scope; bindings are merged at call execution time.
- **Group iteration**: Groups with `iterations > 1` are handled via command-queue index rewind (jump back to loop start).
- **Resource reuse**: `reuseUnmodifiedItems()` compares new vs. previous command queue; unchanged buffers/textures/programs are moved rather than recreated.
- **Time queries**: Per-call GPU timing with `beginTimeQuery()` / `resetTimeQueries()`.
- **Printf**: Shader printf support via buffer-based printf instrumentation.
- **Download pipeline**: `beginDownloadModifiedResources()` → `finishCommandQueue()` reads back modified textures/buffers to update editors.

### Bindings

Resolved bindings are typed structs:

| Type | Contents |
|------|----------|
| `UniformBinding` | name, type, transpose flag, script-evaluated values |
| `SamplerBinding` | texture ptr, filter/wrap/border/comparison settings |
| `ImageBinding` | texture ptr, level, layer, format |
| `BufferBinding` | buffer ptr, block ID, offset/rowCount/stride |
| `SubroutineBinding` | name, subroutine name |

---

## 7. Backend-Specific Implementations

All three backends follow an identical pattern: `XXRenderer`, `XXRenderSession`, `XXProgram`, `XXShader`, `XXBuffer`, `XXTexture`, `XXTarget`, `XXStream`, `XXCall`, `XXPipeline`, `XXPrintf`, `XXShareSync`, plus backend-specific context/enums.

### OpenGL (`render/opengl/`)

| File | Role |
|------|------|
| `GLRenderer` | QThread + Worker; offscreen `QOpenGLContext`; task queue with configure→render→release signals |
| `GLRenderSession` | Owns `CommandQueue` (map of `GLProgram`, `GLTexture`, `GLBuffer`, `GLTarget`, `GLStream`, `GLCall`); VAO; timer queries |
| `GLProgram` | Compiles/links GLSL shaders; reflection via `GLProgram_Reflection.cpp` |
| `GLShader` | GLSL shader compilation (driver or glslang→SPIRV→SPIRV-Cross→GLSL) |
| `GLBuffer` | `glNamedBufferStorage`-based buffers with readback |
| `GLTexture` | Texture creation, upload (KTX), mipmap generation, readback, shared-memory export |
| `GLTarget` | FBO management with attachment validation |
| `GLStream` | Vertex attribute binding |
| `GLCall` | Dispatches `glDrawArrays`/`glDrawElements`/`glDispatchCompute`/etc.; applies all pipeline state |
| `GLPrintf` | Printf buffer management (SSBO-based) |
| `GLComputeRange` | Compute shader for texture range calculation (histogram support) |
| `GLShareSync` | OpenGL↔Vulkan/D3D shared texture sync via semaphores |

### Vulkan (`render/vulkan/`)

| File | Role |
|------|------|
| `VKRenderer` | KDGpu `Device` + `Queue`; KTX Vulkan device info; task queue |
| `VKRenderSession` | Vulkan command queue; timestamp queries via `KDGpu::TimestampQueryRecorder` |
| `VKPipeline` | Graphics/compute/ray-tracing pipeline creation with descriptor set layout |
| `VKProgram` | SPIRV module management; reflection |
| `VKShader` | SPIRV compilation (glslang or DXC) |
| `VKBuffer` | VMA-allocated buffers with staging |
| `VKTexture` | KTX2 upload; mipmap blit; shared-memory handle export |
| `VKTarget` | Render pass / framebuffer (dynamic rendering) |
| `VKStream` | Vertex input state |
| `VKCall` | Command buffer recording for all call types |
| `VKPrintf` | Printf SSBO management |
| `VKAccelerationStructure` | BLAS/TLAS construction for ray tracing |
| `VKShareSync` | Vulkan↔OpenGL texture sharing via semaphores/external memory |
| `KDGpuEnums` | Mapping between GPUpad's GL-based enums and KDGpu/Vulkan enums |

### Direct3D 12 (`render/direct3d/`, Windows-only)

| File | Role |
|------|------|
| `D3DRenderer` | D3D12 device, command queue; on non-Windows: stub that emits `Direct3DNotAvailable` |
| `D3DRenderSession` | Command allocator, fence, timestamp query heap |
| `D3DPipeline` | PSO creation (graphics/compute), root signature generation |
| `D3DProgram` | DXIL bytecode management |
| `D3DShader` | HLSL compilation via D3DCompiler or DXC; reflection via `D3DShader_Reflection` |
| `D3DBuffer` | Committed resources with upload/readback heaps |
| `D3DTexture` | Texture creation, upload, mipmap generation |
| `D3DTarget` | RTV/DSV descriptor heap management |
| `D3DStream` | Input layout description |
| `D3DCall` | Command list recording |
| `D3DPrintf` | Printf UAV buffer |
| `D3DAccelerationStructure` | DXR BLAS/TLAS |
| `D3DShareSync` | D3D12↔OpenGL fence/texture sharing |
| `D3DEnums` | GL-enum → DXGI/D3D12 enum translation |
| `d3dx12.h` | Microsoft D3D12 helper header |

### Resource Sharing (ShareSync)

The `ShareSync` interface enables cross-API texture sharing for real-time preview:

- **GL→GL**: Trivial (shared contexts).
- **VK→GL**: External memory (VK exports handle → GL imports via `GL_EXT_memory_object`).
- **D3D→GL**: D3D11-interop texture → GL import.

Each backend's `ShareSync` implements `beginUsage()`/`endUsage()` for GL-side synchronization.

---

## 8. Shader Compilation & Reflection

### ShaderBase

**File**: `render/ShaderBase.h`

Abstract base for shader compilation across backends. Aggregates sources from multiple `Shader` items (with preambles, include paths). Methods:

- `compileSpirv()` → SPIRV binary via `ShaderCompiler` namespace.
- `preprocess()` → preprocessed source text.
- `generateGLSL()` / `generateHLSL()` → cross-compiled source via SPIRV-Cross.
- `disassemble()` → SPIRV disassembly.
- `getReflection()` → `Reflection` object.
- `getPatchedSources*()` → sources with printf instrumentation, preprocessor defines.

### ShaderCompiler Namespace

**Files**: `render/ShaderCompiler.h`, `ShaderCompiler.cpp`, `ShaderCompiler_glslang.cpp`, `ShaderCompiler_Microsoft.cpp`

| Function | Purpose |
|----------|---------|
| `compileSpirv()` | Compile shader sources to SPIRV using the session's chosen compiler (Driver/glslang/D3DCompiler/DXC/Slang) |
| `preprocess()` | GLSL/HLSL preprocessing with custom `#include` resolution |
| `disassemble()` | SPIRV → textual disassembly |
| `generateGLSL()` | SPIRV → GLSL via SPIRV-Cross |
| `generateHLSL()` | SPIRV → HLSL via SPIRV-Cross |
| `stripReflection()` | Remove reflection info from SPIRV for smaller modules |
| `generateAST()` | Produce AST dump via glslang |

### Compiler Backends

- **glslang** (`ShaderCompiler_glslang.cpp`): GLSL/HLSL → SPIRV. Supports `#include` via custom `TShader::Includer`. Configurable via session settings (auto-map bindings, auto-map locations, Vulkan relaxed rules, SPIRV version).
- **Microsoft** (`ShaderCompiler_Microsoft.cpp`, Windows-only): D3DCompiler (fxc) and DXC for HLSL → DXIL/SPIRV.
- **Slang** (`render/Slang.h`/`.cpp`): Optional Slang language support.

### Reflection

**Files**: `render/Reflection.h`, `Reflection.cpp`, `Reflection_Builder.cpp`, `Reflection_JSON.cpp`

Wraps `spirv-reflect` (`SpvReflectShaderModule`) with:

- Two construction paths: from raw SPIRV binary, or from a manually-built `Builder` struct (for backends like D3D that have their own reflection).
- Accessors: `descriptorBindings()`, `pushConstantBlocks()`, `inputVariables()`.
- JSON serialization for the output window.
- Helper functions: `getBufferMemberDataType()`, `getBufferMemberColumnCount()`, `isGlobalUniformBlockName()`, etc.

### Printf Support

**File**: `render/PrintfBase.h`

Shader-side `printf()` is implemented by:

1. Source patching: `patchSource()` replaces `printf(...)` calls with SSBO writes.
2. Each backend provides a printf SSBO (`_printfBuffer`).
3. After rendering, the buffer is read back and parsed: format strings are matched to written argument values.
4. Results are emitted as `ScriptMessage`-type messages.

---

## 9. Editor Subsystem

### EditorManager

**File**: `editors/EditorManager.h`

Central manager for all editor dock widgets. Inherits `DockWindow` (a `QMainWindow` used as a dock container within the main splitter).

Maintains typed lists: `mSourceEditors`, `mBinaryEditors`, `mTextureEditors`, `mQmlViews`.

Features:
- **Editor-per-file**: Only one editor per unique filename.
- **Navigation stack**: Back/forward navigation between editor positions.
- **Tabify groups**: Editors are grouped into tabs by type (source=0, binary=0, texture=1, qml=3).
- **Auto-raise**: Newly opened editors are automatically raised.
- **EditActions bridge**: `connectEditActions()` relays undo/redo/cut/copy/paste to the active editor.
- **Toolbars**: Per-type toolbars (`SourceEditorToolBar`, `BinaryEditorToolBar`, `TextureEditorToolBar`) with visibility toggled based on active editor.

### IEditor Interface

```cpp
class IEditor {
    virtual QString fileName() const = 0;
    virtual void setFileName(QString) = 0;
    virtual bool load() = 0;
    virtual bool save() = 0;
    virtual void setModified() = 0;
    virtual int tabifyGroup() const = 0;
    virtual QList<QMetaObject::Connection> connectEditActions(const EditActions&) = 0;
};
```

### SourceEditor

**Files**: `editors/source/SourceEditor.cpp`, plus supporting classes.

Full-featured code editor built on `QPlainTextEdit`:

| Feature | Implementation |
|---------|----------------|
| Line numbers | Custom `LineNumberArea` widget overlay |
| Syntax highlighting | `SyntaxHighlighter` → dispatches to `SyntaxGLSL`, `SyntaxHLSL`, `SyntaxSlang`, `SyntaxJavaScript`, `SyntaxGeneric` |
| Auto-completion | `Completer` class with language-aware keyword lists |
| Find & Replace | `FindReplaceBar` with regex support |
| Multi-cursor | `MultiTextCursors` for simultaneous multi-point editing |
| Source type detection | `SourceType` enum; auto-detected from file extension + content |

### BinaryEditor

**File**: `editors/binary/BinaryEditor.h`

Hex/data editor built on `QTableView`:

- Two view modes: raw hex (`HexModel`) and structured data (`DataModel`) based on buffer's `Block`/`Field` layout.
- `SpinBoxDelegate` for typed data editing.
- `EditableRegion` to constrain edits to the current block.
- Supports `replace()` for external data updates (render readback).

### TextureEditor

**File**: `editors/texture/TextureEditor.h`

Texture viewer built on `QAbstractScrollArea`:

- **GLWidget**: OpenGL rendering of texture data with zoom/pan.
- **TextureItem**: Renders the actual texture quad.
- **TextureBackground**: Checkerboard pattern for alpha visualization.
- **Histogram**: GPU-computed histogram via `ComputeRange` (compute shader on GL backend).
- Supports preview textures from render sessions via shared memory handles.
- Zoom to fit, per-pixel info, mipmap level selection.

### QmlView

**File**: `editors/qml/QmlView.h`

Embeds a `QQuickWidget` for rendering QML content:
- Used for custom UI overlays and interactive visualizations.
- Dependency tracking for automatic reload.
- Optional custom `ScriptEngine` integration.

---

## 10. Scripting Engine

### ScriptEngine

**File**: `scripting/ScriptEngine.h`

Wraps `QJSEngine` to provide:

- **Expression evaluation**: `evaluateValue()`, `evaluateValues()`, `evaluateInt()`, `evaluateUInt()` — used everywhere for numeric properties that support expressions (e.g., texture width = `"viewportWidth"`, workgroup count = `"ceil(width/16)"`).
- **Global objects**: `setGlobal()` exposes named objects/values to scripts.
- **Interrupt timer**: Prevents infinite loops with configurable timeout.
- **Omit reference errors**: Default engine mode silently ignores undefined variables (used for expression evaluation where not all globals are available).

### Script Objects (scripting/objects/)

| Object | Exposed As | Purpose |
|--------|-----------|---------|
| `AppScriptObject` | `app` | Application info, frame index, time, viewport size |
| `ConsoleScriptObject` | `console` | `console.log()` → message window |
| `EditorScriptObject` | `editor` | Open/close/modify source files |
| `LibraryScriptObject` | `library` | Math utilities (gl-matrix, etc.) |
| `MouseScriptObject` | `mouse` | Mouse position, button states |
| `KeyboardScriptObject` | `keyboard` | Key states |
| `SessionScriptObject` | `session` | Read/write session model items from JS |

### ScriptSession

**File**: `scripting/ScriptSession.h`

Per-render-session scripting context:

1. **Main thread** (`update()`): Registers globals (time, frame, mouse, keyboard, viewport).
2. **Render thread** (`beginSessionUpdate()`): Engine available for expression evaluation during command queue execution.
3. **Main thread** (`endSessionUpdate()`): Collects messages, resets state.

### CustomActions

**File**: `scripting/CustomActions.h`

Plugin system for user-defined JavaScript actions:

- Scans `actions/` directories for JS files with manifest metadata.
- Each `CustomAction` gets its own `ScriptEngine` instance.
- Actions can manipulate the session model (add items, modify properties).
- Applied via `SessionScriptObject` bridge.

---

## 11. Synchronization Logic (Evaluation Loop)

**File**: `SynchronizeLogic.h`, `SynchronizeLogic.cpp`

The **conductor** of the application. Bridges the session model, render system, and editors.

### Evaluation Modes

| Mode | Behavior |
|------|----------|
| `Paused` | No rendering. Render session can be released. |
| `Automatic` | Re-renders when session items change (debounced 10ms). |
| `Steady` | Continuous rendering at 1ms interval (animation mode). |

### Evaluation Types

| Type | Trigger |
|------|---------|
| `Reset` | First evaluation or manual reset. Frame=0, Time=0. |
| `Manual` | User-triggered single evaluation. |
| `Automatic` | Triggered by model change in Automatic mode. |
| `Steady` | Each frame in Steady mode. |

### Flow

1. **Model change** → `handleItemModified()` → `invalidateRenderSession()`.
2. **Timer fires** → `evaluate()`:
   - `fileCache.updateFromEditors()` — sync editor content to cache.
   - `initializeRenderSession()` — lazily create `RenderSessionBase` for current backend.
   - `mRenderSession->update()` — triggers the RenderTask lifecycle.
3. **After render** → `handleSessionRendered()`:
   - Updates editor content from downloaded textures/buffers.
   - Updates active-item highlighting in session tree.

### Source Processing

Separate from rendering, `ProcessSource` validates/compiles the current editor's source on the render thread. Supports:

- **Validation**: Compile shader, report errors.
- **Process types**: Preprocess, SPIRV disassembly, AST dump, GLSL/HLSL cross-compilation, JSON reflection.

### Editor Synchronization

- When a `Buffer` item is modified → its `BinaryEditor` is updated with new block/field layout.
- When a `Texture` item is modified → its `TextureEditor` is updated with new raw format.
- File renames propagate between session items and editors.
- `FileCache::fileChanged` → item re-evaluation.

---

## 12. File Cache & I/O

**File**: `FileCache.h`, `FileCache.cpp`

Thread-safe cache for all file content:

| Data Type | Cache Map | Thread-Safe |
|-----------|-----------|-------------|
| Source text | `QMap<QString, QString>` | QMutex |
| Textures | `QMap<(fileName, flipVertically), TextureData>` | QMutex |
| Binaries | `QMap<QString, QByteArray>` | QMutex |

Features:
- **Background loader** (`BackgroundLoader` on `mBackgroundLoaderThread`): Loads files off the main thread.
- **File system watcher** (`QFileSystemWatcher`): Detects external changes.
- **Editor integration**: `updateFromEditors()` syncs in-memory editor content to the cache without disk I/O.
- **Video player requests**: When a video file is referenced, emits `videoPlayerRequested` → `VideoManager`.

### TextureData

**File**: `TextureData.h`, `TextureData.cpp`

Wraps `ktxTexture1` (KTX library) for texture storage:

- Supports: KTX, OpenImageIO formats, QImage formats, PFM.
- Create/resize/convert/load/save.
- GL upload (`uploadGL` → `glTextureStorage`+`glTextureSubImage`).
- VK upload (`uploadVK` → KTX Vulkan upload).
- Metadata: target, format, dimensions, levels, layers, faces.
- Compression-aware.

### FileDialog

**File**: `FileDialog.h`, `FileDialog.cpp`

- Manages untitled file naming (`Untitled-1.glsl`, etc.).
- File type detection via extension.
- Session file identification (`.gpjs`).
- Native canonical path normalization.

---

## 13. UI Windows & Widgets

### MainWindow

**File**: `windows/MainWindow.h`, `MainWindow.ui`

The top-level `QMainWindow`:

- **Menu bar**: File, Edit, View, Evaluation, Session, Help.
- **Toolbar**: Evaluation controls, file actions.
- **Splitter layout**: Session tree (left) | Editor area (center).
- **Docks**: Message window, output window, file browser, session tree, properties editor.
- **State persistence**: Window geometry, splitter sizes, dock positions saved to QSettings.
- **Recent files**: Separate lists for session files and source files.
- **Themes**: Dynamic theme menu populated from theme files.
- **Full-screen mode**: Dedicated toolbar.
- **Drag & drop**: Files can be dropped onto the window.

### MessageWindow

**File**: `windows/MessageWindow.h`

`QTableWidget` displaying compile errors, warnings, script messages, timing info. Messages are:

- Globally collected via `MessageList::insert()` (thread-safe).
- Periodically polled and deduplicated.
- Clickable → navigates to source file:line.

### OutputWindow

**File**: `windows/OutputWindow.h`

`QPlainTextEdit` showing processed source output (preprocessed, disassembled, cross-compiled, reflection JSON). Has a type selector combo box.

### FileBrowserWindow

**File**: `windows/FileBrowserWindow.h`

File system tree browser for the session's directory.

### SessionEditor

**File**: `session/SessionEditor.h`

`QTreeView` displaying the session model tree. Features:

- Context menu with add/delete actions for each item type.
- Drag & drop reordering.
- Undo/redo integration.
- Item activation → opens corresponding editor.

### PropertiesEditor

**File**: `session/properties/PropertiesEditor.h`

Dynamically swaps property panels based on selected item type. Each type has a `.ui` form:

| Item Type | Properties Form |
|-----------|----------------|
| Session | Renderer, shader language, compiler, preamble, include paths |
| Buffer | (name/file only) |
| Block | Offset, row count |
| Field | Data type, count, padding |
| Texture | Target, format, width/height/depth/layers/samples, flip |
| Shader | Type, entry point, preamble, include paths |
| Binding | Type-dependent: uniform/sampler/image/buffer settings |
| Target | Front face, cull mode, polygon mode, logic operation, blend constant |
| Attachment | Blend/depth/stencil state (full pipeline state per-attachment) |
| Call | Type, program/target/stream references, draw/compute parameters |
| ... | (AccelerationStructure, Instance, Geometry, etc.) |

### Custom Widgets (`widgets/`)

| Widget | Purpose |
|--------|---------|
| `ExpressionLineEdit` | Line edit that accepts numeric expressions (evaluated via ScriptEngine) |
| `ExpressionEditor` | Multi-line expression editor |
| `ExpressionMatrix` | Matrix editor (2x2 to 4x4) for uniform bindings |
| `ReferenceComboBox` | Combo box that lists session items by type for ID references |
| `DataComboBox` | Combo box with associated data values |
| `ColorPicker` | Color selection widget |
| `ColorMask` | 4-bit RGBA write mask toggle |

---

## 14. Theming & Styling

### Theme

**File**: `Theme.h`, `Theme.cpp`

- Themes are loaded from JSON files in `extra/themes/`.
- Each theme defines: name, author, dark/light flag, full `QPalette`, and syntax colors.
- Syntax colors: Function, Keyword, BuiltinFunction, BuiltinConstant, Number, Quotation, Preprocessor, Comment, WhiteSpace.
- Window theme and editor theme can be set independently.
- Themes are applied via `Settings::applyTheme()` → `QApplication::setPalette()`.

### Style

**File**: `Style.h`, `Style.cpp`

Custom `QProxyStyle` subclass for fine-tuning Qt widget appearance (tab bar metrics, etc.).

---

## 15. Input & Video

### InputState

**File**: `InputState.h`, `InputState.cpp`

Captures mouse/keyboard state for shader-accessible input:

- **Mouse**: Position, previous position, button states (Up/Down/Pressed/Released).
- **Keyboard**: Key states with the same 4-state model.
- **Editor size**: Viewport dimensions.
- State is double-buffered (next → current) via `update()`.
- Changes emit `mouseChanged()` / `keysChanged()` → trigger re-evaluation in Automatic mode.

### VideoManager / VideoPlayer

**Files**: `VideoManager.h`, `VideoPlayer.h`, `VideoPlayer.cpp`

- Optional (requires `Qt6Multimedia`).
- Videos are treated as animated textures.
- `VideoPlayer` decodes frames and pushes them to `FileCache` as texture data.
- `VideoManager` controls play/pause/rewind across all active video players.

---

## 16. Cross-Subsystem Data Flow

### Session Evaluation Data Flow

```
┌────────────────┐    model changes     ┌─────────────────────┐
│  SessionEditor  │ ──────────────────► │  SynchronizeLogic   │
│  (QTreeView)   │                      │  (orchestrator)     │
└────────────────┘                      └─────────┬───────────┘
                                                  │ evaluate()
                                                  ▼
┌────────────────┐   fileCache sync    ┌─────────────────────┐
│  EditorManager  │ ◄────────────────► │  RenderSessionBase  │
│  (source/bin/   │   update editors   │  (GL/VK/D3D impl)   │
│   tex editors)  │                    └─────────┬───────────┘
└────────────────┘                               │
                                                 │ render thread
                                                 ▼
                                       ┌─────────────────────┐
                                       │  CommandQueue        │
                                       │  (programs, buffers, │
                                       │   textures, targets, │
                                       │   calls, bindings)   │
                                       └─────────────────────┘
```

### Expression Evaluation Flow

```
UI Property Field (ExpressionLineEdit)
    │
    ▼ "viewportWidth * 2"
ScriptEngine::evaluateInt()
    │
    ▼ QJSEngine evaluation
numeric result → used for texture width / workgroup count / etc.
```

### Shader Compilation Flow

```
Shader items → ShaderBase::getPatchedSources()
    │ (preamble, includes, printf patching)
    ▼
ShaderCompiler::compileSpirv()
    │ (glslang / DXC / Slang)
    ▼
SPIRV binary
    │
    ├──► Reflection (spirv-reflect) → descriptor bindings, inputs, push constants
    ├──► SPIRV-Cross → GLSL (for GL backend)
    ├──► SPIRV-Cross → HLSL (for D3D backend)
    ├──► SPIRV-Tools → optimization, stripping
    └──► Direct use as VkShaderModule (Vulkan backend)
```

---

## 17. Threading Model

| Thread | Responsibilities |
|--------|-----------------|
| **Main (GUI)** | Qt event loop, all UI, session model mutations, `RenderTask::prepare()`/`configured()`/`finish()` |
| **GL Render** | `GLRenderer::Worker` — GL context current, `RenderTask::configure()`/`render()`/`release()` |
| **VK Render** | `VKRenderer::Worker` — Vulkan command submission |
| **D3D Render** | `D3DRenderer::Worker` — D3D12 command list recording/submission |
| **FileCache Background** | `FileCache::BackgroundLoader` — file I/O, image decoding |
| **Script Interrupt** | Dedicated thread for `QJSEngine` interrupt timer |

**Synchronization**:
- Render threads communicate with main thread via `QMetaObject::invokeMethod` (blocking queued connections).
- `FileCache` protects its maps with `QMutex`.
- `RenderSessionBase::mUsedItemsCopy` protected by `QMutex`.
- The session model is only accessed from the main thread; the render session works on `mSessionModelCopy`.

---

## 18. Key Design Patterns

### 1. Service Locator (Singletons)

All global services accessed via `Singletons::xxx()`. Lazily initialized renderers. Main-thread assertions prevent incorrect cross-thread access.

### 2. Command Queue (Builder + Interpreter)

The render session builds a command queue (vector of `std::function<void(BindingState&)>`) from the session tree, then executes it sequentially. This separates tree traversal (main thread) from GPU command submission (render thread).

### 3. CRTP-like Backend Polymorphism

`RenderSessionBase` uses template methods (`buildCommandQueue<RenderSession, CommandQueue>`) rather than virtual dispatch for the hot path. The `CommandQueue` struct is backend-specific (contains `GLTexture`/`VKTexture`/`D3DTexture`, etc.).

### 4. X-Macro Property Mapping

`SessionModelPriv.h` defines `ADD_EACH_COLUMN_TYPE()` — an X-macro that maps every model column to its item type and field. Used to generate:
- `data()` / `setData()` switch cases.
- JSON serialization/deserialization.
- Undo command generation.

### 5. Discriminated Union Items

Item structs form a type hierarchy, but discriminated by `Item::Type` enum rather than virtual dispatch. `castItem<T>()` provides safe downcasting without RTTI.

### 6. Resource Reuse

`reuseUnmodifiedItems()` uses `operator==` on resource objects to detect unchanged resources between frames, moving previous GPU objects forward rather than recreating them.

### 7. Expression-Driven Properties

String properties like `"256"`, `"viewportWidth"`, `"ceil(numParticles/64)"` are evaluated at render time via `ScriptEngine`, enabling dynamic/animated pipeline configurations.

### 8. Scoped Binding Resolution

Bindings are accumulated on a scope stack as the session tree is traversed. At call execution time, all visible scopes are merged into a flat binding set. This enables inheritance/override semantics.

---

## 19. File/Module Index

### Top-level (`src/`)

| File | Purpose |
|------|---------|
| `main.cpp` | Application entry point, single-instance logic, GL format setup |
| `Singletons.h/cpp` | Global service registry |
| `SynchronizeLogic.h/cpp` | Evaluation orchestrator |
| `Settings.h/cpp` | Persistent settings (QSettings subclass) |
| `FileCache.h/cpp` | Thread-safe file content cache with filesystem watcher |
| `FileDialog.h/cpp` | File dialog utilities, untitled file management |
| `MessageList.h/cpp` | Global thread-safe message collection |
| `TextureData.h/cpp` | KTX-based texture storage and format conversion |
| `InputState.h/cpp` | Mouse/keyboard input capture |
| `VideoManager.h/cpp` | Video file playback management |
| `VideoPlayer.h/cpp` | Qt Multimedia-based video frame decoder |
| `SourceType.h/cpp` | Shader source type enumeration and detection |
| `Evaluation.h` | Evaluation mode/type enums |
| `EditActions.h` | Edit menu action struct |
| `Range.h` | Min/max range struct |
| `Style.h/cpp` | Custom QProxyStyle |
| `Theme.h/cpp` | Theme loading and palette management |
| `getEventPosition.h` | Qt6 event position helper |
| `resources.qrc` | Qt resource file (icons, JS) |
| `version.h.in` / `_version.h` | Git-based version stamp |

### `render/` — Rendering Infrastructure

| File | Purpose |
|------|---------|
| `Renderer.h` | Abstract renderer interface |
| `RenderTask.h/cpp` | Base class for GPU work units with 5-phase lifecycle |
| `RenderSessionBase.h/cpp` | Session render task with command queue, script session, resource management |
| `RenderSessionBase_CommandQueue.h` | Template implementations for command queue build/execute/download |
| `ShaderBase.h/cpp` | Base class for shader compilation across backends |
| `ShaderCompiler.h/cpp` | Shader compilation namespace (glslang, SPIRV-Cross, SPIRV-Tools) |
| `ShaderCompiler_glslang.cpp` | glslang backend for GLSL/HLSL → SPIRV |
| `ShaderCompiler_Microsoft.cpp/h` | D3DCompiler/DXC backend (Windows) |
| `Reflection.h/cpp` | SPIRV reflection wrapper |
| `Reflection_Builder.cpp` | Manual reflection builder (non-SPIRV backends) |
| `Reflection_JSON.cpp` | Reflection → JSON serialization |
| `PipelineBase.h/cpp` | Base class for pipeline state (bindings, buffer member application) |
| `BufferBase.h/cpp` | Base class for GPU buffers |
| `TextureBase.h/cpp` | Base class for GPU textures |
| `PrintfBase.h/cpp` | Shader printf support (source patching, buffer readback, message formatting) |
| `ProcessSource.h/cpp` | Background source validation/transformation task |
| `AdapterIdentity.h/cpp` | GPU adapter UUID/LUID for cross-API identification |
| `ShareSync.h` | Abstract shared resource synchronization |
| `ComputeRange.h` | Compute shader for texture range calculation |
| `Slang.h/cpp` | Optional Slang language integration |

### `render/opengl/` — OpenGL 4.5 Backend

27 files implementing the full GL pipeline: `GLRenderer`, `GLRenderSession`, `GLProgram`, `GLShader`, `GLBuffer`, `GLTexture`, `GLTarget`, `GLStream`, `GLCall`, `GLPrintf`, `GLComputeRange`, `GLShareSync`, `GLContext.h`, `GLObject.h`, `GLAccelerationStructure.h`.

### `render/vulkan/` — Vulkan Backend (via KDGpu)

29 files: `VKRenderer`, `VKRenderSession`, `VKPipeline`, `VKProgram`, `VKShader`, `VKBuffer`, `VKTexture`, `VKTarget`, `VKStream`, `VKCall`, `VKPrintf`, `VKAccelerationStructure`, `VKShareSync`, `VKContext.h`, `KDGpuEnums`.

### `render/direct3d/` — Direct3D 12 Backend (Windows)

31 files: `D3DRenderer`, `D3DRenderSession`, `D3DPipeline`, `D3DProgram`, `D3DShader`, `D3DBuffer`, `D3DTexture`, `D3DTarget`, `D3DStream`, `D3DCall`, `D3DPrintf`, `D3DAccelerationStructure`, `D3DShareSync`, `D3DEnums`, `D3DContext.h`, `d3dx12.h`.

### `editors/` — Editor Subsystem

| Subdirectory | Contents |
|-------------|----------|
| (root) | `EditorManager`, `IEditor`, `DockWindow`, `DockTitle` |
| `source/` | `SourceEditor`, `FindReplaceBar`, `MultiTextCursors`, `Completer`, `SyntaxHighlighter`, `SyntaxGLSL/HLSL/Slang/JavaScript/Generic`, `SourceEditorToolBar` |
| `binary/` | `BinaryEditor`, `BinaryEditorToolBar` |
| `texture/` | `TextureEditor`, `GLWidget`, `TextureItem`, `TextureBackground`, `Histogram`, `TextureInfoBar`, `TextureEditorToolBar` |
| `qml/` | `QmlView` |

### `session/` — Session Model

| File | Purpose |
|------|---------|
| `Item.h/cpp` | Item struct hierarchy + utility functions |
| `ItemEnums.h` | All GPU-pipeline enums (GL-compatible values) |
| `SessionModelCore.h/cpp` | QAbstractItemModel with undo support |
| `SessionModel.h/cpp` | Full model with drag-drop, JSON serialization, scoped traversal |
| `SessionModelPriv.h` | X-macro column-to-field mapping |
| `SessionEditor.h/cpp` | QTreeView for the session tree |
| `properties/` | Per-item-type property panels (10 `.ui` forms, 6 `.cpp` files) |

### `scripting/` — JavaScript Scripting

| File | Purpose |
|------|---------|
| `ScriptEngine.h/cpp` | QJSEngine wrapper with expression evaluation |
| `ScriptEngine.js` | Bootstrap JavaScript (loaded into engine on init) |
| `ScriptSession.h/cpp` | Per-render-session scripting context |
| `CustomActions.h/cpp` | User-defined JS action plugin system |
| `IScriptRenderSession.h` | Interface for script↔render session bridge |
| `objects/` | 7 script objects (App, Console, Editor, Library, Mouse, Keyboard, Session) |

### `windows/` — Application Windows

| File | Purpose |
|------|---------|
| `MainWindow.h/cpp/ui` | Top-level window with menus, toolbars, dock layout |
| `MessageWindow.h/cpp` | Error/warning/info message table |
| `OutputWindow.h/cpp` | Processed source output viewer |
| `FileBrowserWindow.h/cpp` | File system browser |
| `AboutDialog.h/cpp` | About dialog |
| `WindowTitle.h/cpp` | Custom window title rendering |
| `AutoOrientationSplitter.h` | Splitter that adapts orientation to window aspect ratio |

### `widgets/` — Reusable Custom Widgets

7 widgets: `ExpressionLineEdit`, `ExpressionEditor`, `ExpressionMatrix`, `ReferenceComboBox`, `DataComboBox`, `ColorPicker`, `ColorMask`.

---

*End of report.*
