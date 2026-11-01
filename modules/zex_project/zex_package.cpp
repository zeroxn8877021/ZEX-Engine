// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#include "zex_package.h"

#include "core/object/class_db.h"

#include "modules/zip/zip_packer.h"
#include "modules/zip/zip_reader.h"

#include "core/config/engine.h"
#include "core/crypto/hashing_context.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/time.h"

static const int64_t ZEX_CHUNK = 1024 * 1024;

static String _zex_sha256_hex(const PackedByteArray &p_bytes) {
	Ref<HashingContext> ctx;
	ctx.instantiate();
	ctx->start(HashingContext::HASH_SHA256);
	ctx->update(p_bytes);
	PackedByteArray h = ctx->finish();
	return String::hex_encode_buffer(h.ptr(), h.size());
}

static bool _zex_is_excluded(const String &p_name) {
	return p_name == ".godot" || p_name == ".git" || p_name == ".zex_backups" || p_name.ends_with(".zexproj") || p_name.ends_with(".zexbuild");
}

static void _zex_collect(const String &p_root, const String &p_rel, Vector<String> &r_files) {
	Ref<DirAccess> da = DirAccess::open(p_root.path_join(p_rel));
	if (da.is_null()) {
		return;
	}
	Vector<String> dirs;
	Vector<String> files;
	da->list_dir_begin();
	String n = da->get_next();
	while (!n.is_empty()) {
		if (n != "." && n != ".." && !_zex_is_excluded(n)) {
			if (da->current_is_dir()) {
				dirs.push_back(n);
			} else {
				files.push_back(n);
			}
		}
		n = da->get_next();
	}
	da->list_dir_end();
	// Sorted so the package content is deterministic.
	dirs.sort();
	files.sort();
	for (const String &f : files) {
		r_files.push_back(p_rel.is_empty() ? f : p_rel.path_join(f));
	}
	for (const String &d : dirs) {
		_zex_collect(p_root, p_rel.is_empty() ? d : p_rel.path_join(d), r_files);
	}
}

bool ZexPackage::is_safe_relative_path(const String &p_path) {
	if (p_path.is_empty() || p_path.begins_with("/") || p_path.begins_with("\\") || p_path.contains("\\") || p_path.contains(":")) {
		return false;
	}
	Vector<String> parts = p_path.split("/");
	for (const String &part : parts) {
		if (part.is_empty() || part == "." || part == "..") {
			return false;
		}
	}
	return true;
}

String ZexPackage::hash_files_array(const Array &p_files) {
	// Canonical text built by hand: JSON round trips turn integers into floats, which would change a JSON-based hash.
	String canonical;
	for (int i = 0; i < p_files.size(); i++) {
		Dictionary fi = p_files[i];
		canonical += String(fi.get("path", "")) + "\n" + String(fi.get("sha256", "")) + "\n" + String::num_int64((int64_t)fi.get("size", 0)) + "\n";
	}
	return _zex_sha256_hex(canonical.to_utf8_buffer());
}

