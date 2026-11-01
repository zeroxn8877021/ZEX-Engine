# ZEX Progress Report

Last updated: 2026-11-01. Everything below is in the repository; **nothing has been compiled or run on a device by the author** (the sandbox used to write it had no compiler). The first GitHub Actions build is the real test. If it fails, fix the errors it reports before adding features.

## Completed (code written)
- Source audit: `docs_zex/AUDIT.md`.
- Branding, applied automatically at build time by `misc/zex/apply_branding.py` (called from `modules/zex_blocks/config.py`):
  engine name, Android app id/label, "Godot" -> "ZEX" in editor UI strings, About dialog (new ZEX INTERACTIVE tab; Godot license, authors and donors kept), ZEX logo as editor icon, launcher icons, splash and app icon generated from `misc/zex/branding/zex_logo.svg`.
- Android editor orientation: sensor landscape on phones, project manager can rotate freely.
- ZEX World Studio, part 1 (`modules/zex_world_studio`): `ZexTerrain` (noise terrain, heightmap import/export, raise/lower/smooth/flatten brushes, subdivision) and `ZexFoliage` (scatter with density, random rotation/scale, slope limit).
- ZEX Cloud (`modules/zex_cloud`): `ZexAuth` (Supabase email/password sign-up, login, logout, refresh, encrypted saved session, offline-safe) and `ZexAuthPanel` (touch-friendly login/sign-up UI).
- ZEX Project (`modules/zex_project`): `ZexLog` (local project/import/export log with SHA-256, search/filter, diagnostic report, clear) and `ZexPackage` (`.zexproj` / `.zexbuild` create, validate, extract with integrity hashes and path protection). Format: `docs_zex/ZEX_PACKAGE_FORMAT.md`.
- ZEX Runtime (`modules/zex_runtime`): `ZexGraphics` (Very Low .. Ultra presets, separate Android/PC) and `ZexPerfMonitor` (FPS, memory, draw calls, overlay, section profiler).
- ZEX Block Scripting (`modules/zex_blocks`): `ZexBlockCompiler` turns block graphs into real GDScript (30 blocks, validation, injection protection, catalog). Docs: `docs_zex/BLOCK_SCRIPTING.md`.
- Supabase: schema applied to the connected project (7 tables, Row Level Security, role-protection trigger); `cloud/supabase/schema.sql`.

## Not started
- Block scripting visual editor (GraphEdit front-end for the compiler).
- Touch brush panel, texture painting layers, roads, rivers, lakes, grid/surface snapping, object grouping.
- 3D Model Previewer, ZEX Build Center screen, OTA update checker, Android-first custom editor layout.
- Hooking the login panel and tools into editor menus.
- Unit tests exist in the author's copy for several modules but are not part of this repository yet.

## Known risks
- Compile errors are likely in the new modules on the first build.
- `.github/workflows` could not be changed by the assistant (missing permission); the existing workflow is used as is. Branding runs from `modules/zex_blocks/config.py` instead.
- Release signing needs your own keystore (secrets `ZEX_KEYSTORE_BASE64`, `ZEX_KEYSTORE_PASSWORD`, `ZEX_KEYSTORE_ALIAS`).
