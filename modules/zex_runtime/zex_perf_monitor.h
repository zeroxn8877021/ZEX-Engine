// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#pragma once

#include "core/templates/hash_map.h"
#include "scene/main/node.h"

class Label;

// ZEX performance tools: FPS / memory / draw-call monitor with an optional on-screen overlay,
// plus a basic section profiler (begin_section / end_section) for CPU timing of your own code.
class ZexPerfMonitor : public Node {
	GDCLASS(ZexPerfMonitor, Node);

	struct Section {
		uint64_t started_usec = 0;
		uint64_t total_usec = 0;
		uint64_t max_usec = 0;
		int calls = 0;
	};

	double update_interval = 0.5;
	double low_fps_threshold = 20.0;
	bool overlay_enabled = false;
	double time_accum = 0.0;
	Label *overlay = nullptr;

	Vector<double> fps_history;
	HashMap<String, Section> sections;

	void _sample();
	void _update_overlay_visibility();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_update_interval(double p_seconds);
	double get_update_interval() const { return update_interval; }
	void set_low_fps_threshold(double p_fps) { low_fps_threshold = p_fps; }
	double get_low_fps_threshold() const { return low_fps_threshold; }
	void set_overlay_enabled(bool p_enabled);
	bool is_overlay_enabled() const { return overlay_enabled; }

	// fps, frame_ms, static_memory_mb, objects, draw_calls, primitives, objects_in_frame, video_memory_mb.
	Dictionary get_snapshot() const;
	// min / avg / max FPS over the recent history.
	Dictionary get_fps_stats() const;

	void begin_section(const String &p_name);
	void end_section(const String &p_name);
	Dictionary get_sections() const;
	void reset_sections();

	ZexPerfMonitor();
};
