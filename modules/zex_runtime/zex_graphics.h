// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/variant/type_info.h"

class Viewport;
class Environment;

// ZEX graphics quality presets (Very Low .. Ultra), tuned separately for Android and PC.
// Presets only use settings that exist in every Godot rendering backend; no expensive marketing-only effects.
class ZexGraphics : public RefCounted {
	GDCLASS(ZexGraphics, RefCounted);

public:
	enum Quality {
		QUALITY_VERY_LOW,
		QUALITY_LOW,
		QUALITY_MEDIUM,
		QUALITY_HIGH,
		QUALITY_ULTRA,
		QUALITY_MAX,
	};

protected:
	static void _bind_methods();

public:
	static String get_quality_name(int p_quality);
	static int get_quality_from_name(const String &p_name);

	// platform: "android" or "pc". Keys: msaa_3d, screen_space_aa, directional_shadow_size,
	// positional_shadow_atlas_size, anisotropic_level (1,2,4,8,16), ssao, ssr, glow, fog, scaling_3d_scale.
	static Dictionary get_preset(int p_quality, const String &p_platform);

	static void apply_to_viewport(Viewport *p_viewport, const Dictionary &p_preset);
	static void apply_to_environment(const Ref<Environment> &p_environment, const Dictionary &p_preset);

	// Writes the preset into ProjectSettings. For "android" the keys get the ".android" feature override
	// so PC and Android can be customised independently. Call ProjectSettings.save() afterwards to persist.
	static Error apply_to_project_settings(int p_quality, const String &p_platform);
};

VARIANT_ENUM_CAST(ZexGraphics::Quality);
