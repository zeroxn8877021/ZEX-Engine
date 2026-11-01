# ZEX Studio (editor addon)

The visual tools of ZEX INTERACTIVE Engine, written as an editor plugin so they can be improved without rebuilding the engine.
It needs the ZEX INTERACTIVE Engine (it uses the `Zex*` classes) and will show a notice in stock Godot.

## Install
Copy `addons/zex_studio` into your project's `addons/` folder, then enable it in Project > Project Settings > Plugins.
A **ZEX Studio** panel appears at the bottom of the editor.

## Tabs
| Tab | What it does |
|---|---|
| Blocks | Block Scripting editor: add blocks, connect them, set values, compile to real GDScript, attach to the selected node and run, save/load graphs to `res://zex_blocks/` |
| World | Create/generate terrain, sculpt (raise, lower, smooth, flatten) with size/strength/softness, paint Grass/Sand/Rock/Snow/Mud with custom textures, scatter foliage, roads and rivers from a Path3D, water surface and lakes, snap to grid/surface, duplicate, group |
| Models | Preview GLB/GLTF (and imported FBX/OBJ): orbit, pan, pinch zoom, animations, skeleton lines, material/texture info, vertex/triangle counts, size; drop files onto the editor to preview and import them |
| Build | `.zexproj` backups, validate and restore packages, `.zexbuild` packages, export preset validation, desktop export, build history |
| Logs | ZEX Project Log viewer: search, category/status/date filters, diagnostic report export, clear logs |
| Graphics | Very Low .. Ultra presets for Android and PC (written to project settings), FPS/memory overlay node, live performance readout |
| Account | Login / sign-up (Supabase), optional and offline-safe |

Sculpting and painting: choose **Tool = Sculpt** or **Paint texture** on the World tab, then drag on the terrain in the 3D view.
Touch: one finger paints (it arrives as a mouse drag); two-finger gestures still move the camera.

## Status
Written without being able to run it. Expect to fix small mistakes the first time you open it; report any red errors from the Output panel.
