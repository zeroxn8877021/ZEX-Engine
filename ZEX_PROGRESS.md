# ZEX Progress Report

## Completed
- Source audit (`docs_zex/AUDIT.md`)
- Branding: engine name, Android app id/label, launcher + monochrome icons, splash, app icon
- GitHub Actions workflow to build the Android editor APK and export templates

- ZEX World Studio, part 1 (`modules/zex_world_studio`): `ZexTerrain` node (generate noise terrain, heightmap import/export, raise/lower/smooth/flatten brushes with radius/strength/softness, subdivision, height/normal queries) and `ZexFoliage` node (scatter trees/grass/rocks with count, random rotation, random scale, slope limit). C++ unit tests in `modules/zex_world_studio/tests/`.
- ZEX Cloud, part 1 (`modules/zex_cloud`): `ZexAuth` (Supabase email/password sign-up, login, logout, token refresh, encrypted saved session, offline-safe, no hardcoded keys) and `ZexAuthPanel` (touch-friendly login/sign-up UI). Unit tests for validation and error parsing.
- Supabase schema (`cloud/supabase/schema.sql`): profiles + roles, engine releases, remote config / feature flags, announcements, projects, assets, audit log; Row Level Security on every table; role changes locked to super_admin by a trigger.
- Admin Panel (`cloud/admin_panel/index.html`): admin login with server-side role check, dashboard, releases, remote config / feature flags, announcements, users and roles, assets, audit log. Uses only the anon key.
- Config template: `cloud/zex_cloud.cfg.example`.
- Supabase project connected and schema applied (migration `zex_initial_schema`): 7 tables, RLS on all, role-protection trigger. `cloud/zex_cloud.cfg` holds the public URL and publishable key; the Admin Panel is prefilled with them.

## In progress
- Android-first editor layout (Phase 1) – uses Godot's existing Android editor; custom mobile layout not started

## Not started
Hooking ZexAuthPanel into the editor menu, OTA update checker/downloader with signed manifests, Realtime subscription, cloud project backup. Rest of Phase 2 (touch brush editor UI/dock, texture painting layers, grid/surface snapping, roads, rivers, lakes) and Phases 3–7:  Model Previewer, Block Scripting, Graphics presets, Log System, .zexproj/.zexbuild, Build Center, Supabase, Admin Panel, OTA.

## Modified files
version.py, main/splash.png, main/app_icon.png, platform/android/java/editor/build.gradle, platform/android/java/editor/src/main/res/values/strings.xml, platform/android/java/lib/src/main/res/mipmap*/icon_*.webp, .github/workflows/*, misc/zex/branding/*, docs_zex/*

## Build status
NOT yet built. The new C++ module was checked by reading it against the engine headers only; it was not compiled (no SCons/NDK in the sandbox). First build runs on GitHub Actions (sandbox here has no network to compile/download SDKs).

## Known bugs / risks
- Unverified compile (first CI run may need small fixes).
- Editor-internal icons (`editor/icons/Godot.svg`) still Godot's; need an SVG version of the ZEX logo.

## Testing
Unit tests written but not run yet. After a Linux build run: `./bin/godot.linuxbsd.editor.x86_64 --test --test-case="*ZexTerrain*"`
