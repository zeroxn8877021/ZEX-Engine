// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#include "zex_log.h"

#include "core/object/class_db.h"

#include "core/config/engine.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/time.h"

Error ZexLog::add_entry(const String &p_category, const String &p_action, const String &p_status, const Dictionary &p_details) {
	ERR_FAIL_COND_V_MSG(p_category.is_empty() || p_action.is_empty(), ERR_INVALID_PARAMETER, "Log category and action must not be empty.");

	Error err = DirAccess::make_dir_recursive_absolute(log_path.get_base_dir());
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		return err;
	}

	Dictionary entry;
	int64_t now = (int64_t)Time::get_singleton()->get_unix_time_from_system();
	entry["time"] = now;
	entry["iso"] = Time::get_singleton()->get_datetime_string_from_unix_time(now);
	entry["category"] = p_category;
	entry["action"] = p_action;
	entry["status"] = p_status.is_empty() ? String("ok") : p_status;
	entry["details"] = p_details;

	Ref<FileAccess> f = FileAccess::open(log_path, FileAccess::READ_WRITE);
	if (f.is_null()) {
		f = FileAccess::open(log_path, FileAccess::WRITE);
	}
	if (f.is_null()) {
		return FileAccess::get_open_error();
	}
	f->seek_end();
	f->store_line(JSON::stringify(entry));
	return OK;
}

Error ZexLog::record_operation(const String &p_file_path, const String &p_format, const String &p_operation, const String &p_project_id, const String &p_status, const String &p_error) {
	Dictionary d;
	d["file_name"] = p_file_path.get_file();
	d["file_format"] = p_format;
	d["operation"] = p_operation;
	d["project_id"] = p_project_id;
	d["error"] = p_error;
	if (FileAccess::exists(p_file_path)) {
		d["sha256"] = FileAccess::get_sha256(p_file_path);
	}
	return add_entry(p_operation.begins_with("import") ? "import" : "export", p_operation, p_status, d);
}

TypedArray<Dictionary> ZexLog::query(const Dictionary &p_filter) const {
	TypedArray<Dictionary> result;
	Ref<FileAccess> f = FileAccess::open(log_path, FileAccess::READ);
	if (f.is_null()) {
		return result;
	}

	const String category = p_filter.get("category", "");
	const String status = p_filter.get("status", "");
	const String text = String(p_filter.get("text", "")).to_lower();
	const int64_t from_unix = p_filter.get("from_unix", 0);
	const int64_t to_unix = p_filter.get("to_unix", 0);
	const int limit = p_filter.get("limit", 0);

	Vector<Dictionary> matches;
	while (!f->eof_reached()) {
		String line = f->get_line().strip_edges();
		if (line.is_empty()) {
			continue;
		}
		Variant parsed = JSON::parse_string(line);
		if (parsed.get_type() != Variant::DICTIONARY) {
			continue; // Skip corrupt lines instead of failing the whole log.
		}
		Dictionary e = parsed;
		if (!category.is_empty() && String(e.get("category", "")) != category) {
			continue;
		}
		if (!status.is_empty() && String(e.get("status", "")) != status) {
			continue;
		}
		int64_t t = e.get("time", 0);
		if (from_unix > 0 && t < from_unix) {
			continue;
		}
		if (to_unix > 0 && t > to_unix) {
			continue;
		}
		if (!text.is_empty() && !JSON::stringify(e).to_lower().contains(text)) {
			continue;
		}
		matches.push_back(e);
	}

	// Newest first.
	for (int i = matches.size() - 1; i >= 0; i--) {
		result.push_back(matches[i]);
		if (limit > 0 && result.size() >= limit) {
			break;
		}
	}
	return result;
}

int ZexLog::get_entry_count() const {
	return query().size();
}

Error ZexLog::export_report(const String &p_path, const Dictionary &p_filter) const {
	Dictionary report;
	report["report_type"] = "zex_diagnostic_report";
	report["engine_version"] = Engine::get_singleton()->get_version_info().get("string", "");
	report["generated"] = Time::get_singleton()->get_datetime_string_from_system();
	TypedArray<Dictionary> entries = query(p_filter);
	report["entry_count"] = entries.size();
	report["entries"] = entries;

	Error err = DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		return err;
	}
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return FileAccess::get_open_error();
	}
	f->store_string(JSON::stringify(report, "\t"));
	return OK;
}

Error ZexLog::clear() {
	if (!FileAccess::exists(log_path)) {
		return OK;
	}
	return DirAccess::remove_absolute(log_path);
}

void ZexLog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_log_path", "path"), &ZexLog::set_log_path);
	ClassDB::bind_method(D_METHOD("get_log_path"), &ZexLog::get_log_path);
	ClassDB::bind_method(D_METHOD("add_entry", "category", "action", "status", "details"), &ZexLog::add_entry, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("record_operation", "file_path", "format", "operation", "project_id", "status", "error"), &ZexLog::record_operation, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("query", "filter"), &ZexLog::query, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_entry_count"), &ZexLog::get_entry_count);
	ClassDB::bind_method(D_METHOD("export_report", "path", "filter"), &ZexLog::export_report, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("clear"), &ZexLog::clear);
}
