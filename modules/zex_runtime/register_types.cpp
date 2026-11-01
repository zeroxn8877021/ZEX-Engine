// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#include "register_types.h"

#include "zex_graphics.h"
#include "zex_perf_monitor.h"

#include "core/object/class_db.h"

void initialize_zex_runtime_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(ZexGraphics);
	GDREGISTER_CLASS(ZexPerfMonitor);
}

void uninitialize_zex_runtime_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
