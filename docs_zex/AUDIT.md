# ZEX INTERACTIVE Engine – Technical Audit (Phase 1)

Base: Godot 4.8-dev (`version.py`: major=4, minor=8, status=beta). License MIT; `LICENSE.txt`, `COPYRIGHT.txt`, `AUTHORS.md`, `DONORS.md` and `thirdparty/` licenses are preserved and must stay.

## Existing systems we reuse (no rewrite)
| ZEX feature | Reuse / extend | Location |
|---|---|---|
| Android editor | Already exists (Java/Kotlin + native editor lib) | `platform/android/java/editor`, `platform/android/editor` |
| Touch / gestures | Godot input events (`InputEventScreenTouch`, `InputEventMagnifyGesture`, `InputEventPanGesture`) | `core/input`, `platform/android/android_input_handler.cpp` |
| Terrain / World Studio | New EditorPlugin (C++ module) using `ArrayMesh`, `Image` heightmaps, `MultiMeshInstance3D` for foliage | new `modules/zex_world_studio` (Phase 2) |
| Model previewer | `modules/gltf`, `modules/fbx` (ufbx), `modules/tinyexr`, asset dock `EditorFileSystem`/`EditorResourcePreview` | `modules/gltf`, `editor/file_system`, `editor/inspector` |
| Block scripting | Extend `modules/visual_script`-style graph via `GraphEdit`/`GraphNode`, compile to GDScript | `scene/gui/graph_edit.cpp`, `modules/gdscript` (Phase 3) |
| Graphics presets | `RenderingServer` + `Environment`, `ProjectSettings` rendering/* | `servers/rendering`, `scene/resources/environment.cpp` |
| Monitors | `Performance` singleton, `EditorProfiler`, Android Perfetto/Swappy hooks | `main/performance.cpp`, `editor/debugger` |
| Logging / recovery | `core/io/logger`, editor autosave (`EditorNode::_save_scene`), `.godot/` backups | `core/io/logger.cpp`, `editor/editor_node.cpp` |
| Export | Android export plugin, `EditorExportPlatform` | `platform/android/export`, `editor/export` |
| Cloud | `HTTPRequest` + `modules/websocket` (Supabase REST/Realtime) – no SDK needed | `scene/main/http_request.cpp`, `modules/websocket` |
| Package format `.zexproj` | `core/io/zip_io`, `core/crypto` (SHA-256, HMAC, Ed25519 verify via mbedtls) | `core/io`, `modules/mbedtls` |

## Phase 1 changes (this commit)
- Engine name: `version.py` → "ZEX INTERACTIVE Engine".
- Android editor: applicationId `com.zexinteractive.engine`, app label "ZEX INTERACTIVE", play window label.
- Launcher icons (adaptive, monochrome, all densities), editor/engine splash (`main/splash.png`), runtime app icon (`main/app_icon.png`) from the supplied logo.
- CI: Godot upstream workflows moved to `.github/upstream-workflows/`; new `.github/workflows/zex_android_build.yml`.
- Not changed on purpose: Java/Kotlin package `org.godotengine.*` and C++ namespaces (renaming breaks JNI and every plugin, no user benefit).

## Honest scope note
The master prompt describes several months of work for a team. It is delivered incrementally; see `ZEX_PROGRESS.md` for what really works vs. not started.
