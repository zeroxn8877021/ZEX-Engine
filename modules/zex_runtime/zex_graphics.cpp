// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#include "zex_graphics.h"

#include "core/config/project_settings.h"
#include "scene/main/viewport.h"
#include "scene/resources/environment.h"

struct ZexPresetRow {
	int msaa;
	int fxaa;
	int dir_shadow;
	int atlas;
	int aniso;
	bool ssao;
	bool ssr;
	bool glow;
	bool fog;
	float scale;
};

static const ZexPresetRow ZEX_PC[ZexGraphics::QUALITY_MAX] = {
	{ 0, 0, 1024, 1024, 1, false, false, false, false, 0.75f },
	{ 0, 1, 2048, 2048, 2, false, false, false, true, 0.85f },
	{ 1, 1, 4096, 2048, 4, false, false, true, true, 1.0f },
	{ 2, 1, 4096, 4096, 8, true, false, true, true, 1.0f },
	{ 3, 1, 8192, 8192, 16, true, true, true, true, 1.0f },
};

static const ZexPresetRow ZEX_ANDROID[ZexGraphics::QUALITY_MAX] = {
	{ 0, 0, 512, 512, 1, false, false, false, false, 0.6f },
	{ 0, 1, 1024, 1024, 1, false, false, false, true, 0.75f },
	{ 0, 1, 2048, 1024, 2, false, false, false, true, 0.85f },
	{ 1, 1, 2048, 2048, 4, false, false, true, true, 1.0f },
	{ 2, 1, 4096, 4096, 8, true, false, true, true, 1.0f },
};

String ZexGraphics::get_quality_name(int p_quality) {
	switch (p_quality) {
		case QUALITY_VERY_LOW:
			return "Very Low";
		case QUALITY_LOW:
			return "Low";
		case QUALITY_MEDIUM:
			return "Medium";
		case QUALITY_HIGH:
			return "High";
		case QUALITY_ULTRA:
			return "Ultra";
	}
	return "Unknown";
}

int ZexGraphics::get_quality_from_name(const String &p_name) {
	for (int i = 0; i < QUALITY_MAX; i++) {
		if (get_quality_name(i).to_lower() == p_name.strip_edges().to_lower()) {
			return i;
		}
	}
	return -1;
}

Dictionary ZexGraphics::get_preset(int p_quality, const String &p_platform) {
	Dictionary d;
	ERR_FAIL_INDEX_V_MSG(p_quality, QUALITY_MAX, d, "Invalid graphics quality.");
	const ZexPresetRow &r = (p_platform.to_lower() == "android") ? ZEX_ANDROID[p_quality] : ZEX_PC[p_quality];
	d["quality"] = get_quality_name(p_quality);
	d["platform"] = p_platform.to_lower() == "android" ? "android" : "pc";
	d["msaa_3d"] = r.msaa;
	d["screen_space_aa"] = r.fxaa;
	d["directional_shadow_size"] = r.dir_shadow;
	d["positional_shadow_atlas_size"] = r.atlas;
	d["anisotropic_level"] = r.aniso;
	d["ssao"] = r.ssao;
	d["ssr"] = r.ssr;
	d["glow"] = r.glow;
	d["fog"] = r.fog;
	d["scaling_3d_scale"] = r.scale;
	return d;
}

void ZexGraphics::apply_to_viewport(Viewport *p_viewport, const Dictionary &p_preset) {
	ERR_FAIL_NULL(p_viewport);
	ERR_FAIL_COND_MSG(p_preset.is_empty(), "Preset is empty.");
	p_viewport->set_msaa_3d(Viewport::MSAA(CLAMP((int)p_preset.get("msaa_3d", 0), 0, 3)));
	p_viewport->set_screen_space_aa((int)p_preset.get("screen_space_aa", 0) ? Viewport::SCREEN_SPACE_AA_FXAA : Viewport::SCREEN_SPACE_AA_DISABLED);
	p_viewport->set_positional_shadow_atlas_size((int)p_preset.get("positional_shadow_atlas_size", 2048));
	p_viewport->set_scaling_3d_scale((float)(double)p_preset.get("scaling_3d_scale", 1.0));
}

void ZexGraphics::apply_to_environment(const Ref<Environment> &p_environment, const Dictionary &p_preset) {
	ERR_FAIL_COND(p_environment.is_null());
	ERR_FAIL_COND_MSG(p_preset.is_empty(), "Preset is empty.");
	p_environment->set_ssao_enabled(p_preset.get("ssao", false));
	p_environment->set_ssr_enabled(p_preset.get("ssr", false));
	p_environment->set_glow_enabled(p_preset.get("glow", false));
	p_environment->set_fog_enabled(p_preset.get("fog", false));
}

Error ZexGraphics::apply_to_project_settings(int p_quality, const String &p_platform) {
	ERR_FAIL_INDEX_V(p_quality, QUALITY_MAX, ERR_INVALID_PARAMETER);
	ProjectSettings *ps = ProjectSettings::get_singleton();
	ERR_FAIL_NULL_V(ps, ERR_UNAVAILABLE);

	Dictionary p = get_preset(p_quality, p_platform);
	const String suffix = p_platform.to_lower() == "android" ? ".android" : "";

	// Default filtering enum: 0=off, 1=2x, 2=4x, 3=8x, 4=16x.
	int level = p["anisotropic_level"];
	int aniso_enum = level <= 1 ? 0 : (level <= 2 ? 1 : (level <= 4 ? 2 : (level <= 8 ? 3 : 4)));

	ps->set_setting("rendering/anti_aliasing/quality/msaa_3d" + suffix, p["msaa_3d"]);
	ps->set_setting("rendering/anti_aliasing/quality/screen_space_aa" + suffix, p["screen_space_aa"]);
	ps->set_setting("rendering/lights_and_shadows/directional_shadow/size" + suffix, p["directional_shadow_size"]);
	ps->set_setting("rendering/lights_and_shadows/positional_shadow/atlas_size" + suffix, p["positional_shadow_atlas_size"]);
	ps->set_setting("rendering/textures/default_filters/anisotropic_filtering_level" + suffix, aniso_enum);
	ps->set_setting("rendering/scaling_3d/scale" + suffix, p["scaling_3d_scale"]);
	return OK;
}

void ZexGraphics::_bind_methods() {
	ClassDB::bind_static_method("ZexGraphics", D_METHOD("get_quality_name", "quality"), &ZexGraphics::get_quality_name);
	ClassDB::bind_static_method("ZexGraphics", D_METHOD("get_quality_from_name", "name"), &ZexGraphics::get_quality_from_name);
	ClassDB::bind_static_method("ZexGraphics", D_METHOD("get_preset", "quality", "platform"), &ZexGraphics::get_preset);
	ClassDB::bind_static_method("ZexGraphics", D_METHOD("apply_to_viewport", "viewport", "preset"), &ZexGraphics::apply_to_viewport);
	ClassDB::bind_static_method("ZexGraphics", D_METHOD("apply_to_environment", "environment", "preset"), &ZexGraphics::apply_to_environment);
	ClassDB::bind_static_method("ZexGraphics", D_METHOD("apply_to_project_settings", "quality", "platform"), &ZexGraphics::apply_to_project_settings);

	BIND_ENUM_CONSTANT(QUALITY_VERY_LOW);
	BIND_ENUM_CONSTANT(QUALITY_LOW);
	BIND_ENUM_CONSTANT(QUALITY_MEDIUM);
	BIND_ENUM_CONSTANT(QUALITY_HIGH);
	BIND_ENUM_CONSTANT(QUALITY_ULTRA);
}
