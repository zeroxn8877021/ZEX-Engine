# ZEX package format (`.zexproj`, `.zexbuild`) - version 1

A ZEX package is a ZIP container with a fixed, validated structure. It is **not** a renamed APK/ZIP: opening it
through the engine always runs validation first, and anything that does not match is rejected.

```
zex_manifest.json          manifest (JSON)
payload/<relative path>    every file listed in the manifest
```

## Manifest fields
| Field | Meaning |
|---|---|
| `format` | always `"zex"` |
| `format_version` | integer, currently `1`; newer versions are rejected by older engines |
| `kind` | `"zexproj"` (project backup / share) or `"zexbuild"` (packaged build + metadata) |
| `engine` | `{ name, version }` of the engine that created the package (compatibility info) |
| `project` | `{ name }` read from `project.godot` |
| `created_unix` | creation time |
| `meta` | free-form metadata supplied by the caller (e.g. Android package id, version code) |
| `files` | list of `{ path, size, sha256 }` for every payload file |
| `total_size` | sum of file sizes |
| `manifest_hash` | SHA-256 of the canonical text of `files` (`path\nsha256\nsize\n` per file, in order) |

## Validation (`ZexPackage.validate_package`)
1. Archive opens and contains `zex_manifest.json`.
2. `format == "zex"`, `format_version` supported, `kind` known.
3. `manifest_hash` matches the `files` list.
4. Every path is relative and safe (no `..`, no leading `/`, no `\`, no drive letters).
5. Every listed file exists in `payload/` and its size and SHA-256 match.

## Extraction (`ZexPackage.extract_package`)
Validates first, checks for collisions before writing anything, and never overwrites unless `overwrite = true`.

## Creation (`ZexPackage.create_package`)
Excludes `.godot/`, `.git/`, `.zex_backups/` and other `.zexproj` / `.zexbuild` files. Streams files in 1 MB chunks,
writes to a `.tmp` file and renames it only on success.

## Scripting API
```gdscript
var r = ZexPackage.create_package("res://", "user://backup.zexproj", "zexproj", {"note": "before refactor"})
var v = ZexPackage.validate_package("user://backup.zexproj")   # { valid, errors, manifest }
ZexPackage.extract_package("user://backup.zexproj", "user://restored")

var log = ZexLog.new()
log.add_entry("project", "save", "ok", {"scene": "main.tscn"})
log.record_operation("user://backup.zexproj", "zexproj", "export_project", "my-project-id", "ok")
log.query({"category": "build", "status": "error"})
log.export_report("user://diagnostics.json")
log.clear()
```
Logs stay on the device (`user://zex_logs/zex_log.jsonl`); nothing is uploaded.

## Other ZEX runtime tools
```gdscript
# Graphics presets (Very Low .. Ultra), separate Android and PC values
ZexGraphics.apply_to_project_settings(ZexGraphics.QUALITY_MEDIUM, "android")  # writes *.android overrides
ZexGraphics.apply_to_viewport(get_viewport(), ZexGraphics.get_preset(ZexGraphics.QUALITY_HIGH, "pc"))

# Performance monitor: add a ZexPerfMonitor node, set overlay_enabled = true
var snap = $ZexPerfMonitor.get_snapshot()   # fps, frame_ms, static_memory_mb, objects, draw_calls, primitives, video_memory_mb
$ZexPerfMonitor.begin_section("ai"); do_ai(); $ZexPerfMonitor.end_section("ai")
print($ZexPerfMonitor.get_sections())      # calls, total_ms, avg_ms, max_ms
```
