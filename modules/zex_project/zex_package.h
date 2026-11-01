// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"

// ZEX package format (.zexproj = project package, .zexbuild = build package).
//
// Container: a ZIP archive with a fixed structure (see docs_zex/ZEX_PACKAGE_FORMAT.md):
//   zex_manifest.json      manifest: format, format_version, kind, engine, project, created, files[], manifest_hash
//   payload/<relative path>  every file listed in the manifest
// The manifest lists the size and SHA-256 of every payload file; manifest_hash is the SHA-256 of the
// canonical (sorted-key) JSON of the files array. validate() refuses unsafe paths, unknown formats and any mismatch.
class ZexPackage : public RefCounted {
	GDCLASS(ZexPackage, RefCounted);

public:
	static constexpr int FORMAT_VERSION = 1;
	static constexpr const char *MANIFEST_NAME = "zex_manifest.json";
	static constexpr const char *PAYLOAD_PREFIX = "payload/";

protected:
	static void _bind_methods();

public:
	static bool is_safe_relative_path(const String &p_path);
	static String hash_files_array(const Array &p_files);

	// kind: "zexproj" or "zexbuild". p_meta is extra user metadata stored in the manifest.
	// Returns { ok: bool, error: String, file_count: int, size: int, sha256: String }.
	static Dictionary create_package(const String &p_source_dir, const String &p_output_path, const String &p_kind, const Dictionary &p_meta = Dictionary());

	// Returns { valid: bool, errors: Array[String], manifest: Dictionary }.
	static Dictionary validate_package(const String &p_package_path);

	// Validates first, then extracts. Existing files are never overwritten unless p_overwrite is true.
	// Returns { ok: bool, error: String, file_count: int }.
	static Dictionary extract_package(const String &p_package_path, const String &p_dest_dir, bool p_overwrite = false);
};