Dictionary ZexPackage::create_package(const String &p_source_dir, const String &p_output_path, const String &p_kind, const Dictionary &p_meta) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = "";

	if (p_kind != "zexproj" && p_kind != "zexbuild") {
		result["error"] = "Kind must be 'zexproj' or 'zexbuild'.";
		return result;
	}
	if (!DirAccess::dir_exists_absolute(p_source_dir)) {
		result["error"] = "Source folder does not exist.";
		return result;
	}

	Vector<String> rel_files;
	_zex_collect(p_source_dir, "", rel_files);
	if (rel_files.is_empty()) {
		result["error"] = "Source folder has no files to package.";
		return result;
	}

	Error derr = DirAccess::make_dir_recursive_absolute(p_output_path.get_base_dir());
	if (derr != OK && derr != ERR_ALREADY_EXISTS) {
		result["error"] = "Cannot create the output folder.";
		return result;
	}

	// Write to a temporary file first so a failed export never leaves a half-written package.
	const String tmp_path = p_output_path + ".tmp";
	Ref<ZIPPacker> zp;
	zp.instantiate();
	if (zp->open(tmp_path, ZIPPacker::APPEND_CREATE) != OK) {
		result["error"] = "Cannot create the package file.";
		return result;
	}

	Array files_info;
	int64_t total = 0;
	for (const String &rel : rel_files) {
		Ref<FileAccess> in = FileAccess::open(p_source_dir.path_join(rel), FileAccess::READ);
		if (in.is_null()) {
			zp->close();
			DirAccess::remove_absolute(tmp_path);
			result["error"] = "Cannot read file: " + rel;
			return result;
		}
		Ref<HashingContext> ctx;
		ctx.instantiate();
		ctx->start(HashingContext::HASH_SHA256);
		zp->start_file(String(PAYLOAD_PREFIX) + rel);
		int64_t size = 0;
		while (!in->eof_reached()) {
			PackedByteArray chunk = in->get_buffer(ZEX_CHUNK);
			if (chunk.is_empty()) {
				break;
			}
			ctx->update(chunk);
			zp->write_file(chunk);
			size += chunk.size();
		}
		zp->close_file();
		PackedByteArray h = ctx->finish();
		Dictionary fi;
		fi["path"] = rel;
		fi["size"] = size;
		fi["sha256"] = String::hex_encode_buffer(h.ptr(), h.size());
		files_info.push_back(fi);
		total += size;
	}

	// Project name from project.godot if present.
	String project_name = p_source_dir.get_file();
	{
		Ref<ConfigFile> cf;
		cf.instantiate();
		if (cf->load(p_source_dir.path_join("project.godot")) == OK) {
			project_name = cf->get_value("application", "config/name", project_name);
		}
	}

	Dictionary engine;
	engine["name"] = "ZEX INTERACTIVE Engine";
	engine["version"] = Engine::get_singleton()->get_version_info().get("string", "");

	Dictionary project;
	project["name"] = project_name;

	Dictionary manifest;
	manifest["format"] = "zex";
	manifest["format_version"] = FORMAT_VERSION;
	manifest["kind"] = p_kind;
	manifest["engine"] = engine;
	manifest["project"] = project;
	manifest["created_unix"] = (int64_t)Time::get_singleton()->get_unix_time_from_system();
	manifest["meta"] = p_meta;
	manifest["total_size"] = total;
	manifest["files"] = files_info;
	manifest["manifest_hash"] = hash_files_array(files_info);

	zp->start_file(MANIFEST_NAME);
	zp->write_file(JSON::stringify(manifest, "\t").to_utf8_buffer());
	zp->close_file();
	zp->close();

	// Replace the final file atomically-ish.
	if (FileAccess::exists(p_output_path)) {
		DirAccess::remove_absolute(p_output_path);
	}
	if (DirAccess::rename_absolute(tmp_path, p_output_path) != OK) {
		result["error"] = "Cannot finalize the package file.";
		return result;
	}

	result["ok"] = true;
	result["file_count"] = files_info.size();
	result["size"] = total;
	result["sha256"] = FileAccess::get_sha256(p_output_path);
	return result;
}

