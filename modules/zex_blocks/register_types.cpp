// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#include "register_types.h"

#include "zex_block_compiler.h"

#include "core/object/class_db.h"

void initialize_zex_blocks_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(ZexBlockCompiler);
}

void uninitialize_zex_blocks_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
