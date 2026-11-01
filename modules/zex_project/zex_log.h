// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"

// ZEX PROJECT LOG SYSTEM.
// Append-only JSON-lines log stored locally (default: user://zex_logs/zex_log.jsonl).
// Nothing is ever uploaded by this class. Each line is one JSON object:
//   { "time": <unix>, "iso": "<ISO 8601>", "category": "...", "action": "...", "status": "ok|error|warning", "details": { ... } }
class ZexLog : public RefCounted {
	GDCLASS(ZexLog, RefCounted);

	String log_path = "user://zex_logs/zex_log.jsonl";

protected:
	static void _bind_methods();

public:
	void set_log_path(const String &p_path) { log_path = p_path; }
	String get_log_path() const { return log_path; }

	Error add_entry(const String &p_category, const String &p_action, const String &p_status, const Dictionary &p_details = Dictionary());

	// Import / export history record with file name, format, operation, project id, status, error and SHA-256.
	Error record_operation(const String &p_file_path, const String &p_format, const String &p_operation, const String &p_project_id, const String &p_status, const String &p_error = String());

	// Filter keys (all optional): category, status, text (case-insensitive substring), from_unix, to_unix, limit (newest first).
	TypedArray<Dictionary> query(const Dictionary &p_filter = Dictionary()) const;
	int get_entry_count() const;
	Error export_report(const String &p_path, const Dictionary &p_filter = Dictionary()) const;
	Error clear();
};
