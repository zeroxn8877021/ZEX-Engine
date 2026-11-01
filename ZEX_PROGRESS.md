# ZEX Progress Report

Last updated: 2026-11-01. **Nothing here has been compiled or run by the author** (no compiler or Godot binary was available while writing it). The first GitHub Actions build and the first start of the editor are the real tests. If something fails, fix it before adding more.

## Engine (C++ modules in `modules/`)
- Branding, applied at build time by `misc/zex/apply_branding.py` (called from `modules/zex_blocks/config.py`): engine name, Android app id/label, "Godot" -> "ZEX" in editor UI strings, About dialog with a ZEX INTERACTIVE tab (Godot license, authors and donors kept), ZEX logo, launcher icons, splash, Android sensor-landscape fix.
- `zex_world_studio`: `ZexTerrain` (noise terrain, heightmap import/export, raise/lower/smooth/flatten, subdivision), `ZexFoliage` (scatter).
- `zex_cloud`: `ZexAuth` (Supabase email/password sign-up/login/logout/refresh, encrypted saved session, offline-safe), `ZexAuthPanel`.
- `zex_project`: `ZexLog` (local project/import/export log, SHA-256, search/filter, report), `ZexPackage` (`.zexproj` / `.zexbuild` create/validate/extract with integrity hashes).
- `zex_runtime`: `ZexGraphics` (Very Low .. Ultra, Android/PC), `ZexPerfMonitor` (FPS, memory, draw calls, overlay, profiler).
- `zex_blocks`: `ZexBlockCompiler` (block graph -> real GDScript, 30 blocks, validation, injection protection, catalog).

## Editor tools (GDScript addon `zex_studio/`)
Block editor (GraphEdit), World Studio (sculpt, texture painting, foliage, roads/rivers from Path3D, water, lakes, snapping, duplicate, grouping), Model Previewer (GLB/GLTF, animations, skeleton, info, drag-and-drop import), Build Center (backup/restore `.zexproj`, `.zexbuild`, export preset validation, desktop export, history), Log viewer, Graphics presets + live monitor, Account (login/sign-up). See `zex_studio/README.md`.

## Cloud
- Supabase schema applied to the connected project (7 tables, Row Level Security, role-protection trigger): `cloud/supabase/schema.sql`.
- Admin Panel: `cloud/admin_panel/index.html` (releases, remote config / feature flags, announcements, users and roles, assets, audit log, dashboard).

## Not done
- OTA update checker/downloader in the engine (the admin panel can publish release rows, but nothing in the engine reads them yet) and signed manifests.
- The ZEX Studio addon is not enabled automatically; it must be copied into a project and enabled.
- Android-first custom editor layout (the standard Godot Android editor is used, branded).
- Texture painting data is stored in the scene as a texture, not as a separate file.
- Unit tests exist in the author's copy for several modules but are not in this repository yet.
- Export from the Build tab starts a second editor process, so it works on desktop only.

## Known risks
- Compile errors are likely in the new C++ modules on the first build; GDScript errors are likely the first time the addon is opened.
- `.github/workflows` cannot be edited by the assistant's GitHub connector (missing permission); the existing workflow is used as is.
- Release signing needs your own keystore (secrets `ZEX_KEYSTORE_BASE64`, `ZEX_KEYSTORE_PASSWORD`, `ZEX_KEYSTORE_ALIAS`).
