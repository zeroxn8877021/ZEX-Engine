# ZEX INTERACTIVE Engine
Android-first game engine based on Godot Engine (MIT). See `docs_zex/AUDIT.md` and `ZEX_PROGRESS.md`.
Godot is (c) Juan Linietsky, Ariel Manzur and contributors - see `LICENSE.txt` and `COPYRIGHT.txt`.

## Build the APK on GitHub
1. Push this repo to GitHub (branch `main`).
2. Open **Actions -> ZEX Android Build** (starts on push, or press *Run workflow*).
3. When finished, download the artifact **ZEX-Editor-Android-arm64** (the editor APK).
4. Optional release signing: add repository secrets `ZEX_KEYSTORE_BASE64`, `ZEX_KEYSTORE_PASSWORD`, `ZEX_KEYSTORE_ALIAS`. Without them a test keystore is generated.

Branding (names, About dialog, landscape fix, icons) is applied automatically during the build by `misc/zex/apply_branding.py`.

## Cloud setup (login / sign-up)
1. Create a Supabase project, open the SQL editor and run `cloud/supabase/schema.sql`.
2. Sign up once through the engine (or the Supabase dashboard), then promote yourself once in the SQL editor:
   `update public.profiles set role = 'super_admin' where email = 'you@example.com';`
3. Copy `cloud/zex_cloud.cfg.example` to `user://zex_cloud.cfg`, fill in the project URL and the **anon** key, and call `ZexAuth.load_config("user://zex_cloud.cfg")`.
4. Host `cloud/admin_panel/index.html` anywhere static and log in with your admin account.
Never put the `service_role` key in the engine or the admin page.