Dictionary ZexPackage::validate_package(const String &p_package_path) {
	Dictionary result;
	Array errors;
	result["valid"] = false;
	result["manifest"] = Dictionary();

	Ref<ZIPReader> zr;
	zr.instantiate();
	if (zr->open(p_package_path) != OK) {
		errors.push_back("Cannot open the package (not a valid ZEX package file).");
		result["errors"] = errors;
		return result;
	}

	PackedStringArray names = zr->get_files();
	if (!names.has(MANIFEST_NAME)) {
		errors.push_back("Manifest zex_manifest.json is missing.");
		zr->close();
		result["errors"] = errors;
		return result;
	}

	PackedByteArray manifest_bytes = zr->read_file(MANIFEST_NAME, true);
	Variant parsed = JSON::parse_string(String::utf8((const char *)manifest_bytes.ptr(), manifest_bytes.size()));
	if (parsed.get_type() != Variant::DICTIONARY) {
		errors.push_back("Manifest is not valid JSON.");
		zr->close();
		result["errors"] = errors;
		return result;
	}
	Dictionary manifest = parsed;
	result["manifest"] = manifest;

	if (String(manifest.get("format", "")) != "zex") {
		errors.push_back("Unknown package format.");
	}
	int version = manifest.get("format_version", 0);
	if (version < 1 || version > FORMAT_VERSION) {
		errors.push_back(vformat("Unsupported package format version %d (this engine supports up to %d).", version, FORMAT_VERSION));
	}
	String kind = manifest.get("kind", "");
	if (kind != "zexproj" && kind != "zexbuild") {
		errors.push_back("Unknown package kind.");
	}

	Array files = manifest.get("files", Array());
	if (hash_files_array(files) != String(manifest.get("manifest_hash", ""))) {
		errors.push_back("Manifest integrity check failed (manifest_hash mismatch).");
	}

	for (int i = 0; i < files.size(); i++) {
		Dictionary fi = files[i];
		String rel = fi.get("path", "");
		if (!is_safe_relative_path(rel)) {
			errors.push_back("Unsafe file path in package: " + rel);
			continue;
		}
		String entry = String(PAYLOAD_PREFIX) + rel;
		if (!names.has(entry)) {
			errors.push_back("File listed but missing from package: " + rel);
			continue;
		}
		PackedByteArray data = zr->read_file(entry, true);
		if ((int64_t)data.size() != (int64_t)fi.get("size", -1)) {
			errors.push_back("Size mismatch: " + rel);
		} else if (_zex_sha256_hex(data) != String(fi.get("sha256", ""))) {
			errors.push_back("Checksum mismatch (file damaged or modified): " + rel);
		}
	}
	zr->close();

	result["errors"] = errors;
	result["valid"] = errors.is_empty();
	return result;
}

Dictionary ZexPackage::extract_package(const String &p_package_path, const String &p_dest_dir, bool p_overwrite) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = "";
	result["file_count"] = 0;

	Dictionary v = validate_package(p_package_path);
	if (!bool(v["valid"])) {
		Array errs = v["errors"];
		result["error"] = errs.is_empty() ? String("Package is not valid.") : String(errs[0]);
		return result;
	}

	Dictionary manifest = v["manifest"];
	Array files = manifest["files"];

	// Check for collisions before writing anything.
	if (!p_overwrite) {
		for (int i = 0; i < files.size(); i++) {
			String rel = Dictionary(files[i])["path"];
			if (FileAccess::exists(p_dest_dir.path_join(rel))) {
				result["error"] = "File already exists (not overwritten): " + rel;
				return result;
			}
		}
	}

	Ref<ZIPReader> zr;
	zr.instantiate();
	if (zr->open(p_package_path) != OK) {
		result["error"] = "Cannot open the package.";
		return result;
	}

	for (int i = 0; i < files.size(); i++) {
		String rel = Dictionary(files[i])["path"];
		String out_path = p_dest_dir.path_join(rel);
		Error derr = DirAccess::make_dir_recursive_absolute(out_path.get_base_dir());
		if (derr != OK && derr != ERR_ALREADY_EXISTS) {
			zr->close();
			result["error"] = "Cannot create folder for: " + rel;
			return result;
		}
		Ref<FileAccess> out = FileAccess::open(out_path, FileAccess::WRITE);
		if (out.is_null()) {
			zr->close();
			result["error"] = "Cannot write file: " + rel;
			return result;
		}
		out->store_buffer(zr->read_file(String(PAYLOAD_PREFIX) + rel, true));
	}
	zr->close();

	result["ok"] = true;
	result["file_count"] = files.size();
	return result;
}

void ZexPackage::_bind_methods() {
	ClassDB::bind_static_method("ZexPackage", D_METHOD("is_safe_relative_path", "path"), &ZexPackage::is_safe_relative_path);
	ClassDB::bind_static_method("ZexPackage", D_METHOD("create_package", "source_dir", "output_path", "kind", "meta"), &ZexPackage::create_package, DEFVAL(Dictionary()));
	ClassDB::bind_static_method("ZexPackage", D_METHOD("validate_package", "package_path"), &ZexPackage::validate_package);
	ClassDB::bind_static_method("ZexPackage", D_METHOD("extract_package", "package_path", "dest_dir", "overwrite"), &ZexPackage::extract_package, DEFVAL(false));
}
