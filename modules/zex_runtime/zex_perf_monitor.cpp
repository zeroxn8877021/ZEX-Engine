// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#include "zex_perf_monitor.h"

#include "core/object/class_db.h"

#include "core/os/os.h"
#include "main/performance.h"
#include "scene/gui/label.h"

static const int ZEX_FPS_HISTORY = 120;

ZexPerfMonitor::ZexPerfMonitor() {
	set_process(true);
}

void ZexPerfMonitor::set_update_interval(double p_seconds) {
	update_interval = MAX(0.1, p_seconds);
}

void ZexPerfMonitor::set_overlay_enabled(bool p_enabled) {
	overlay_enabled = p_enabled;
	_update_overlay_visibility();
}

void ZexPerfMonitor::_update_overlay_visibility() {
	if (overlay_enabled && overlay == nullptr) {
		overlay = memnew(Label);
		overlay->set_position(Point2(8, 8));
		add_child(overlay, false, INTERNAL_MODE_FRONT);
	}
	if (overlay != nullptr) {
		overlay->set_visible(overlay_enabled);
	}
}

Dictionary ZexPerfMonitor::get_snapshot() const {
	Performance *perf = Performance::get_singleton();
	Dictionary d;
	if (perf == nullptr) {
		return d;
	}
	const double mb = 1024.0 * 1024.0;
	double fps = perf->get_monitor(Performance::TIME_FPS);
	d["fps"] = fps;
	d["frame_ms"] = fps > 0.0 ? 1000.0 / fps : 0.0;
	d["static_memory_mb"] = perf->get_monitor(Performance::MEMORY_STATIC) / mb;
	d["objects"] = perf->get_monitor(Performance::OBJECT_COUNT);
	d["objects_in_frame"] = perf->get_monitor(Performance::RENDER_TOTAL_OBJECTS_IN_FRAME);
	d["primitives"] = perf->get_monitor(Performance::RENDER_TOTAL_PRIMITIVES_IN_FRAME);
	d["draw_calls"] = perf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME);
	d["video_memory_mb"] = perf->get_monitor(Performance::RENDER_VIDEO_MEM_USED) / mb;
	return d;
}

Dictionary ZexPerfMonitor::get_fps_stats() const {
	Dictionary d;
	if (fps_history.is_empty()) {
		d["min"] = 0.0;
		d["avg"] = 0.0;
		d["max"] = 0.0;
		return d;
	}
	double mn = fps_history[0], mx = fps_history[0], sum = 0.0;
	for (double v : fps_history) {
		mn = MIN(mn, v);
		mx = MAX(mx, v);
		sum += v;
	}
	d["min"] = mn;
	d["avg"] = sum / fps_history.size();
	d["max"] = mx;
	return d;
}

void ZexPerfMonitor::_sample() {
	Dictionary snap = get_snapshot();
	if (snap.is_empty()) {
		return;
	}
	double fps = snap["fps"];
	fps_history.push_back(fps);
	if (fps_history.size() > ZEX_FPS_HISTORY) {
		fps_history.remove_at(0);
	}
	if (overlay != nullptr && overlay_enabled) {
		overlay->set_text(vformat("FPS: %d  (%.1f ms)\nMemory: %.1f MB\nDraw calls: %d\nObjects: %d",
				(int)fps, (double)snap["frame_ms"], (double)snap["static_memory_mb"], (int)(double)snap["draw_calls"], (int)(double)snap["objects"]));
	}
	if (fps > 0.0 && fps < low_fps_threshold) {
		emit_signal("low_fps", fps);
	}
}

void ZexPerfMonitor::_notification(int p_what) {
	if (p_what == NOTIFICATION_PROCESS) {
		time_accum += get_process_delta_time();
		if (time_accum >= update_interval) {
			time_accum = 0.0;
			_sample();
		}
	}
}

void ZexPerfMonitor::begin_section(const String &p_name) {
	ERR_FAIL_COND(p_name.is_empty());
	sections[p_name].started_usec = OS::get_singleton()->get_ticks_usec();
}

void ZexPerfMonitor::end_section(const String &p_name) {
	HashMap<String, Section>::Iterator it = sections.find(p_name);
	ERR_FAIL_COND_MSG(it == sections.end() || it->value.started_usec == 0, "end_section() called without a matching begin_section().");
	uint64_t elapsed = OS::get_singleton()->get_ticks_usec() - it->value.started_usec;
	it->value.started_usec = 0;
	it->value.total_usec += elapsed;
	it->value.max_usec = MAX(it->value.max_usec, elapsed);
	it->value.calls++;
}

Dictionary ZexPerfMonitor::get_sections() const {
	Dictionary out;
	for (const KeyValue<String, Section> &kv : sections) {
		Dictionary s;
		s["calls"] = kv.value.calls;
		s["total_ms"] = kv.value.total_usec / 1000.0;
		s["avg_ms"] = kv.value.calls > 0 ? kv.value.total_usec / 1000.0 / kv.value.calls : 0.0;
		s["max_ms"] = kv.value.max_usec / 1000.0;
		out[kv.key] = s;
	}
	return out;
}

void ZexPerfMonitor::reset_sections() {
	sections.clear();
}

void ZexPerfMonitor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_update_interval", "seconds"), &ZexPerfMonitor::set_update_interval);
	ClassDB::bind_method(D_METHOD("get_update_interval"), &ZexPerfMonitor::get_update_interval);
	ClassDB::bind_method(D_METHOD("set_low_fps_threshold", "fps"), &ZexPerfMonitor::set_low_fps_threshold);
	ClassDB::bind_method(D_METHOD("get_low_fps_threshold"), &ZexPerfMonitor::get_low_fps_threshold);
	ClassDB::bind_method(D_METHOD("set_overlay_enabled", "enabled"), &ZexPerfMonitor::set_overlay_enabled);
	ClassDB::bind_method(D_METHOD("is_overlay_enabled"), &ZexPerfMonitor::is_overlay_enabled);
	ClassDB::bind_method(D_METHOD("get_snapshot"), &ZexPerfMonitor::get_snapshot);
	ClassDB::bind_method(D_METHOD("get_fps_stats"), &ZexPerfMonitor::get_fps_stats);
	ClassDB::bind_method(D_METHOD("begin_section", "name"), &ZexPerfMonitor::begin_section);
	ClassDB::bind_method(D_METHOD("end_section", "name"), &ZexPerfMonitor::end_section);
	ClassDB::bind_method(D_METHOD("get_sections"), &ZexPerfMonitor::get_sections);
	ClassDB::bind_method(D_METHOD("reset_sections"), &ZexPerfMonitor::reset_sections);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "update_interval", PROPERTY_HINT_RANGE, "0.1,10,0.1,or_greater"), "set_update_interval", "get_update_interval");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "low_fps_threshold", PROPERTY_HINT_RANGE, "1,240,1"), "set_low_fps_threshold", "get_low_fps_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "overlay_enabled"), "set_overlay_enabled", "is_overlay_enabled");

	ADD_SIGNAL(MethodInfo("low_fps", PropertyInfo(Variant::FLOAT, "fps")));
}
