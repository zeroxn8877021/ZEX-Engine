// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#include "register_types.h"

#include "zex_log.h"
#include "zex_package.h"

#include "core/object/class_db.h"

void initialize_zex_project_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(ZexLog);
	GDREGISTER_CLASS(ZexPackage);
}

void uninitialize_zex_project_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
