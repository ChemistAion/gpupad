# GPUpad — Complete Feature & Architecture Encyclopedia

> **Audience**: AI coding assistants, plugin/action authors, session designers.
> **Scope**: Every user-facing feature, every session-item type, every scripting API surface, every demo session — plus the complete codebase architecture for developers extending or modifying GPUpad.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Codebase Architecture & Conventions](#2-codebase-architecture--conventions)
3. [Session & the GPJS File Format](#3-session--the-gpjs-file-format)
4. [Session Items — Complete Reference](#4-session-items--complete-reference)
5. [Enumerations — Complete Reference](#5-enumerations--complete-reference)
6. [Evaluation Pipeline](#6-evaluation-pipeline)
7. [Scripting API](#7-scripting-api)
8. [Custom Actions](#8-custom-actions)
9. [Built-in Shader Features](#9-built-in-shader-features)
10. [Sample Sessions Catalogue](#10-sample-sessions-catalogue)
11. [Existing Custom Actions Catalogue](#11-existing-custom-actions-catalogue)
12. [Extra Directory — Themes, Libraries & Packaging](#12-extra-directory--themes-libraries--packaging)

---

## 1. Overview

GPUpad is a lightweight IDE for GPU algorithm development. It supports **OpenGL**, **Vulkan**, and **Direct3D 12** renderers with GLSL, HLSL, and Slang shader languages.

**Tech stack**: C++20, Qt 6 (Widgets, OpenGL, Qml, Quick, Multimedia), CMake 3.21+. Targets Windows (MSVC), Linux (GCC/Clang), macOS.

**Core workflow**: Define a *session* (`.gpjs`) describing GPU resources (textures, buffers, programs, targets, bindings) and *calls* (draw, compute, ray-trace). Evaluate the session to see results. Extend with JavaScript scripts and custom actions.

**Key capabilities**:
- Fully customizable render state (blend, depth, stencil, polygon mode, logic ops)
- Compute shaders with SSBO, image load/store, atomic operations
- Hardware ray tracing (Vulkan) with acceleration structures
- Mesh and task shaders (NV extension)
- Tessellation pipeline (control + evaluation + geometry stages)
- Printf-style shader debugging with per-pixel output
- JavaScript expressions in uniform bindings (live-evaluated each frame)
- JavaScript scripting for dynamic session manipulation
- Custom actions (JS + QML + optional C++ plugins) for tool extensions
- Video file playback as texture source
- KTX/DDS support for 3D/array/cube/compressed textures

---

## 2. Codebase Architecture & Conventions

> For the full deep-dive report, see `.github/docs/architecture/gpupad-report.md`.
> This section provides the essential architectural knowledge for modifying or extending GPUpad.

### 2.1 Source Tree Layout

```
src/
├── main.cpp                    # Entry point, single-instance, GL format setup
├── Singletons.h/cpp            # Global service registry (service locator)
├── SynchronizeLogic.h/cpp      # Evaluation orchestrator (bridges model↔render↔editors)
├── Settings.h/cpp              # Persistent settings (QSettings subclass)
├── FileCache.h/cpp             # Thread-safe file cache with filesystem watcher
├── FileDialog.h/cpp            # File dialog helpers, untitled file naming
├── MessageList.h/cpp           # Thread-safe message collection
├── TextureData.h/cpp           # KTX-based texture storage and format conversion
├── InputState.h/cpp            # Mouse/keyboard input capture for shaders
├── VideoManager.h / VideoPlayer.h  # Video file playback (optional Qt Multimedia)
├── Evaluation.h                # EvaluationMode/EvaluationType enums
├── Theme.h/cpp                 # JSON-based theme loading, QPalette management
├── Style.h/cpp                 # Custom QProxyStyle
│
├── session/                    # SESSION DATA MODEL
│   ├── Item.h/cpp              # Item struct hierarchy + castItem<T>()
│   ├── ItemEnums.h             # All GPU-pipeline enums (GL-compatible values)
│   ├── SessionModelCore.h/cpp  # QAbstractItemModel with undo, ~136 ColumnType entries
│   ├── SessionModelPriv.h      # X-macro column↔field mapping (ADD_EACH_COLUMN_TYPE)
│   ├── SessionModel.h/cpp      # Full model: JSON, drag-drop, scoped traversal
│   ├── SessionEditor.h/cpp     # QTreeView for the session tree
│   └── properties/             # Per-item-type property panels (10 .ui forms)
│
├── render/                     # RENDERING INFRASTRUCTURE
│   ├── Renderer.h              # Abstract renderer interface
│   ├── RenderTask.h/cpp        # 6-phase GPU work unit lifecycle
│   ├── RenderSessionBase.h/cpp # Command queue, script session, resource management
│   ├── RenderSessionBase_CommandQueue.h  # Template: buildCommandQueue / executeCommandQueue
│   ├── ShaderBase.h/cpp        # Shader compilation base (preamble, includes, printf patching)
│   ├── ShaderCompiler.h/cpp    # glslang/DXC/Slang → SPIRV compilation
│   ├── Reflection.h/cpp        # SPIRV reflection (spirv-reflect wrapper)
│   ├── PipelineBase.h/cpp      # Pipeline state, uniform buffer member application
│   ├── PrintfBase.h/cpp        # Shader printf (source patching, buffer readback)
│   ├── ProcessSource.h/cpp     # Background source validation/transformation
│   ├── ShareSync.h             # Cross-API texture sharing interface
│   │
│   ├── opengl/                 # OpenGL 4.5 backend (~27 files)
│   │   └── GL{Renderer,RenderSession,Program,Shader,Buffer,Texture,Target,Stream,Call,...}
│   ├── vulkan/                 # Vulkan backend via KDGpu (~29 files)
│   │   └── VK{Renderer,RenderSession,Pipeline,Program,Shader,Buffer,Texture,Target,...}
│   └── direct3d/               # Direct3D 12 backend, Windows-only (~31 files)
│       └── D3D{Renderer,RenderSession,Pipeline,Program,Shader,Buffer,Texture,Target,...}
│
├── editors/                    # EDITOR SUBSYSTEM
│   ├── EditorManager.h/cpp     # Central editor dock management
│   ├── IEditor.h               # Abstract editor interface
│   ├── source/                 # SourceEditor, FindReplaceBar, MultiTextCursors,
│   │                           # Completer, SyntaxHighlighter, SyntaxGLSL/HLSL/Slang/JS
│   ├── binary/                 # BinaryEditor (hex + structured data views)
│   ├── texture/                # TextureEditor, GLWidget, TextureItem, Histogram
│   └── qml/                    # QmlView (QQuickWidget for custom UIs)
│
├── scripting/                  # JAVASCRIPT SCRIPTING
│   ├── ScriptEngine.h/cpp      # QJSEngine wrapper, expression evaluation
│   ├── ScriptSession.h/cpp     # Per-render-session scripting context
│   ├── CustomActions.h/cpp     # User-defined JS action plugin system
│   └── objects/                # 7 script objects: App, Console, Editor, Library,
│                               # Mouse, Keyboard, Session
│
├── windows/                    # APPLICATION WINDOWS
│   ├── MainWindow.h/cpp/ui     # Top-level QMainWindow with menus, toolbars, docks
│   ├── MessageWindow.h/cpp     # Error/warning/info message table
│   ├── OutputWindow.h/cpp      # Processed source output viewer
│   └── FileBrowserWindow.h/cpp # File system tree browser
│
└── widgets/                    # REUSABLE CUSTOM WIDGETS
    ├── ExpressionLineEdit      # Numeric expression input with mouse-wheel stepping
    ├── ExpressionEditor        # Multi-line expression editor
    ├── ExpressionMatrix        # Matrix editor (2x2 to 4x4)
    ├── ReferenceComboBox       # Session item picker (by type + ID)
    ├── DataComboBox            # Combo with associated data values
    ├── ColorPicker             # Color selection widget
    └── ColorMask               # 4-bit RGBA write mask toggle
```

### 2.2 Build System & Dependencies

| Feature | Details |
|---------|---------|
| **Standard** | C++20, CMake 3.21+ |
| **Unity Build** | Optional (`ENABLE_UNITY_BUILD`) |
| **Qt Modules** | Core, Widgets, OpenGLWidgets, OpenGL, Qml; optional Quick, QuickWidgets, Multimedia |
| **GPU Libraries** | KDGpu (Vulkan abstraction, `libs/KDGpu`), Vulkan SDK, VulkanMemoryAllocator |
| **Shader Toolchain** | glslang (GLSL→SPIRV), SPIRV-Cross (cross-compilation), SPIRV-Tools (optimization), spirv-reflect |
| **Optional** | OpenImageIO (extended image formats), Slang (shader language), DXC (DirectX Shader Compiler) |
| **Bundled** | SingleApplication, spirv-reflect, stb, d3d12 helpers, dllreflect |
| **Version** | Git-tag based: `version.h.in` → `_version.h` |
| **Packaging** | CPack: WIX (Windows), TGZ (Linux), DragNDrop (macOS) |

### 2.3 C++ Conventions

**Qt hardening macros** (all enforced via compile definitions):
```
QT_NO_CAST_TO_ASCII, QT_NO_URL_CAST_FROM_STRING, QT_NO_CAST_FROM_BYTEARRAY,
QT_NO_SIGNALS_SLOTS_KEYWORDS, QT_USE_QSTRINGBUILDER,
QT_NO_NARROWING_CONVERSIONS_IN_CONNECT, QT_NO_KEYWORDS,
QT_DISABLE_DEPRECATED_BEFORE=0x060500, QT_NO_FOREACH
```

**Key rules**:
- **No `signals`/`slots`/`emit` keywords** — use `Q_SIGNALS`, `Q_SLOTS`, `Q_EMIT` exclusively
- **No RTTI** — disabled via `/GR-` (MSVC) / `-fno-rtti` (GCC/Clang); use `castItem<T>()` instead
- **No `foreach`** — use range-for loops
- **No deprecated Qt APIs** before Qt 6.5

**Naming conventions**:
- Member variables: `mCamelCase` prefix (e.g., `mSessionFileName`, `mEvaluationMode`, `mFrameIndex`)
- Methods: `camelCase` (e.g., `resetRenderSession()`, `evaluateBlockProperties()`)
- Signal handlers: `handle*` prefix (e.g., `handleItemModified()`, `handleSessionRendered()`)
- Types/Classes: `PascalCase`

**Include order** (per header):
1. `#pragma once`
2. Local project includes (quoted: `"FileCache.h"`)
3. Qt headers (angle brackets: `<QObject>`, `<QMap>`)
4. System/external headers

**Enum values** in `ItemEnums.h` are set to their **OpenGL constants** (e.g., `Triangles = GL_TRIANGLES`, `Float = GL_FLOAT`) for zero-cost translation in the GL backend. Non-GL values use high hex (e.g., `RayGeneration = 0x10000`).

### 2.4 Singleton Registry

`Singletons.h/cpp` — central service locator, constructed on the stack inside `MainWindow`, exposed via static accessors.

| Accessor | Type | Purpose |
|----------|------|---------|
| `settings()` | `Settings` | Persistent settings (font, tab size, themes) |
| `fileCache()` | `FileCache` | Thread-safe source/binary/texture cache + filesystem watcher |
| `fileDialog()` | `FileDialog` | File dialog helpers, untitled file naming |
| `editorManager()` | `EditorManager` | All open editor dock widgets |
| `sessionModel()` | `SessionModel` | Session tree data model (QAbstractItemModel) |
| `synchronizeLogic()` | `SynchronizeLogic` | Evaluation loop orchestrator |
| `videoManager()` | `VideoManager` | Video file playback (optional) |
| `inputState()` | `InputState` | Mouse/keyboard state for shaders |
| `customActions()` | `CustomActions` | JS action plugin discovery/execution |
| `defaultScriptEngine()` | `ScriptEngine` | Default JS engine for expression evaluation |
| `glRenderer()` | `GLRenderer` | OpenGL 4.5 renderer (lazy-init) |
| `vkRenderer()` | `VKRenderer` | Vulkan renderer via KDGpu (lazy-init) |
| `d3dRenderer()` | `D3DRenderer` | Direct3D 12 renderer (lazy-init, Windows-only) |
| `sessionRenderer()` | `RendererPtr` | Returns renderer for the session's chosen API |

**Thread safety**: Most singletons assert `onMainThread()`. `FileCache` uses `QMutex`. Renderers are lazily created.

### 2.5 Session Model Internals

**Item struct hierarchy** (`session/Item.h`):
```
Item (base: id, type, parent, items[], name)
├── FileItem : Item (+fileName)
├── ScopeItem : Item (marks hierarchy containers)
│   ├── Root : ScopeItem
│   ├── Session : ScopeItem (+renderer, shaderLanguage, compiler, preamble, ...)
│   └── Group : ScopeItem (+iterations, inlineScope, dynamic)
├── Buffer : FileItem
├── Texture : FileItem (+target, format, width, height, depth, layers, samples, flip)
├── Shader : FileItem (+shaderType, entryPoint, preamble, includePaths)
├── Script : FileItem (+executeOn)
├── Block : Item (+offset, rowCount)
├── Field : Item (+dataType, count, padding)
├── Program : Item
├── Binding : Item (30+ fields: texture/buffer IDs, filters, wrap modes, ...)
├── Stream : Item
├── Attribute : Item (+fieldId, normalize, divisor)
├── Target : Item (+frontFace, cullMode, polygonMode, logicOp, blendConstant, ...)
├── Attachment : Item (40+ fields: blend equations, stencil ops, depth settings)
├── Call : Item (30+ fields: program/target/stream IDs, draw/compute params, ...)
├── AccelerationStructure : Item
├── Instance : Item (+transform)
└── Geometry : Item (+geometryType, vertex/index/transform buffer IDs, ...)
```

**Type-safe casting** — no RTTI, discriminated by `Item::Type` enum:
```cpp
template <typename T>
const T *castItem(const Item &item) {
    if (item.type == getItemType<T>()) return static_cast<const T *>(&item);
    return nullptr;
}
// Specializations: castItem<FileItem> accepts Buffer|Texture|Shader|Script
//                  castItem<ScopeItem> accepts Root|Session|Group
```

**X-macro property mapping** (`SessionModelPriv.h`):
```cpp
#define ADD_EACH_COLUMN_TYPE()          \
    ADD(SessionRenderer, Session, renderer) \
    ADD(SessionShaderLanguage, Session, shaderLanguage) \
    /* ... ~136 entries total ... */ \
    ADD(GeometryOffset, Geometry, offset)
```
- `ADD(ColumnName, StructType, fieldName)` maps each model column to its item struct and field
- Generates: `ColumnType` enum entries, `data()`/`setData()` switch cases, JSON serialization, undo commands

**Undo system**: All mutations via `undoableAssignment(index, &field, value, mergeId)` backed by `QUndoStack`.

**Scoped traversal**: `forEachItemScoped(index, func)` walks UP the tree from a position, visiting items that are visible in the current scope (respects `Group::inlineScope`). Used to populate combo boxes and resolve bindings.

### 2.6 Rendering Pipeline

**Architecture**:
```
SessionModel ──► SynchronizeLogic (orchestrator)
                       │
                       ├─ RenderSessionBase::prepare()    [main thread]
                       ├─ RenderSessionBase::configure()   [render thread]
                       ├─ RenderSessionBase::configured()  [main thread]
                       ├─ RenderSessionBase::render()      [render thread]
                       └─ RenderSessionBase::finish()      [main thread]
```

**Renderer** (`render/Renderer.h`) — abstract base:
```cpp
class Renderer {
    RenderAPI mApi;  // OpenGL | Vulkan | Direct3D
    virtual QThread *renderThread() = 0;
    virtual void render(RenderTask *task) = 0;
    virtual void release(RenderTask *task) = 0;
};
```
Each renderer owns a dedicated `QThread` and task queue. Tasks are submitted from main thread.

**RenderTask** (`render/RenderTask.h`) — 6-phase GPU work lifecycle:
1. `prepare(bool, EvaluationType)` — main thread, snapshot session state
2. `configure()` — render thread, create/update GPU resources
3. `configured()` — main thread callback
4. `render()` — render thread, issue GPU commands (pure virtual)
5. `finish()` — main thread, read back results, update editors
6. `release()` — render thread, destroy GPU resources

Thread dispatch: `dispatchToRenderThread(F&&)` uses `Qt::BlockingQueuedConnection`.

**RenderSessionBase** (`render/RenderSessionBase.h`) — the heart of rendering:
- Inherits `RenderTask` + `IScriptRenderSession`
- `using Command = std::function<void(BindingState&)>` — command queue entry type
- `using BindingState = QStack<Bindings>` — scope stack for binding resolution
- Template methods: `buildCommandQueue<RenderSession, CommandQueue>()`, `executeCommandQueue()`, `beginDownloadModifiedResources()`, `finishCommandQueue()`
- **Resource reuse**: `reuseUnmodifiedItems()` compares new vs. previous frame; unchanged GPU objects are moved, not recreated
- **ScriptSession** integration: `std::unique_ptr<ScriptSession>` for expression evaluation during command execution

**Command queue building** (`RenderSessionBase_CommandQueue.h`):
- Iterates session tree via `sessionModel.forEachItem()`
- Groups → push/pop scope, handle iteration loops
- Bindings → create typed binding commands (Uniform/Sampler/Image/Buffer/Subroutine)
- Calls → merge visible binding scopes, execute with time query wrapping
- Helper lambdas: `addProgramOnce`, `addBufferOnce`, `addTextureOnce`, `addTargetOnce`

**Binding resolution** — scoped stack model:
```
Group A (push scope)
  Binding uColor = red       ← enters scope
  Group B (push scope)
    Binding uColor = blue    ← shadows parent
    Call (sees uColor=blue)
  (pop scope)
  Call (sees uColor=red)
(pop scope)
```

### 2.7 Backend Implementations

All three backends follow identical naming: `XX{Renderer,RenderSession,Program,Shader,Buffer,Texture,Target,Stream,Call,Pipeline,Printf,ShareSync}` + backend-specific context/enums.

**OpenGL** (`render/opengl/`, ~27 files):
- `GLRenderer`: QThread + Worker; offscreen `QOpenGLContext`; VAO-based
- `GLShader`: Driver compilation or glslang→SPIRV→SPIRV-Cross→GLSL pipeline
- `GLBuffer`: `glNamedBufferStorage` with readback
- `GLTexture`: KTX upload, mipmap generation, shared-memory export
- `GLProgram_Reflection.cpp`: GL-native reflection via `glGetActiveUniform` etc.

**Vulkan** (`render/vulkan/`, ~29 files):
- `VKRenderer`: KDGpu `Device` + `Queue`; VMA for memory allocation
- `VKPipeline`: Graphics/compute/ray-tracing PSO creation with descriptor set layout
- `VKAccelerationStructure`: BLAS/TLAS construction for ray tracing
- `KDGpuEnums.h`: Maps GPUpad's GL-based enums → KDGpu/Vulkan enums
- `VKShareSync`: External memory + semaphores for VK↔GL sharing

**Direct3D 12** (`render/direct3d/`, ~31 files, Windows-only):
- `D3DRenderer`: D3D12 device + command queue; non-Windows = stub
- `D3DPipeline`: PSO + root signature generation
- `D3DShader`: HLSL compilation via D3DCompiler or DXC; `D3DShader_Reflection`
- `D3DEnums.h`: GL-enum → DXGI/D3D12 enum translation

**Resource sharing** (`ShareSync`): Cross-API texture sharing for real-time preview:
- VK→GL: External memory (`GL_EXT_memory_object`)
- D3D→GL: D3D11-interop texture → GL import
- GL→GL: Shared contexts (trivial)

### 2.8 Shader Compilation & Reflection

**Pipeline**:
```
Shader items → ShaderBase::getPatchedSources()      [preamble, includes, printf patching]
            → ShaderCompiler::compileSpirv()         [glslang / DXC / Slang]
            → SPIRV binary
                ├── spirv-reflect → descriptor bindings, inputs, push constants
                ├── SPIRV-Cross   → GLSL (GL backend) or HLSL (D3D backend)
                ├── SPIRV-Tools   → optimization, stripping
                └── Direct use as VkShaderModule (Vulkan backend)
```

**ShaderCompiler namespace** (`render/ShaderCompiler.h`):
- `compileSpirv()`, `preprocess()`, `disassemble()`, `generateGLSL()`, `generateHLSL()`, `stripReflection()`, `generateAST()`
- Custom `#include` resolution via `TShader::Includer` (glslang)
- Configurable: auto-map bindings/locations, Vulkan relaxed rules, SPIRV version

**Reflection** (`render/Reflection.h`):
- Wraps `SpvReflectShaderModule` (spirv-reflect)
- Two construction paths: from SPIRV binary, or from manual `Builder` (D3D's own reflection)
- Accessors: `descriptorBindings()`, `pushConstantBlocks()`, `inputVariables()`
- JSON serialization for the output window

**Printf** (`render/PrintfBase.h`):
1. `patchSource()` rewrites `printf(...)` → SSBO atomic writes
2. Backend provides `_printfBuffer` (SSBO/UAV)
3. After render, buffer read back → format strings matched to values → `ScriptMessage`

### 2.9 Editor Subsystem

**EditorManager** (`editors/EditorManager.h`):
- Central manager inheriting `DockWindow` (a `QMainWindow` as dock container)
- Maintains typed lists: `mSourceEditors`, `mBinaryEditors`, `mTextureEditors`, `mQmlViews`
- One editor per unique filename; navigation stack for back/forward
- Tabify groups: source=0, binary=0, texture=1, qml=3

**IEditor interface**:
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

**Four editor types**:

| Type | Base | Features |
|------|------|----------|
| `SourceEditor` | `QPlainTextEdit` | Line numbers, syntax highlighting (GLSL/HLSL/Slang/JS), auto-completion, find/replace with regex, multi-cursor editing |
| `BinaryEditor` | `QTableView` | Hex view + structured data view (from Block/Field layout), typed editing via `SpinBoxDelegate` |
| `TextureEditor` | `QAbstractScrollArea` | GL-rendered texture preview with zoom/pan, checkerboard alpha, GPU histogram, mipmap selection |
| `QmlView` | `QQuickWidget` | QML content rendering, dependency tracking, ScriptEngine integration |

### 2.10 Scripting Engine Internals

**ScriptEngine** (`scripting/ScriptEngine.h`) — wraps `QJSEngine`:
```cpp
ScriptValueList evaluateValues(const QString &expr, ItemId);
ScriptValue evaluateValue(const QString &expr, ItemId);
int32_t evaluateInt(const QString &expr, ItemId);
uint32_t evaluateUInt(const QString &expr, ItemId);
QJSValue call(QJSValue &callable, const QJSValueList &args, ItemId);
void setGlobal(const QString &name, QObject *object);
```
- **omitReferenceErrors mode**: Silently ignores undefined variable errors during expression evaluation (not all globals are available in all contexts)
- **Interrupt timer**: Prevents infinite loops with configurable timeout on dedicated thread

**Script objects** (`scripting/objects/`):

| File | Exposed As | Key Members |
|------|-----------|-------------|
| `AppScriptObject` | `app` | frameIndex, frameRate, time, timeDelta, session, mouse, keyboard |
| `ConsoleScriptObject` | `console` | log(), warn(), error() → Message window |
| `SessionScriptObject` | `app.session` | findItem/Items(), insertItem(), deleteItem(), setBufferData(), etc. |
| `MouseScriptObject` | `app.mouse` | coord, fragCoord, prevCoord, buttons, editorSize, delta |
| `KeyboardScriptObject` | `app.keyboard` | keys |
| `EditorScriptObject` | (dynamic) | fileName, viewportSize |
| `LibraryScriptObject` | (via loadLibrary) | Opaque, Array, Callable types |

Registration pattern: `Q_PROPERTY` macros + `Q_INVOKABLE` methods.

**ScriptSession** (`scripting/ScriptSession.h`) — 3-phase lifecycle:
1. `update()` — main thread: registers globals (time, frame, mouse, keyboard, viewport)
2. `beginSessionUpdate()` → `engine()` — render thread: JS engine available for expression eval
3. `endSessionUpdate()` — main thread: collects messages, resets state

### 2.11 Threading Model

| Thread | Responsibilities |
|--------|-----------------|
| **Main (GUI)** | Qt event loop, all UI, session model mutations, `RenderTask::prepare()`/`configured()`/`finish()` |
| **GL Render** | `GLRenderer::Worker` — GL context current, `configure()`/`render()`/`release()` |
| **VK Render** | `VKRenderer::Worker` — Vulkan command submission |
| **D3D Render** | `D3DRenderer::Worker` — D3D12 command list recording/submission |
| **FileCache Background** | `FileCache::BackgroundLoader` — file I/O, image decoding |
| **Script Interrupt** | Dedicated thread for `QJSEngine` interrupt timer |

**Synchronization**:
- Render↔main: `QMetaObject::invokeMethod` with `Qt::BlockingQueuedConnection`
- `FileCache`: `QMutex`-protected maps
- Session model: accessed only from main thread; render session works on `mSessionModelCopy`
- `RenderSessionBase::mUsedItemsCopy`: protected by `QMutex`

### 2.12 Key Design Patterns

| Pattern | Where | How |
|---------|-------|-----|
| **Service Locator** | `Singletons` | All services via static accessors; lazy renderer creation |
| **Command Queue** | `RenderSessionBase` | `vector<function<void(BindingState&)>>` built from session tree, executed sequentially |
| **CRTP-like Templates** | `buildCommandQueue<RS, CQ>()` | Template methods avoid virtual dispatch on hot path; `CommandQueue` is backend-specific |
| **X-Macro Property Mapping** | `SessionModelPriv.h` | `ADD_EACH_COLUMN_TYPE()` generates data()/setData(), JSON, undo for all ~136 properties |
| **Discriminated Unions** | `Item` hierarchy | `Item::Type` enum + `castItem<T>()` instead of virtual dispatch / RTTI |
| **Resource Reuse** | `reuseUnmodifiedItems()` | `operator==` comparison; unchanged GPU objects moved between frames |
| **Expression-Driven Properties** | Binding values, texture dimensions | String expressions evaluated at render time via `ScriptEngine` |
| **Scoped Binding Resolution** | `BindingState = QStack<Bindings>` | Scope push/pop during tree traversal; merged at call execution |

### 2.13 Cross-Subsystem Data Flow

**Session evaluation flow**:
```
SessionEditor (QTreeView) ─► model changes ─► SynchronizeLogic
                                                    │ evaluate()
EditorManager ◄── fileCache sync ──► RenderSessionBase (GL/VK/D3D)
(source/bin/tex)    update editors          │ render thread
                                            ▼
                                      CommandQueue
                                      (programs, buffers, textures,
                                       targets, calls, bindings)
```

**Expression evaluation flow**:
```
UI (ExpressionLineEdit) → "viewportWidth * 2"
    → ScriptEngine::evaluateInt()
        → QJSEngine evaluation
            → numeric result → texture width / workgroup count / etc.
```

**Shader compilation flow**:
```
Shader items → getPatchedSources() [preamble, includes, printf]
    → compileSpirv() [glslang / DXC / Slang]
        → SPIRV binary
            ├─ Reflection → descriptor bindings, inputs
            ├─ SPIRV-Cross → GLSL (GL) / HLSL (D3D)
            └─ Direct use → VkShaderModule (Vulkan)
```

---

## 3. Session & the GPJS File Format

### 3.1 Format Basics

A `.gpjs` file is **JSON**. The top-level is either an array of items or a single item object. Each item has the structure:

```json
{
  "type": "ItemTypeName",
  "name": "Human-readable name",
  "id": 123,
  "items": [ /* child items */ ],
  /* ...type-specific properties... */
}
```

- **Enum values** are stored as their **string key names** (e.g. `"format": "RGBA8_UNorm"`, `"callType": "Draw"`).
- **Expressions** are stored as strings and evaluated as JavaScript at runtime (e.g. `"width": "1024"`, `"workGroupsX": "Math.ceil(count / 256)"`).
- **Item references** are stored as integer IDs (e.g. `"programId": 42`, `"textureId": 7`).
- **`id`** values are session-unique integers. GPUpad auto-assigns them.
- Items can be dragged to/from a text editor (serialized as JSON) and copy/pasted between instances.

### 3.2 Session-Level Properties

| Property | Type | Description |
|----------|------|-------------|
| `renderer` | `Renderer` | `OpenGL`, `Vulkan`, `Direct3D` |
| `shaderLanguage` | `ShaderLanguage` | `GLSL`, `HLSL`, `Slang`, `None` |
| `shaderCompiler` | `ShaderCompiler` | `Driver`, `glslang`, `D3DCompiler`, `DXC`, `Slang` |
| `shaderCompilerSettings` | Object | Compiler-specific key-value settings |
| `shaderPreamble` | String | Global code prefixed to every shader (e.g. `#version 460`) |
| `shaderIncludePaths` | String | Semicolon-separated include search paths |
| `flipViewport` | Bool | Flip viewport Y axis |
| `reverseCulling` | Bool | Reverse front-face winding |

### 3.3 Item Hierarchy Rules

```
Session
├── Group (nestable, creates scope unless inlineScope=true)
│   ├── Program
│   │   └── Shader (Vertex, Fragment, Compute, Mesh, Task, TessControl, TessEval, Geometry, Includable, Ray*)
│   ├── Texture
│   ├── Buffer
│   │   └── Block
│   │       └── Field
│   ├── Target
│   │   └── Attachment
│   ├── Binding
│   ├── Stream
│   │   └── Attribute
│   ├── Call
│   ├── Script
│   ├── AccelerationStructure
│   │   ├── Instance
│   │   └── Geometry
│   └── Group (recursive)
```

- **Scoping**: Groups open a new scope. Items in a scope are invisible outside (they don't appear in combo boxes). Set `inlineScope: true` to share scope with parent.
- **Evaluation order**: Calls execute top-to-bottom. Bindings apply to all subsequent calls in the same scope until shadowed.

---

## 4. Session Items — Complete Reference

### 4.1 Group

Organizational container for structuring complex sessions.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `iterations` | Expression | `"1"` | Number of times to evaluate children |
| `inlineScope` | Bool | `false` | When true, children share parent's scope |
| `dynamic` | Bool | `false` | Dynamic group flag |

### 4.2 Program

Container for shader stages. No additional properties beyond `name`.

Child items: one or more **Shader** items.

### 4.3 Shader

Individual shader source file.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fileName` | String | — | Path to shader source file |
| `shaderType` | `ShaderType` | — | Stage type (see enum) |
| `entryPoint` | String | `""` | Entry point function name |
| `preamble` | String | `""` | Per-shader code prefix (prepended to source) |
| `includePaths` | String | `""` | Per-shader include paths |

**Shader types**: `Vertex`, `Fragment`, `Geometry`, `TessControl`, `TessEvaluation`, `Compute`, `Task`, `Mesh`, `Includable`, `RayGeneration`, `RayIntersection`, `RayAnyHit`, `RayClosestHit`, `RayMiss`, `RayCallable`

The `Includable` type marks a shader as a shared include (not compiled as a stage).

### 4.4 Texture

GPU texture resource. Can be backed by an image/video file.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fileName` | String | `""` | Image/video file path (KTX, DDS, PNG, JPG, MP4, etc.) |
| `target` | `TextureTarget` | `Target2D` | Dimensionality |
| `format` | `TextureFormat` | `RGBA8_UNorm` | Pixel format |
| `width` | Expression | `"256"` | Width in pixels |
| `height` | Expression | `"256"` | Height in pixels |
| `depth` | Expression | `"1"` | Depth (3D textures) |
| `layers` | Expression | `"1"` | Array layer count |
| `samples` | Int | `1` | MSAA sample count |
| `flipVertically` | Bool | `false` | Flip on load |

**Texture targets**: `Target1D`, `Target1DArray`, `Target2D`, `Target2DArray`, `Target3D`, `TargetCubeMap`, `TargetCubeMapArray`

### 4.5 Buffer

Binary data container. Can be backed by a binary file.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fileName` | String | `""` | Binary file path |

Child items: one or more **Block** items.

### 4.6 Block

A structured region within a Buffer.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `offset` | Expression | `"0"` | Byte offset within parent buffer |
| `rowCount` | Expression | `"1"` | Number of data rows |

Child items: one or more **Field** items.

### 4.7 Field

A single data column within a Block.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `dataType` | `DataType` | `Float` | Element data type |
| `count` | Int | `1` | Number of elements per row (e.g. 3 for vec3) |
| `padding` | Int | `0` | Trailing padding bytes |

### 4.8 Binding

Binds data to a program's named binding point. Affects all subsequent calls in scope until shadowed by another binding with the same name.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bindingType` | `BindingType` | `Uniform` | Kind of binding |
| `editor` | `BindingEditor` | `Expression` | UI editor type |
| `values` | String[] | `[]` | Value expressions |
| `textureId` | ItemId | — | Referenced texture (Sampler/Image) |
| `bufferId` | ItemId | — | Referenced buffer |
| `blockId` | ItemId | — | Referenced block |
| `level` | Int | `0` | Mipmap level |
| `layer` | Int | `-1` | Array layer (-1 = all layers) |
| `minFilter` | `Filter` | `Linear` | Minification filter |
| `magFilter` | `Filter` | `Linear` | Magnification filter |
| `anisotropic` | Bool | `false` | Anisotropic filtering |
| `wrapModeX` | `WrapMode` | `Repeat` | X-axis wrap |
| `wrapModeY` | `WrapMode` | `Repeat` | Y-axis wrap |
| `wrapModeZ` | `WrapMode` | `Repeat` | Z-axis wrap |
| `borderColor` | Color | — | Border clamp color |
| `comparisonFunc` | `ComparisonFunc` | — | Depth comparison for shadow samplers |
| `subroutine` | String | `""` | Subroutine function name |
| `imageFormat` | `ImageBindingFormat` | `Internal` | Image binding format |

**Binding types**: `Uniform`, `Sampler`, `Buffer`, `BufferBlock`, `Image`, `TextureBuffer`, `Subroutine`

**Editor types**: `Expression`, `Expression2`, `Expression3`, `Expression4`, `Expression2x2`…`Expression4x4`, `Color`

**Expression bindings** are live-evaluated as JavaScript each frame. All `app.*` globals are available (e.g. `app.time`, `app.mouse.fragCoord`, `Math.sin(app.time)`).

### 4.9 Target

Render target with framebuffer configuration.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `frontFace` | `FrontFace` | `CCW` | Front-face winding |
| `cullMode` | `CullMode` | `NoCulling` | Face culling mode |
| `polygonMode` | `PolygonMode` | `Fill` | Polygon rasterization mode |
| `logicOperation` | `LogicOperation` | `NoLogicOperation` | Framebuffer logic op |
| `blendConstant` | Color | — | Blend constant color |
| `defaultWidth` | Expression | — | Default attachment width |
| `defaultHeight` | Expression | — | Default attachment height |
| `defaultLayers` | Expression | — | Default attachment layers |
| `defaultSamples` | Int | `1` | Default MSAA samples |

Child items: one or more **Attachment** items.

### 4.10 Attachment

Single color/depth/stencil attachment on a Target.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `textureId` | ItemId | — | Attached texture |
| `level` | Int | `0` | Mipmap level |
| `layer` | Int | `-1` | Array layer (-1 = all) |

**Blend state** (per-attachment):

| Property | Type | Default |
|----------|------|---------|
| `blendColorEq` | `BlendEquation` | `Add` |
| `blendAlphaEq` | `BlendEquation` | `Add` |
| `blendColorSource` | `BlendFactor` | `One` |
| `blendColorDest` | `BlendFactor` | `Zero` |
| `blendAlphaSource` | `BlendFactor` | `One` |
| `blendAlphaDest` | `BlendFactor` | `Zero` |
| `colorWriteMask` | UInt | `0xF` | RGBA write mask |

**Depth state**:

| Property | Type | Default |
|----------|------|---------|
| `depthComparisonFunc` | `ComparisonFunc` | — |
| `depthWrite` | Bool | `true` |
| `depthClamp` | Bool | `false` |
| `depthOffsetSlope` | Double | `0.0` |
| `depthOffsetConstant` | Double | `0.0` |

**Stencil state** (separate front/back):

| Property | Type | Default |
|----------|------|---------|
| `stencilFront/BackComparisonFunc` | `ComparisonFunc` | — |
| `stencilFront/BackReference` | Int | `0` |
| `stencilFront/BackReadMask` | UInt | `255` |
| `stencilFront/BackWriteMask` | UInt | `255` |
| `stencilFront/BackFailOp` | `StencilOperation` | `Keep` |
| `stencilFront/BackDepthFailOp` | `StencilOperation` | `Keep` |
| `stencilFront/BackDepthPassOp` | `StencilOperation` | `Keep` |

### 4.11 Stream

Vertex attribute stream for draw calls.

Child items: one or more **Attribute** items.

### 4.12 Attribute

Individual vertex attribute within a Stream.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fieldId` | ItemId | — | Referenced Buffer Field |
| `normalize` | Bool | `false` | Normalize integer values to [0,1] |
| `divisor` | Int | `0` | Instance divisor (0 = per-vertex, 1+ = per-N-instances) |

### 4.13 Call

GPU execution command. All active calls evaluate top-to-bottom during session evaluation.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `callType` | `CallType` | `Draw` | Execution type |
| `executeOn` | `ExecuteOn` | `EveryEvaluation` | When to execute |
| `checked` | Bool | `true` | Enable/disable toggle |
| `programId` | ItemId | — | Program to use |
| `targetId` | ItemId | — | Render target |
| `vertexStreamId` | ItemId | — | Vertex stream |

**Draw parameters**:

| Property | Type | Description |
|----------|------|-------------|
| `primitiveType` | `PrimitiveType` | Geometry topology |
| `count` | Expression | Vertex count |
| `first` | Expression | First vertex offset |
| `instanceCount` | Expression | Instance count |
| `baseInstance` | Expression | Base instance offset |
| `patchVertices` | Expression | Tessellation patch size |

**Indexed draw**:

| Property | Type | Description |
|----------|------|-------------|
| `indexBufferBlockId` | ItemId | Index buffer block |
| `baseVertex` | Expression | Base vertex offset |

**Indirect draw**:

| Property | Type | Description |
|----------|------|-------------|
| `indirectBufferBlockId` | ItemId | Indirect command buffer |
| `drawCount` | Expression | Number of indirect draws |

**Compute**:

| Property | Type | Description |
|----------|------|-------------|
| `workGroupsX` | Expression | X work groups |
| `workGroupsY` | Expression | Y work groups |
| `workGroupsZ` | Expression | Z work groups |

**Ray tracing**:

| Property | Type | Description |
|----------|------|-------------|
| `accelerationStructureId` | ItemId | Top-level acceleration structure |

**Texture/Buffer operations**:

| Property | Type | Description |
|----------|------|-------------|
| `textureId` | ItemId | Target texture (clear/copy/swap) |
| `fromTextureId` | ItemId | Source texture (copy) |
| `clearColor` | Color | Clear color value |
| `clearDepth` | Double | Clear depth value |
| `clearStencil` | Int | Clear stencil value |
| `bufferId` | ItemId | Target buffer (clear/copy/swap) |
| `fromBufferId` | ItemId | Source buffer (copy) |

### 4.14 Script

JavaScript file executed during evaluation.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `fileName` | String | — | Script source file path |
| `executeOn` | `ExecuteOn` | `EveryEvaluation` | When to execute |

Scripts share a single JavaScript state per session. Evaluation is sequential, top-to-bottom. Group scopes do **not** isolate script state.

### 4.15 AccelerationStructure

Top-level acceleration structure for ray tracing (Vulkan).

Child items: **Instance** and/or **Geometry** items.

### 4.16 Instance

Ray tracing instance within an AccelerationStructure.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `transform` | Expression | — | 3×4 transformation matrix |

### 4.17 Geometry

Ray tracing geometry within an AccelerationStructure.

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `geometryType` | `GeometryType` | `Triangles` | `Triangles` or `AxisAlignedBoundingBoxes` |
| `vertexBufferBlockId` | ItemId | — | Vertex data block |
| `indexBufferBlockId` | ItemId | — | Index data block |
| `transformBufferBlockId` | ItemId | — | Per-geometry transform block |
| `count` | Expression | — | Geometry count |
| `offset` | Expression | — | Byte offset |

---

## 5. Enumerations — Complete Reference

### 5.1 DataType
`Int8`, `Int16`, `Int32`, `Int64`, `Uint8`, `Uint16`, `Uint32`, `Uint64`, `Float`, `Double`

### 5.2 ShaderType
`Includable`, `Vertex`, `Fragment`, `Geometry`, `TessControl`, `TessEvaluation`, `Compute`, `Task`, `Mesh`, `RayGeneration`, `RayIntersection`, `RayAnyHit`, `RayClosestHit`, `RayMiss`, `RayCallable`

### 5.3 ShaderLanguage
`None`, `GLSL`, `HLSL`, `Slang`

### 5.4 ShaderCompiler
`Driver`, `glslang`, `D3DCompiler`, `DXC`, `Slang`

### 5.5 Renderer
`OpenGL`, `Vulkan`, `Direct3D`

### 5.6 CallType
| Value | Description |
|-------|-------------|
| `Draw` | Standard draw call |
| `DrawIndexed` | Indexed draw call |
| `DrawIndirect` | Indirect draw from buffer |
| `DrawIndexedIndirect` | Indexed indirect draw |
| `DrawMeshTasks` | Mesh shader dispatch |
| `Compute` | Compute shader dispatch |
| `ComputeIndirect` | Indirect compute from buffer |
| `TraceRays` | Ray tracing dispatch (Vulkan) |
| `ClearTexture` | Clear a texture to a color/depth/stencil value |
| `CopyTexture` | Copy texture contents |
| `SwapTextures` | Swap two texture contents (ping-pong) |
| `ClearBuffer` | Clear a buffer |
| `CopyBuffer` | Copy buffer contents |
| `SwapBuffers` | Swap two buffer contents |

### 5.7 ExecuteOn
| Value | When |
|-------|------|
| `ResetEvaluation` | Only on session/shader reset |
| `ManualEvaluation` | On reset or manual trigger (F6) |
| `EveryEvaluation` | Every frame (F7 auto / F8 steady) |

### 5.8 PrimitiveType
`Points`, `LineStrip`, `LineLoop`, `Lines`, `LineStripAdjacency`, `LinesAdjacency`, `TriangleStrip`, `TriangleFan`, `Triangles`, `TriangleStripAdjacency`, `TrianglesAdjacency`, `Patches`

### 5.9 BindingType
`Uniform`, `Sampler`, `Buffer`, `BufferBlock`, `Image`, `TextureBuffer`, `Subroutine`

### 5.10 BindingEditor
`Expression`, `Expression2`, `Expression3`, `Expression4`, `Expression2x2`, `Expression2x3`, `Expression2x4`, `Expression3x2`, `Expression3x3`, `Expression3x4`, `Expression4x2`, `Expression4x3`, `Expression4x4`, `Color`

### 5.11 ComparisonFunc
`NoComparisonFunc`, `LessEqual`, `GreaterEqual`, `Less`, `Greater`, `Equal`, `NotEqual`, `Always`, `Never`

### 5.12 BlendEquation
`Add`, `Min`, `Max`, `Subtract`, `ReverseSubtract`

### 5.13 BlendFactor
`Zero`, `One`, `SrcColor`, `OneMinusSrcColor`, `SrcAlpha`, `OneMinusSrcAlpha`, `DstAlpha`, `OneMinusDstAlpha`, `DstColor`, `OneMinusDstColor`, `SrcAlphaSaturate`, `ConstantColor`, `OneMinusConstantColor`, `ConstantAlpha`, `OneMinusConstantAlpha`, `Src1Alpha`

### 5.14 StencilOperation
`Keep`, `Zero`, `Replace`, `Increment`, `IncrementWrap`, `Decrement`, `DecrementWrap`, `Invert`

### 5.15 FrontFace
`CCW`, `CW`

### 5.16 CullMode
`NoCulling`, `Front`, `Back`, `FrontAndBack`

### 5.17 PolygonMode
`Fill`, `Line`, `Point`

### 5.18 LogicOperation
`NoLogicOperation`, `Copy`, `Clear`, `Set`, `CopyInverted`, `NoOp`, `Invert`, `And`, `Nand`, `Or`, `Nor`, `Xor`, `Equiv`, `AndReverse`, `AndInverted`, `OrReverse`, `OrInverted`

### 5.19 Filter (Sampler)

**Minification**: `Nearest`, `Linear`, `NearestMipMapNearest`, `NearestMipMapLinear`, `LinearMipMapNearest`, `LinearMipMapLinear`

**Magnification**: `Nearest`, `Linear`

### 5.20 WrapMode
`Repeat`, `MirroredRepeat`, `ClampToEdge`, `ClampToBorder`

### 5.21 TextureTarget
`Target1D`, `Target1DArray`, `Target2D`, `Target2DArray`, `Target3D`, `TargetCubeMap`, `TargetCubeMapArray`

### 5.22 GeometryType (Ray Tracing)
`AxisAlignedBoundingBoxes`, `Triangles`

### 5.23 ImageBindingFormat

For compute shader image bindings (the `imageFormat` property on `Binding` when `bindingType: "Image"`):

| Format | Description |
|--------|-------------|
| `Internal` | Infer from texture |
| **R-channel** | `r8`, `r8ui`, `r8i`, `r16`, `r16_snorm`, `r16f`, `r16ui`, `r16i`, `r32f`, `r32ui`, `r32i` |
| **RG-channel** | `rg8`, `rg8_snorm`, `rg8ui`, `rg8i`, `rg16`, `rg16_snorm`, `rg16ui`, `rg16i`, `rg16f`, `rg32f`, `rg32ui`, `rg32i` |
| **RGB-channel** | `rgb32f`, `rgb32i`, `rgb32ui`, `r11f_g11f_b10f` |
| **RGBA-channel** | `rgba8`, `rgba8_snorm`, `rgba8ui`, `rgba8i`, `rgb10_a2`, `rgb10_a2ui`, `rgba16`, `rgba16_snorm`, `rgba16f`, `rgba16ui`, `rgba16i`, `rgba32f`, `rgba32i`, `rgba32ui` |

### 5.24 TextureFormat

Uses Qt's `QOpenGLTexture::TextureFormat` enum names. Key formats:

| Category | Formats |
|----------|---------|
| **Color 8-bit** | `R8_UNorm`, `R8_SNorm`, `RG8_UNorm`, `RG8_SNorm`, `RGB8_UNorm`, `RGB8_SNorm`, `RGBA8_UNorm`, `RGBA8_SNorm` |
| **Color 16-bit** | `R16_UNorm`, `R16_SNorm`, `R16F`, `RG16_UNorm`, `RG16_SNorm`, `RG16F`, `RGB16_UNorm`, `RGB16_SNorm`, `RGB16F`, `RGBA16_UNorm`, `RGBA16_SNorm`, `RGBA16F` |
| **Color 32-bit** | `R32F`, `RG32F`, `RGB32F`, `RGBA32F` |
| **Integer** | `R8I`, `R8U`, `R16I`, `R16U`, `R32I`, `R32U`, `RG8I`, `RG8U`, `RG16I`, `RG16U`, `RG32I`, `RG32U`, `RGB8I`, `RGB8U`, `RGB16I`, `RGB16U`, `RGB32I`, `RGB32U`, `RGBA8I`, `RGBA8U`, `RGBA16I`, `RGBA16U`, `RGBA32I`, `RGBA32U` |
| **SRGB** | `SRGB8`, `SRGB8_Alpha8` |
| **Packed** | `R5G6B5`, `RGBA4`, `RGB5A1`, `RGB10A2`, `RGB9E5`, `RG11B10F` |
| **Depth/Stencil** | `D16`, `D32`, `D32F`, `D24S8`, `D32FS8X24`, `S8` |
| **Compressed** | `RGB_DXT1`, `RGBA_DXT1`, `RGBA_DXT3`, `RGBA_DXT5`, `SRGB_DXT1`, `SRGB_Alpha_DXT1`, `SRGB_Alpha_DXT3`, `SRGB_Alpha_DXT5`, `R_ATI1N_UNorm`, `R_ATI1N_SNorm`, `RG_ATI2N_UNorm`, `RG_ATI2N_SNorm`, `RGB_BP_UNorm`, `SRGB_BP_UNorm`, `RGB_BP_SIGNED_FLOAT`, `RGB_BP_UNSIGNED_FLOAT`, `RGB8_ETC1`, `RGB8_ETC2`, `SRGB8_ETC2`, `RGB8_PunchThrough_Alpha1_ETC2`, `SRGB8_PunchThrough_Alpha1_ETC2`, `RGBA8_ETC2_EAC`, `SRGB8_Alpha8_ETC2_EAC`, `R11_EAC_UNorm`, `R11_EAC_SNorm`, `RG11_EAC_UNorm`, `RG11_EAC_SNorm` |

---

## 6. Evaluation Pipeline

### 6.1 Triggering Evaluation

| Shortcut | Mode | Behavior |
|----------|------|----------|
| **F6** | Manual | Evaluate once. Runs items with `ManualEvaluation` and `ResetEvaluation`. |
| **F7** | Automatic | Re-evaluate whenever session changes (shader edit, binding change, etc.) |
| **F8** | Steady | Continuous evaluation every frame (for animations). Runs `EveryEvaluation` items. |

### 6.2 ExecuteOn Logic

```
ResetEvaluation    → executes only on Reset
ManualEvaluation   → executes on Reset or Manual
EveryEvaluation    → executes always (every frame)
```

### 6.3 Call Evaluation Order

1. All active (`checked: true`) calls are collected top-to-bottom.
2. Scripts execute in document order (no scope isolation).
3. Bindings apply to all calls below them in the same scope.
4. Groups with `iterations > 1` repeat their children N times.
5. Elapsed GPU time per call is measured via timer queries and reported in the Message window.

### 6.4 The checked Property

Each Call and Script has a `checked` boolean (checkbox in the UI). Unchecked items are **completely skipped** — they are not added to the command queue. Used items from the last evaluation are highlighted in the session tree.

---

## 7. Scripting API

GPUpad uses **Qt's QJSEngine** for JavaScript evaluation. Scripts run in a dedicated thread with 5-second timeout protection.

### 7.1 `app` — Application Object

**Properties** (read/write unless noted):

| Property | Type | Description |
|----------|------|-------------|
| `frameIndex` | Number | Current animation frame |
| `frameRate` | Number | Playback rate |
| `time` | Number | Elapsed time (seconds) |
| `timeDelta` | Number (r/o) | Delta since last frame |
| `date` | [Number] (r/o) | `[year, month, day, secondsOfDay]` |
| `session` | Session (r/o) | Session manipulation API |
| `mouse` | Mouse (r/o) | Mouse input state |
| `keyboard` | Keyboard (r/o) | Keyboard input state |

**Methods**:

| Method | Returns | Description |
|--------|---------|-------------|
| `openEditor(fileName, title?)` | Editor | Open file in editor/QML panel |
| `openFileDialog(pattern)` | String? | Native file dialog |
| `loadLibrary(fileName)` | Object | Load JS library or C++ DLL |
| `callAction(id, args...)` | any | Invoke another custom action |
| `enumerateFiles(pattern)` | [String] | Glob file listing (supports `**`) |
| `readTextFile(fileName)` | String? | Read text file contents |
| `writeTextFile(fileName, text)` | Bool | Write text file |
| `writeBinaryFile(fileName, data)` | Bool | Write binary file |

### 7.2 `app.session` — Session Object

**Properties**:

| Property | Type | Description |
|----------|------|-------------|
| `name` | String | Session name |
| `items` | [Item] (r/o) | Top-level items |
| `selection` | [Item] (r/o) | Currently selected items |

**Item Query**:

| Method | Returns | Description |
|--------|---------|-------------|
| `findItem(ident, origin?, subItems?)` | Item? | Find first matching item |
| `findItems(ident, origin?, subItems?)` | [Item] | Find all matching items |
| `getParentItem(ident)` | Item? | Get parent of item |

**Item identifiers** (`ident` parameter) accept:
- **String** — Item name or path (`"groupName/itemName"`)
- **Number** — Item ID
- **Function** — Filter callback `(item) => boolean`
- **Object** — Item object with `.id` property

**Item Manipulation**:

| Method | Returns | Description |
|--------|---------|-------------|
| `insertItem(parent?, object)` | Item | Insert new item as last child |
| `insertItemAfter(sibling, object)` | Item | Insert after sibling |
| `insertItemBefore(sibling, object)` | Item | Insert before sibling |
| `replaceItems(parent, [objects])` | void | Replace all children |
| `clearItems(parent)` | void | Remove all children |
| `deleteItem(ident)` | void | Delete item and children |

**Data Operations**:

| Method | Description |
|--------|-------------|
| `setBufferData(ident, data)` | Set buffer contents (auto-updates backing file) |
| `setBlockData(ident, data)` | Set block region within buffer |
| `setTextureData(ident, data)` | Set texture pixel data |
| `setShaderSource(ident, source)` | Replace shader source text |
| `setScriptSource(ident, source)` | Replace script source text |

**Data** can be: JavaScript arrays `[1, 2, 3]`, typed arrays `new Float32Array(...)`, or library arrays from `loadLibrary`.

**GPU Handles**:

| Method | Returns | Description |
|--------|---------|-------------|
| `getTextureHandle(ident)` | Number (uint64) | Bindless texture handle |
| `getBufferHandle(ident)` | Number (uint64) | Buffer device address |

**Shader Processing**:

| Method | Returns | Description |
|--------|---------|-------------|
| `processShader(ident, type)` | String/Data | Process shader |

`type` values: `"preprocess"`, `"glsl"`, `"hlsl"`, `"spirv"`, `"spirvBinary"`, `"ast"`, `"programBinary"`, `"json"`

**Editor Opening**:

| Method | Returns | Description |
|--------|---------|-------------|
| `openEditor(ident)` | Editor? | Open item in editor (e.g. texture in viewport) |

### 7.3 `app.mouse` — Mouse State

| Property | Type | Description |
|----------|------|-------------|
| `coord` | {x, y} | Pixel coordinates (top-left origin) |
| `fragCoord` | {x, y} | Fragment coordinates (bottom-left origin, for shaders) |
| `prevCoord` | {x, y} | Previous frame pixel coordinates |
| `prevFragCoord` | {x, y} | Previous frame fragment coordinates |
| `delta` | {x, y} | Movement since last frame |
| `buttons` | [State] | Button states: `0`=Up, `1`=Down, `2`=Pressed, `-1`=Released |
| `editorSize` | {x, y} | Viewport dimensions |

### 7.4 `app.keyboard` — Keyboard State

| Property | Type | Description |
|----------|------|-------------|
| `keys` | [State] | Key states: `0`=Up, `1`=Down, `2`=Pressed, `-1`=Released |

### 7.5 Editor Object

Returned by `app.openEditor()`:

| Property | Type | Description |
|----------|------|-------------|
| `fileName` | String (r/o) | Editor file path |
| `viewportSize` | [width, height] | Viewport dimensions (reactive) |

### 7.6 `console` — Logging

| Method | Description |
|--------|-------------|
| `console.log(msg, ...)` | Info message |
| `console.warn(msg, ...)` | Warning message |
| `console.error(msg, ...)` | Error message |

Output appears in the **Message** window.

### 7.7 Expression Bindings

Binding values with `editor: "Expression"` (and variants) are evaluated as JavaScript expressions every frame. Available globals:
- All `app.*` properties and methods
- `Math.*` standard library
- Variables defined by earlier scripts in the session
- `target.viewportSize` for viewport-dependent calculations

Examples:
```javascript
"Math.sin(app.time)"
"app.mouse.fragCoord"
"[Math.cos(app.time), Math.sin(app.time), 0]"
"target.viewportSize[0]"
```

### 7.8 Practical Scripting Patterns

**Library Loading — gl-matrix.js**:
```javascript
app.loadLibrary("gl-matrix.js");
// Global objects now available: glMatrix, mat4, vec3, quat, etc.
const { mat4, vec3 } = glMatrix;
```

**Camera Orbit Pattern** (from Insert Orbit Camera, Tessellation sample):
```javascript
class Camera {
  constructor() {
    this.viewMatrix = mat4.create();
    this.projMatrix = mat4.create();
    this.distance = 3.0;
    this.rotX = 0.3;
    this.rotY = 0.0;
  }
  update(mouse) {
    if (mouse.buttons[0] === 1) {     // Left button held
      this.rotY += mouse.delta.x * 0.01;
      this.rotX += mouse.delta.y * 0.01;
    }
    this.distance *= (1 - mouse.delta.z * 0.001);  // Scroll zoom
    const eye = vec3.fromValues(
      Math.sin(this.rotY) * Math.cos(this.rotX) * this.distance,
      Math.sin(this.rotX) * this.distance,
      Math.cos(this.rotY) * Math.cos(this.rotX) * this.distance
    );
    mat4.lookAt(this.viewMatrix, eye, [0,0,0], [0,1,0]);
  }
}
```

**Keyboard Input Texture** (from Shadertoy 2 sample):
```javascript
// Create 256×3 R8 texture: row0=current, row1=pressed, row2=toggled
var keyboard = app.session.insertItem(parent, {
  name: "Keyboard", type: "Texture", width: 256, height: 3,
  format: "R8_UNorm", target: "Target2D"
});
// Each frame: encode key states into typed array
var data = new Uint8Array(256 * 3);
for (var i = 0; i < 256; i++) {
  var state = app.keyboard.keys[i] || 0;
  data[i]       = (state === 1 || state === 2) ? 255 : 0;  // Held
  data[256 + i] = (state === 2) ? 255 : 0;                 // Just pressed
  data[512 + i] = toggles[i] ? 255 : 0;                    // Toggled
}
app.session.setTextureData(keyboard, data);
```

**Dynamic Session Building** (from Bitonic Sort, Custom Actions samples):
```javascript
// Programmatically build an entire render pipeline
const group = app.session.insertItem(null, { name: "Sort Pipeline", type: "Group" });
const program = app.session.insertItem(group, { name: "BitonicSort", type: "Program" });
app.session.insertItem(program, { name: "sort.comp", type: "Shader", shaderType: "Compute" });
const binding = app.session.insertItem(group, { name: "Bindings", type: "BindingGroup" });
const buffer = app.session.insertItem(group, {
  name: "Data", type: "Buffer", size: N * 4
});
// Add dispatch call with dynamic workgroup sizes
const call = app.session.insertItem(group, {
  name: "Sort", type: "Call", callType: "Compute",
  workGroupsX: Math.ceil(N / 256)
});
```

**Timer/Animation Loop** (from Timer action):
```javascript
// Poll app state at regular intervals from QML
Timer {
  interval: 16; running: true; repeat: true
  onTriggered: {
    timeLabel.text = app.time.toFixed(3)
    fpsLabel.text  = app.frameRate.toFixed(1)
  }
}
```

**Dynamic Item Discovery** (from Sliders action):
```javascript
// Find all Binding items in the session and create UI controls
function collectBindings(items) {
  var result = [];
  for (var item of items) {
    if (item.type === "Binding") result.push(item);
    if (item.items) result.push(...collectBindings(item.items));
  }
  return result;
}
var bindings = collectBindings(app.session.items);
// Create a slider for each binding's value range
```

---

## 8. Custom Actions

### 8.1 Discovery & Location

Actions live in `extra/actions/`. Each is either:
- A **standalone `.js` file** (simplest form)
- A **directory** containing `script.js` (+ optional `ui.qml`, `module.cpp`, `CMakeLists.txt`)

Actions appear in the **Session** menu automatically. The `CustomActions` singleton recursively scans `{AppDir}/actions/` for `*.js` files and subdirectories with `script.js`.

### 8.2 Manifest

Every action declares a manifest:

```javascript
const manifest = {
  name: "&Action Name...",        // Menu text (& = keyboard accelerator)
  applicable: true                // Boolean or function: () => boolean
}
```

When `applicable` is `false` or returns `false`, the menu item is greyed out. The manifest is re-evaluated when the menu opens.

### 8.3 Action Types

| Type | Components | When to Use | Example |
|------|-----------|-------------|---------|
| **Pure JS** | `ActionName.js` | Batch operations, item injection | Compile to SPIR-V, Insert Orbit Camera |
| **JS + QML** | `script.js` + `ui.qml` | Interactive tools with UI panels | Sliders, Timer |
| **JS + QML + C++** | `script.js` + `ui.qml` + `module.cpp` | CPU-heavy operations | GenerateMesh, ImportOBJ |
| **QML + C++ module** | QML + native module | Standalone visual components | NodeGraph |

### 8.4 Script Class Lifecycle

Directory-based actions use a standard class pattern:

```javascript
// script.js — standard lifecycle
class Script {
  constructor() {
    // Load C++ libraries, initialize state
    this.library = app.loadLibrary("ModuleName");
  }

  initializeUi(ui) {
    // Called after QML loads — wire UI to script
    this.ui = ui;
    // Populate combo boxes, set defaults
    const typeCount = this.library.getTypeCount();
    ui.typeNames = Array.from({length: typeCount},
      (_, i) => this.library.getTypeName(i));
  }

  refresh() {
    // Called on UI parameter change — update preview
    const settings = this.getSettings();
    this.library.setSettings(this.model, JSON.stringify(settings));
  }

  insert() {
    // Main action — create session items, populate data
    this.group = app.session.insertItem(null, {
      name: 'Generated', type: 'Group'
    });
    // Create Buffer → Block → Field hierarchy...
    const vertices = this.library.getVertices(this.geometry);
    app.session.setBlockData(this.block, vertices);
  }
}

this.script = new Script();
app.openEditor("ui.qml", manifest.name);
```

**Key pattern**: `this.script = new Script()` makes the instance globally available to QML as `script.*`.

### 8.5 QML UI Patterns

```qml
// ui.qml — standard structure for interactive actions
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

ScrollView {
  // Properties bridged to JavaScript via script.ui.*
  property alias typeIndex: typeCombo.currentIndex
  property alias fileName: fileField.text
  property alias indexed: indexedCheck.checked

  GridLayout {
    columns: 2

    Label { text: "Type:" }
    ComboBox {
      id: typeCombo
      onActivated: script.refresh()       // JS callback on change
    }

    Label { text: "File:" }
    RowLayout {
      TextField { id: fileField }
      Button {
        text: "Browse"
        onClicked: {
          var result = app.openFileDialog("*.obj")
          if (result) fileField.text = result
        }
      }
    }

    CheckBox {
      id: indexedCheck; checked: true
      onToggled: script.refresh()
    }

    Button {
      text: "Insert"
      onClicked: script.insert()          // Main action trigger
    }
  }
}
```

**Available QML modules**: `QtQuick 2.12`, `QtQuick.Controls 2.12`, `QtQuick.Layouts 1.12`, `QtQuick.Shapes 1.15`. The QML context shares the script engine, so all `script.*` methods and `app.*` globals are directly callable.

**JS↔QML data flow**:
1. QML `property alias` → readable/writable from JS as `this.ui.propertyName`
2. QML `onActivated`/`onClicked` → calls JS methods directly (`script.refresh()`)
3. JS `app.openEditor("ui.qml", title)` → opens QML panel as dockable editor tab
4. JS can read QML state: `this.ui.typeIndex`, `this.ui.fileName`

### 8.6 C++ Plugin Pattern (DLLREFLECT)

C++ plugins use the `dllreflect.h` macro system to export functions:

```cpp
// module.cpp
#include "dllreflect.h"

// Functions receive/return: std::string, int, size_t, float, double,
// std::vector<float>, std::vector<uint32_t>, and opaque struct handles

struct Model { /* internal state */ };

Model loadFile(const std::string& filename) noexcept { /* parse file */ }
std::string getError(const Model& model) { return model.error; }
void setSettings(Model& model, const std::string& json) { /* apply settings */ }
std::vector<float> getVertices(const Model& model) { /* interleaved vertex data */ }
std::vector<uint32_t> getIndices(const Model& model) { /* index buffer */ }
int getShapeCount(const Model& model) { return model.shapes.size(); }

DLLREFLECT_BEGIN()
DLLREFLECT_FUNC(loadFile)
DLLREFLECT_FUNC(getError)
DLLREFLECT_FUNC(setSettings)
DLLREFLECT_FUNC(getVertices)
DLLREFLECT_FUNC(getIndices)
DLLREFLECT_FUNC(getShapeCount)
DLLREFLECT_END()
```

```javascript
// script.js — loading and using the plugin
var lib = app.loadLibrary("ModuleName");
var model = lib.loadFile(fileName);       // Returns opaque handle
var error = lib.getError(model);          // String or empty
lib.setSettings(model, JSON.stringify(settings));  // JSON → C++
var verts = lib.getVertices(model);       // Returns typed array
app.session.setBlockData(block, verts);   // Upload to GPU
```

**Key rules**: C++ plugins have **no GPU context**. All GPU work goes through `app.session.*` APIs. Data passes as JSON strings (settings) or typed arrays (geometry). Opaque struct handles can be passed back to other plugin functions.

### 8.7 Action Composition

Actions can invoke other actions:
```javascript
app.callAction("GenerateMesh", { type: "Sphere", slices: 32 });
app.callAction("ImportOBJ", { fileName: "model.obj", normalize: true });
```

The Custom Actions sample demonstrates cycling between mesh types:
```javascript
// Dynamically switch between imported OBJ and generated primitives
const meshTypes = ["Cylinder", "Icosahedron", "Dodecahedron"];
for (const type of meshTypes) {
  app.callAction("GenerateMesh", { type });
}
```

### 8.8 Available Support Libraries

**gl-matrix.js** (v3.4.0) — bundled at `extra/libs/gl-matrix.js`:
```javascript
app.loadLibrary("gl-matrix.js");
// Exposes global: glMatrix, mat2, mat2d, mat3, mat4, quat, quat2, vec2, vec3, vec4
const mat4 = glMatrix.mat4;
const view = mat4.lookAt(mat4.create(), eye, center, up);
const proj = mat4.perspective(mat4.create(), fov, aspect, near, far);
```

Used by: Insert Orbit Camera, Custom Actions sample, Tessellation sample, Ray Tracing samples.

### 8.9 Inspector Action — Deep Dive

The **Inspector** is the most complex action — a GPU-resident shader variable visualizer with 3-stage pipeline:

**Architecture**: Expression Render → Histogram Compute → Composite Display

**Stage 1 — Expression Render (RGBA32F)**:
- User enters a GLSL expression (e.g., `normalize(vNormal)`, `gl_FragDepth`, `abs(color.rgb)`)
- Type inference engine determines result type via regex (constructors, swizzles, builtins)
- Fragment shader is rewritten: `main()` body replaced with `outColor = coerce_to_vec4(expr);`
- Coercion rules: `float→vec4(v,v,v,1)`, `vec2→vec4(v,0,1)`, `vec3→vec4(v,1)`, `mat→vec4(col0)`
- Printf injection: `#if defined(GPUPAD) printf("val=%v4f", result); #endif`
- All in-scope bindings from the original call are forwarded to the inspector group

**Stage 2 — Histogram Compute (128-bin SSBO)**:
```glsl
// histogram.comp — 16×16 workgroups
layout(std430) buffer HistogramSSBO {
  uint binsR[128], binsG[128], binsB[128], binsA[128];
  uint dataMin, dataMax, autoMin, autoMax;
  uint totalPixels;
};
// Float→sortable-uint for atomicMin/Max comparison
uint floatToSortableUint(float f) { ... }
// Per-pixel: classify into bin, atomicAdd, atomicMin/Max
```

**Stage 3 — Composite Display (sRGB)**:
- Mapping modes: Linear (clamp), Sigmoid (exp±8), Log2(1+t×255)
- Channel mask: RGBA toggles for isolating channels
- Out-of-range highlighting: Magenta (below) / Cyan (above) checkerboard
- Histogram overlay: 128 bins, normalized bars, range indicators
- Crosshair: Inverted color at mouse position

**QML UI**: Expression input + history, target call selector, mapping mode/range controls, channel checkboxes, histogram height slider.

### 8.10 NodeGraph — Reusable QML Component Library

The **NodeGraph** action provides a standalone visual node-graph editor as a QML module:

```qml
// qmldir
module NodeGraph
NodeGraph 1.0 NodeGraph.qml
```

**Component hierarchy**: `NodeGraph` (root) → `Node` (draggable container) → `Attribute` (row with sockets) → `Socket` (input/output connector) → `Link`/`Cable` (visual connection).

**Key APIs**:
```javascript
graph.addNode({ name: "Transform", x: 100, y: 50 })
graph.addLink(fromAttribute, toAttribute)
graph.canLink(fromAttr, toAttr)  // Validation: different nodes, available input
graph.removeNode(node)           // Cascading link cleanup
graph.selectedNodes              // Multi-selection support
```

**Selection**: Ctrl+click toggle, rectangle selection, multi-node drag. This is a pure UI component — it does **not** interact with `app.session` or rendering.

---

## 9. Built-in Shader Features

### 9.1 Printf Debugging

GPUpad automatically injects a `printf()` function into shaders. Use with a preprocessor guard:

```glsl
#if defined(GPUPAD)
  if (gl_FragCoord.xy == uMouseFragCoord)
    printf("value = %f, color = %v4f", myValue, myColor);
#endif
```

**How it works**:
1. Source is scanned for `printf()` calls.
2. Each call is rewritten to `_printfBegin(formatIdx, argCount), _printf(arg1), ...`
3. An SSBO (`_printfBuffer`) with atomic operations collects output.
4. After rendering, the buffer is read back and formatted messages appear in the **Message** window.

**Supported format specifiers**: `%d`, `%u`, `%f`, `%e`, `%v2f`, `%v3f`, `%v4f`, `%v2d`, etc. (vector types use `v<N><type>` syntax).

### 9.2 Predefined Macros

| Macro | Value | Description |
|-------|-------|-------------|
| `GPUPAD` | `1` | Always defined when compiling in GPUpad |
| `GPUPAD_OPENGL` | `1` | When using OpenGL renderer |
| `GPUPAD_GLSLANG` | `1` | When using glslang compiler |
| `GPUPAD_D3DCOMPILER` | `1` | When using D3DCompiler |
| `GPUPAD_DXC` | `1` | When using DXC compiler |
| `GPUPAD_SLANG` | `1` | When using Slang compiler |

### 9.3 Shader Preamble

The session-level `shaderPreamble` string is prepended to every shader. Per-shader `preamble` strings are also supported. Common uses:
- Version declarations: `#version 460`
- Feature defines: `#define PASS 0`
- Extension enables: `#extension GL_EXT_nonuniform_qualifier : enable`

### 9.4 Includable Shaders

Shaders with `shaderType: "Includable"` are not compiled as pipeline stages. They serve as shared include files that can be `#include`d by other shaders in the same program.

---

## 10. Sample Sessions Catalogue

All samples live in `extra/samples/GLSL/`. They can be opened from the **Help** menu and serve as templates (Save As copies all dependencies).

### 10.1 Atomic Counters

**Purpose**: Atomic operations on counters using SSBO with atomic functions.

| Feature | Detail |
|---------|--------|
| Calls | `ClearBuffer`, `Compute` |
| Bindings | `Buffer`, `TextureBuffer` |
| Format | `rgba8ui` (texture buffer) |
| Compute | 16×1×1 work groups |

Demonstrates: atomic counter buffers, SSBO atomic operations, TextureBuffer bindings.

### 10.2 Bindless Texture

**Purpose**: Dynamic texture access without traditional binding points using uint64 GPU handles.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture`, `Draw` |
| Bindings | `BufferBlock` (Uint64 handles) |
| Script | `setHandles.js` — populates handles via `getTextureHandle()` |

Demonstrates: bindless texture handles, BufferBlock bindings, script-driven GPU handle management.

### 10.3 Bitonic Sort

**Purpose**: Parallel bitonic sorting of 262,144 elements using compute shaders.

| Feature | Detail |
|---------|--------|
| Calls | `Compute` (FillRandom) |
| Buffer | 262,144 rows of Uint32 |
| Script | Dynamic multi-pass compute call insertion |

Demonstrates: large-scale compute, dynamic session manipulation from scripts, expression-based work groups.

### 10.4 Buffer Reference

**Purpose**: Vulkan buffer device addresses for pointer-like GPU memory access.

| Feature | Detail |
|---------|--------|
| Renderer | **Vulkan** |
| Bindings | `Buffer` with Uint64 device addresses |
| Script | `setHandles.js` — populates via `getBufferHandle()` |

Demonstrates: Vulkan-specific buffer references, push constants, device address pointers.

### 10.5 Compute (Game of Life)

**Purpose**: Conway's Game of Life using compute shaders with ping-pong texture swapping.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture`, `Compute`, `SwapTextures` |
| Bindings | `Image` (read + write) |
| Textures | Two textures for ping-pong |

Demonstrates: compute shaders, image load/store, ping-pong pattern via `SwapTextures`.

### 10.6 Cube

**Purpose**: Classic 3D textured cube with depth testing and matrix transforms.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture` (color + depth), `Draw` |
| Target | Depth: `D16`, Color: `SRGB8_Alpha8` |
| Stream | Position, Normal, TexCoord attributes |
| Bindings | Model/View/Projection matrices, Sampler |

Demonstrates: standard 3D pipeline, depth testing, SRGB rendering, anisotropic filtering, vertex streams.

### 10.7 Custom Actions

**Purpose**: Interactive 3D model viewer showcasing custom action integration.

| Feature | Detail |
|---------|--------|
| Scripts | `ManualEvaluation` + `ResetEvaluation` |
| Actions | `app.callAction()` for GenerateMesh/ImportOBJ |

Demonstrates: manual script execution, orbit camera, action composition.

### 10.8 glTF Viewer

**Purpose**: PBR (Physically Based Rendering) viewer with IBL (Image-Based Lighting).

| Feature | Detail |
|---------|--------|
| Textures | 11 (cubemaps, albedo, normal, metalRoughness, AO, emissive) |
| Target | MSAA (`samples: 4`) |
| Calls | `DrawIndexed` |
| Groups | Nested: DamagedHelmet, Environment, Matrices, Material, Lights |
| Bindings | 25+ (uniforms, samplers, array uniforms) |

Demonstrates: indexed drawing, MSAA, cubemap textures, nested groups, extensive uniform arrays, PBR/IBL pipeline.

### 10.9 Indirect

**Purpose**: GPU-generated draw commands with indirect rendering.

| Feature | Detail |
|---------|--------|
| Calls | `Compute`, `ComputeIndirect`, `ClearTexture`, `DrawIndirect` |
| Buffer | ComputeCommand + DrawCommand blocks at different offsets |
| Target | `polygonMode: "Line"` |

Demonstrates: indirect compute dispatch, indirect drawing, multi-block buffers with offsets, wireframe rendering.

### 10.10 Instancing

**Purpose**: GPU instancing with per-instance attributes.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture`, `Draw` |
| Stream | Attributes with `divisor: 0` (per-vertex) and `divisor: 1` (per-instance) |
| Target | Alpha blending (`SrcAlpha`, `OneMinusSrcAlpha`) |
| Call | `instanceCount: 4` |

Demonstrates: instanced rendering, per-instance attributes via divisor, alpha blending.

### 10.11 Javascript Library (Delaunay)

**Purpose**: JavaScript-driven Delaunay triangulation using an external library.

| Feature | Detail |
|---------|--------|
| Scripts | `delaunay.js` (library), `generate.js` (mesh generation) |
| Calls | `DrawIndexed` |
| Buffer | Vertices (Float×2) + Indices (Uint16) |
| Target | `polygonMode: "Line"` |

Demonstrates: `app.loadLibrary()`, JS mesh generation, indexed wireframe drawing, script-driven buffer population.

### 10.12 Mesh Shader

**Purpose**: NVIDIA mesh shader extension for GPU-driven vertex generation.

| Feature | Detail |
|---------|--------|
| Calls | `DrawMeshTasks` (1×1×1 work groups) |
| Program | Mesh + Fragment shaders |
| Extension | `GL_NV_mesh_shader` |

Demonstrates: mesh task dispatch, GPU-side vertex/primitive generation without vertex streams.

### 10.13 Nonuniform Indexing

**Purpose**: Descriptor indexing with dynamic texture array access.

| Feature | Detail |
|---------|--------|
| Renderer | **Vulkan** |
| Bindings | `Sampler` array (`uTexture[0]`, `uTexture[1]`) |
| Extension | `GL_EXT_nonuniform_qualifier` |

Demonstrates: nonuniform descriptor indexing, dynamic texture selection in shaders.

### 10.14 Order-Independent Transparency (OIT)

**Purpose**: Linked-list based order-independent transparency.

| Feature | Detail |
|---------|--------|
| Calls | `ClearBuffer`, `ClearTexture`, `Draw` (2 passes) |
| Textures | R32U heads pointer texture |
| Buffer | Fragment linked-list SSBO |
| Bindings | `Image`, `Buffer` |
| Groups | Organized (Targets, Buffers, Programs, Bindings, Calls) |
| Stream | Instanced (`divisor: 1`, 100 particles) |

Demonstrates: atomic image operations, SSBO linked lists, multi-pass OIT, instanced rendering, group organization.

### 10.15 Paint

**Purpose**: Interactive painting application with mouse input.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture` (ResetEvaluation), `Draw` (EveryEvaluation) |
| Bindings | `app.mouse.coord`, `app.mouse.prevCoord`, `app.mouse.buttons[0]` |
| Target | Alpha blending |

Demonstrates: real-time mouse input, persistent render target (clear only on reset), brush rendering.

### 10.16 Particles

**Purpose**: GPU-accelerated particle system with compute + rasterization.

| Feature | Detail |
|---------|--------|
| Programs | Reset (Compute), Update (Compute), Render (VS+FS) |
| Shaders | `Includable` shared particle definition |
| Calls | `Compute` (reset + update), `ClearTexture`, `Draw` (points) |
| Bindings | `Expression2` for animation, `Buffer` for particle data |
| Target | Additive blending (`SrcAlpha` + `One`) |

Demonstrates: multi-stage compute pipeline, includable shader modules, expression-based work groups, additive blending, point rendering.

### 10.17 Printf

**Purpose**: Per-pixel shader printf debugging.

| Feature | Detail |
|---------|--------|
| Binding | `uMouseFragCoord: "app.mouse.fragCoord"` |
| Shader | `#if defined(GPUPAD)` + `printf()` |

Demonstrates: built-in printf debugging, mouse fragment coordinate tracking, conditional per-pixel output.

### 10.18 Quad

**Purpose**: Basic textured quad rendering.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture`, `Draw` |
| Bindings | `Sampler` (Nearest filter, Repeat wrap), `Uniform` (color) |
| No Stream | Attributeless fullscreen quad (4 vertices) |

Demonstrates: minimal texture sampling setup, attributeless rendering.

### 10.19 Ray Tracing In Vulkan (Procedural)

**Purpose**: Hardware ray tracing with procedural AABB sphere geometry.

| Feature | Detail |
|---------|--------|
| Renderer | **Vulkan** |
| Calls | `TraceRays` |
| Program | RayGeneration + RayMiss + RayClosestHit + RayIntersection |
| AccelerationStructure | AABB geometry type |
| Buffers | Spheres, Materials, AABBs |
| Script | `generateSpheres.js`, `camera.js` |

Demonstrates: procedural ray intersection, path tracing, depth of field, accumulation buffer, AABB acceleration structure.

### 10.20 Ray Tracing In Vulkan 2 (Mesh)

**Purpose**: Hardware ray tracing with triangle mesh geometry.

| Feature | Detail |
|---------|--------|
| Renderer | **Vulkan** |
| Calls | `TraceRays` |
| Program | RayGeneration + RayMiss + RayClosestHit |
| AccelerationStructure | Triangle geometry type |
| Bindings | `BufferBlock` arrays, `Sampler` with anisotropic |
| Script | `generateScene.js`, `camera.js` |

Demonstrates: triangle mesh acceleration structure, texture mapping in ray tracing, buffer block arrays, dynamic scene generation.

### 10.21 Shadertoy

**Purpose**: Basic Shadertoy-compatible shader rendering.

| Feature | Detail |
|---------|--------|
| Script | Per-frame uniform updates |
| Bindings | `iResolution`, `iFrame`, `iChannel0-3` (Samplers) |
| Calls | `Draw` (TriangleStrip, 4 vertices) |

Demonstrates: Shadertoy uniform conventions, multi-channel sampler bindings, script-driven updates.

### 10.22 Shadertoy 2

**Purpose**: Advanced multi-pass Shadertoy with buffer feedback and keyboard input.

| Feature | Detail |
|---------|--------|
| Calls | `ClearTexture`, `Draw`, `CopyTexture` |
| Textures | RGBA32F buffers, RGBA16F cubemap, R8_UNorm keyboard texture |
| Groups | Nested buffer passes |
| Shader | `preamble: "#define PASS 0"` for multi-pass selection |

Demonstrates: multi-pass feedback loops, `CopyTexture`, keyboard texture input, shader preambles, cubemap sampling.

### 10.23 Sliders

**Purpose**: Interactive parameter control via expression bindings.

| Feature | Detail |
|---------|--------|
| Bindings | `Expression`, `Expression3` editors |
| Values | `app.frameIndex`, `app.time`, `[0.283, 0.511, 0.61]` |

Demonstrates: live expression evaluation, multi-component expression editors, the Sliders custom action companion.

### 10.24 Stencil Buffer

**Purpose**: Two-pass stencil mask + fill rendering.

| Feature | Detail |
|---------|--------|
| Textures | `RGBA8_UNorm` (color), `D24S8` (depth-stencil) |
| Targets | 2 (mask generation + masked fill) |
| Programs | 2 (mask writer + fill renderer) |
| Stencil | Front: `Always`→`Increment` (write), `NotEqual`→reference=0 (read) |

Demonstrates: stencil operations (read/write masks, comparison, increment), multi-pass rendering, D24S8 format.

### 10.25 Subroutine

**Purpose**: GLSL subroutine for runtime shader function selection.

| Feature | Detail |
|---------|--------|
| Binding | `bindingType: "Subroutine"`, `subroutine: "fade1"` |

Demonstrates: `Subroutine` binding type, polymorphic shader functions without recompilation.

### 10.26 Sync Test

**Purpose**: GPU synchronization and timing verification.

| Feature | Detail |
|---------|--------|
| Bindings | Manual resolution (`[1920, 1080]`), `app.frameIndex` |

Demonstrates: fixed-resolution rendering independent of viewport, frame indexing.

### 10.27 Tessellation

**Purpose**: Full GPU tessellation pipeline with control, evaluation, and geometry stages.

| Feature | Detail |
|---------|--------|
| Program | Vertex + TessControl + TessEvaluation + Geometry + Fragment |
| Calls | `DrawIndexed` with `primitiveType: "Patches"` |
| Call | `patchVertices: "3"` |
| Buffer | Binary mesh files (`geodesic_positions.bin`, `geodesic_indices.bin`) |
| Bindings | `TessLevelInner: 5`, `TessLevelOuter: 4`, matrix math expressions |

Demonstrates: complete tessellation pipeline, patch primitives, binary buffer files, complex JS matrix expressions.

### 10.28 Uniforms

**Purpose**: Comprehensive test of all uniform binding patterns.

| Feature | Detail |
|---------|--------|
| Call | `Compute` (1×1×1) |
| Groups | 4 (whole uniforms, elements, blocks, block arrays) |
| Editors | `Expression`, `Expression2`, `Expression2x2`, `Expression2x3`, `Expression3`, `Expression3x3`, `Color` |
| Paths | `u_values_1[0].y[0].z`, `u_block.values_1[1].y[1].w` |

Demonstrates: every binding editor type, nested uniform paths, array-of-blocks indexing, uniform block members.

### 10.29 Video

**Purpose**: Video file playback as a texture source.

| Feature | Detail |
|---------|--------|
| Texture | `fileName: "robin.mp4"` |
| Sampler | `MirroredRepeat` wrap, `LinearMipMapLinear` minification |
| Binding | `uTime: "(Date.now() % 100000) / 1000"` |

Demonstrates: video texture source, mirrored repeat wrapping, JS-computed time uniform.

### 10.30 Volume

**Purpose**: 3D texture manipulation via compute shader.

| Feature | Detail |
|---------|--------|
| Texture | `target: "Target3D"`, `depth: "64"`, `format: "R8_UNorm"` |
| Binding | `Image` with `layer: -1` (all layers) |
| Call | `Compute` with 8×8×64 work groups |

Demonstrates: 3D texture target, volume compute dispatch, all-layer image binding.

### 10.31 HLSL Samples

All HLSL samples live in `extra/samples/HLSL/` and use the **Direct3D** renderer backend.

**HLSL Cube** — equivalent to GLSL Cube but with HLSL-specific patterns:
| Feature | Detail |
|---------|--------|
| Renderer | **Direct3D** |
| Target | `flipViewport: true`, `reverseCulling: true` (D3D clip-space conventions) |
| Shaders | `VS()` / `PS()` explicit entry points |
| Bindings | `ConstantBuffer<T>` instead of uniform blocks, `SamplerState` separate from `Texture2D` |
| Buffer | `DXGI_FORMAT_*` style formats |

**HLSL Uniforms** — equivalent to GLSL Uniforms for compute:
| Feature | Detail |
|---------|--------|
| Call | `Compute` (1×1×1) |
| Bindings | Same exhaustive uniform path tests as GLSL Uniforms |
| Syntax | `cbuffer`, `StructuredBuffer`, `RWStructuredBuffer` |

**GLSL → HLSL key differences**:
| Aspect | GLSL | HLSL |
|--------|------|------|
| Renderer | OpenGL/Vulkan | Direct3D |
| Target | Default | `flipViewport: true, reverseCulling: true` |
| Uniform blocks | `layout(std140) uniform UBO { }` | `ConstantBuffer<T>` or `cbuffer` |
| Samplers | `uniform sampler2D s` | `Texture2D tex; SamplerState samp` (separate) |
| Entry point | `void main()` | `PSOutput PS(VSOutput input)` (named) |
| Vertex ID | `gl_VertexID` | `SV_VertexID` |
| Fragment coord | `gl_FragCoord` | `SV_Position` |
| Image | `layout(rgba8) image2D` | `RWTexture2D<float4>` |
| Compute SSBO | `layout(std430) buffer { }` | `RWStructuredBuffer<T>` |

### 10.32 GPU Technique Patterns (Cross-Sample Reference)

**Atomic Image Operations** (OIT sample):
```glsl
// Fragment linked list: per-pixel head pointer + append to SSBO
layout(r32ui) uniform uimage2D uHeadPointers;
layout(std430) buffer FragList { Fragment fragments[]; };
uint newIdx = atomicAdd(fragmentCount, 1);
uint oldHead = imageAtomicExchange(uHeadPointers, ivec2(gl_FragCoord.xy), newIdx);
fragments[newIdx] = Fragment(color, depth, oldHead);
```

**Shared Memory Compute** (Bitonic Sort):
```glsl
shared uint sharedData[BLOCK_SIZE];  // Workgroup-local memory
// Load → barrier → compare-swap → barrier → store
sharedData[gl_LocalInvocationIndex] = data[gl_GlobalInvocationID.x];
barrier();
for (uint k = 2; k <= BLOCK_SIZE; k <<= 1) {
  for (uint j = k >> 1; j > 0; j >>= 1) {
    uint partner = gl_LocalInvocationIndex ^ j;
    if (partner > gl_LocalInvocationIndex) {
      if ((gl_LocalInvocationIndex & k) == 0)
        atomicMin(sharedData, ...);  // Ascending
    }
    barrier();
  }
}
```

**Accumulation Buffer** (Ray Tracing samples):
```glsl
// Progressive path tracing with frame accumulation
vec4 prev = imageLoad(accumBuffer, ivec2(gl_LaunchIDEXT.xy));
vec4 curr = traceNewSample();
float weight = 1.0 / float(frameIndex + 1);
imageStore(accumBuffer, ivec2(gl_LaunchIDEXT.xy), mix(prev, curr, weight));
```

**Volume Distance Field** (Volume sample):
```glsl
// 3D SDF computation in compute shader
layout(r8) writeonly uniform image3D uVolume;
vec3 p = vec3(gl_GlobalInvocationID) / volumeSize;
float d = sdfSphere(p, center, radius);
d = min(d, sdfBox(p, boxMin, boxMax));
imageStore(uVolume, ivec3(gl_GlobalInvocationID), vec4(d));
```

**Multi-Pass with Preamble Defines** (Shadertoy 2):
```json
{ "type": "Shader", "preamble": "#define PASS 0" }
{ "type": "Shader", "preamble": "#define PASS 1" }
```
Same shader file compiled with different defines for buffer A/B/main passes.

### 10.33 Cross-Sample Feature Matrix

| Feature | Samples Using It |
|---------|-----------------|
| Compute shaders | Atomic Counters, Bitonic Sort, Compute, Indirect, Particles, Uniforms, Volume |
| Image load/store | Compute, OIT, Particles, Ray Tracing, Volume |
| SSBO / Buffer | Atomic Counters, Bitonic Sort, Buffer Reference, OIT, Particles |
| Instancing | Instancing, OIT |
| Indexed drawing | Cube, glTF Viewer, Javascript Library, Tessellation |
| Indirect dispatch/draw | Indirect |
| Tessellation | Tessellation |
| Mesh shaders | Mesh Shader |
| Ray tracing | Ray Tracing, Ray Tracing 2 |
| Stencil ops | Stencil Buffer |
| Mouse input | Paint, Printf, Shadertoy |
| Keyboard input | Shadertoy 2 |
| Script automation | Bindless Texture, Bitonic Sort, Buffer Reference, Custom Actions, Ray Tracing, Shadertoy |
| Video textures | Video |
| MSAA | glTF Viewer |
| Cubemaps | glTF Viewer, Shadertoy 2 |
| 3D textures | Volume |
| Alpha blending | Instancing, OIT, Paint, Particles |
| Ping-pong swap | Compute, Shadertoy 2 |
| Vulkan-only | Buffer Reference, Nonuniform Indexing, Ray Tracing, Ray Tracing 2 |
| HLSL / Direct3D | HLSL Cube, HLSL Uniforms |

---

## 11. Existing Custom Actions Catalogue

Actions in `extra/actions/`:

### 11.1 Compile all shader files to Spir-V

**Type**: Pure JS | **File**: `Compile all shader files to Spir-V.js`

Iterates all Shader items, calls `session.processShader(shader, "spirvBinary")`, writes `.spv` files.

**API used**: `findItems()`, `processShader()`, `writeBinaryFile()`

### 11.2 GenerateMesh

**Type**: C++ + JS + QML | **Directory**: `GenerateMesh/`

Procedural mesh generator (16 types: cube, cylinder, cone, torus, sphere variants, klein bottle, trefoil knot, hemisphere, plane, polyhedra, rock). C++ uses `par_shapes` library.

**API used**: `loadLibrary()`, `insertItem()`, `setBlockData()`, `replaceItems()`, `deleteItem()`

### 11.3 ImportOBJ

**Type**: C++ + JS + QML | **Directory**: `ImportOBJ/`

Wavefront OBJ importer with vertex deduplication, normal generation, transformations (normalize, center, swap Y/Z). C++ uses `rapidobj`.

**API used**: `loadLibrary()`, `openFileDialog()`, `insertItem()`, `setBlockData()`, `replaceItems()`

### 11.4 Import glTF

**Type**: Pure JS | **File**: `Import_glTF.js` | **Status**: Disabled (`applicable: false`)

Parses glTF 2.0 JSON, maps accessors to Buffers/Streams, extracts textures/materials.

**API used**: `openFileDialog()`, `readTextFile()`, `insertItem()`, `findItem()`

### 11.5 Insert Orbit Camera

**Type**: Pure JS | **File**: `Insert Orbit Camera.js`

Creates an interactive orbit camera controller (left-drag rotate, right-drag zoom). Inserts a Script item with embedded camera code + view matrix Binding.

**API used**: `loadLibrary()` (gl-matrix), `insertItem()`, `setScriptSource()`

### 11.6 Inspector

**Type**: JS + QML + GLSL | **Directory**: `Inspector/`

Real-time GLSL expression debugger. Rewrites fragment shaders to visualize any expression. GPU-resident pipeline: expression render → histogram compute → composite display with mapping modes (Linear/Sigmoid/Log), channel toggles, OOR highlighting, per-pixel printf inspection.

**API used**: `findItems()`, `insertItem()`, `setShaderSource()`, `readTextFile()`, `deleteItem()`, `getParentItem()`, `openEditor()`

### 11.7 NodeGraph

**Type**: QML + C++ module | **Directory**: `NodeGraph/`

Visual node-graph editor with drag-and-drop nodes, attribute connections, and procedural graph layout. Uses a custom C++ `NodeGraph 1.0` QML module.

**API used**: `openEditor()`

### 11.8 Sliders

**Type**: JS + QML | **Directory**: `Sliders/`

Auto-generates slider controls for all Binding uniform values in the session.

**API used**: `findItems()`, `findItem()`, `openEditor()`

### 11.9 Timer

**Type**: JS + QML | **Directory**: `Timer/`

Displays `app.time`, `app.timeDelta`, `app.frameRate`, and `app.date` in a dockable panel with 16ms polling.

**API used**: `app.frameRate`, `app.time`, `app.timeDelta`, `app.date`, `openEditor()`

---

## 12. Extra Directory — Themes, Libraries & Packaging

### 12.1 Color Themes (`extra/themes/`)

GPUpad supports custom editor themes using the **Base16** color scheme format (YAML):

```yaml
scheme: "Theme Name"
author: "Author Name"
base00: "1d1f21"    # Default Background
base01: "282a2e"    # Lighter Background (status bars, line highlights)
base02: "373b41"    # Selection Background
base03: "969896"    # Comments, Invisibles, Line Highlighting
base04: "b4b7b4"    # Dark Foreground (status bars)
base05: "c5c8c6"    # Default Foreground, Caret, Delimiters
base06: "e0e0e0"    # Light Foreground (not often used)
base07: "ffffff"    # Light Background (not often used)
base08: "cc6666"    # Variables, XML Tags, Markup Link Text, Diff Deleted
base09: "de935f"    # Integers, Boolean, Constants, Markup Link URL
base0A: "f0c674"    # Classes, Markup Bold, Search Text Background
base0B: "b5bd68"    # Strings, Inherited Class, Markup Code
base0C: "8abeb7"    # Support, Regular Expressions, Escape Characters
base0D: "81a2be"    # Functions, Methods, Attribute IDs
base0E: "b294bb"    # Keywords, Storage, Selector
base0F: "a3685a"    # Deprecated, Embedded Language Tags
```

Themes are loaded from `extra/themes/` at startup. Over 200 themes are bundled (Solarized, Dracula, Monokai, Gruvbox, Nord, Tomorrow Night, etc.).

### 12.2 JavaScript Libraries (`extra/libs/`)

**gl-matrix.js** (v3.4.0) — the bundled matrix/vector math library:
- **Format**: UMD module (works in QJSEngine via `app.loadLibrary()`)
- **Exports**: `glMatrix`, `mat2`, `mat2d`, `mat3`, `mat4`, `quat`, `quat2`, `vec2`, `vec3`, `vec4`
- **Usage**: Camera controllers, matrix uniforms, scene generation
- All types use `Float32Array` backing (compatible with `setBlockData()`)

Loading pattern:
```javascript
app.loadLibrary("gl-matrix.js");
const { mat4, vec3 } = glMatrix;
```

### 12.3 QML Imports (`extra/qml/`)

`imports.qml` documents the available Qt Quick modules in the scripting environment:

| Module | Version | Use Case |
|--------|---------|----------|
| `QtQuick` | 2.12 | Core items, animations, layouts |
| `QtQuick.Controls` | 2.12 | Buttons, sliders, combo boxes, text fields |
| `QtQuick.Layouts` | 1.12 | GridLayout, RowLayout, ColumnLayout |
| `QtQuick.Shapes` | 1.15 | Vector graphics, paths, gradients |

Custom QML modules (like `NodeGraph 1.0`) use `qmldir` manifests in their directories.

### 12.4 Linux Desktop Integration (`extra/share/`)

- `applications/gpupad.desktop` — freedesktop `.desktop` entry (Name, Exec, Icon, Categories, MimeType for `.gpjs`)
- `metainfo/gpupad.metainfo.xml` — AppStream metadata (description, screenshots, content rating, releases)

### 12.5 Windows Installer (`extra/wix/`)

WiX-based MSI installer template with:
- Product GUID, component registration
- File associations for `.gpjs`
- Start menu shortcuts
- Program files installation

### 12.6 `extra/` Directory Overview

```
extra/
├── actions/               # Custom actions (Section 8 + 11)
│   ├── Compile all shader files to Spir-V.js
│   ├── GenerateMesh/      # C++ + JS + QML
│   ├── ImportOBJ/         # C++ + JS + QML
│   ├── Import_glTF.js
│   ├── Insert Orbit Camera.js
│   ├── Inspector/         # JS + QML + GLSL (most complex)
│   ├── NodeGraph/         # QML module
│   ├── Sliders/           # JS + QML
│   └── Timer/             # JS + QML
├── libs/                  # JavaScript libraries
│   └── gl-matrix.js       # v3.4.0 matrix/vector math
├── qml/                   # QML module documentation
│   └── imports.qml        # Available Qt Quick modules
├── samples/               # Sample sessions (Section 10)
│   ├── GLSL/              # 30 OpenGL/Vulkan samples
│   └── HLSL/              # 2 Direct3D samples
├── share/                 # Linux desktop integration
│   ├── applications/      # .desktop file
│   └── metainfo/          # AppStream XML
├── themes/                # 200+ Base16 color themes
│   └── *.yaml
└── wix/                   # Windows MSI installer template
```

---

## Appendix A — Quick Recipes

### Fullscreen Fragment Shader (Attributeless Quad)

```json
{
  "type": "Call",
  "callType": "Draw",
  "primitiveType": "TriangleStrip",
  "count": "4"
}
```
Vertex shader generates positions from `gl_VertexID`. No Stream needed.

### Ping-Pong Texture Pattern

```json
[
  { "type": "Call", "callType": "Compute", "programId": ..., ... },
  { "type": "Call", "callType": "SwapTextures", "textureId": ..., "fromTextureId": ... }
]
```

### Conditional Reset/Per-Frame Execution

```json
{ "type": "Call", "executeOn": "ResetEvaluation", "callType": "ClearTexture", ... },
{ "type": "Call", "executeOn": "EveryEvaluation", "callType": "Draw", ... }
```

### GPU Histogram via Compute + SSBO

1. `Buffer` with `Block` (histogram bins as Uint32 fields)
2. `ClearBuffer` call (ResetEvaluation or EveryEvaluation)
3. `Compute` call reading a texture via `Image` binding, writing to `Buffer` binding with atomics

### Per-Pixel Debug Output

```glsl
#if defined(GPUPAD)
uniform vec2 uMouseFragCoord;
if (ivec2(gl_FragCoord.xy) == ivec2(uMouseFragCoord))
    printf("val=%f", myValue);
#endif
```

Bind `uMouseFragCoord` with `editor: "Expression"`, `values: ["app.mouse.fragCoord"]`.

---

*Generated from source analysis of the GPUpad codebase (`houmain/gpupad`) and all sample sessions/actions.*
