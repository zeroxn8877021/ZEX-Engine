// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#include "zex_block_compiler.h"

#include "modules/gdscript/gdscript.h"

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "scene/main/node.h"

// ---------------------------------------------------------------------------------------------------------------------
// Validation helpers.
// ---------------------------------------------------------------------------------------------------------------------

static const char *ZEX_FORBIDDEN_TOKENS[] = {
	"OS.", "FileAccess", "DirAccess", "JavaClassWrapper", "load(", "preload(", "execute", "shell_open",
	"set_script", "get_script", "Expression", "ClassDB", "GDScript", "ProjectSettings", "ResourceSaver", "HTTPRequest"
};

bool ZexBlockCompiler::is_safe_identifier(const String &p_ident) {
	return p_ident.is_valid_ascii_identifier();
}

bool ZexBlockCompiler::is_safe_expression(const String &p_expr) {
	if (p_expr.is_empty() || p_expr.length() > 200) {
		return false;
	}
	if (p_expr.contains_char('\n') || p_expr.contains_char('\r') || p_expr.contains_char(';') || p_expr.contains_char('#') || p_expr.contains_char('`') || p_expr.contains_char('\\')) {
		return false;
	}
	int depth = 0;
	int quotes = 0;
	for (int i = 0; i < p_expr.length(); i++) {
		char32_t ch = p_expr[i];
		if (ch == '"') {
			quotes++;
		} else if (quotes % 2 == 0) {
			if (ch == '(' || ch == '[' || ch == '{') {
				depth++;
			} else if (ch == ')' || ch == ']' || ch == '}') {
				depth--;
				if (depth < 0) {
					return false;
				}
			}
		}
	}
	if (depth != 0 || quotes % 2 != 0) {
		return false;
	}
	for (const char *tok : ZEX_FORBIDDEN_TOKENS) {
		if (p_expr.contains(tok)) {
			return false;
		}
	}
	return true;
}

static String _q(const String &p_s) {
	return "\"" + p_s.c_escape() + "\"";
}

static bool _is_safe_path(const String &p_s) {
	return !p_s.is_empty() && p_s.length() <= 200 && !p_s.contains_char('\n') && !p_s.contains_char('\r') && !p_s.contains_char('"') && !p_s.contains_char('\\');
}

// ---------------------------------------------------------------------------------------------------------------------
// Block table.
// ---------------------------------------------------------------------------------------------------------------------

struct ZexParamDef {
	const char *name;
	const char *type; // ident, idents, expr, number, string, path, enum
	const char *def;
	bool required;
	const char *options; // comma separated, for enum
};

struct ZexCtx {
	Dictionary by_id;
	Array errors;
	Array warnings;
	HashSet<String> chain_stack;
	Vector<String> ready_lines;
	Vector<String> extra_signals;
	String extends_type = "Node";

	void error(const String &p_msg) { errors.push_back(p_msg); }
	void warn(const String &p_msg) { warnings.push_back(p_msg); }
	void add_signal(const String &p_name) {
		if (!extra_signals.has(p_name)) {
			extra_signals.push_back(p_name);
		}
	}
};

typedef void (*ZexEmitFn)(ZexCtx &, const Dictionary &, const Dictionary &, const String &, Vector<String> &);

struct ZexBlockDef {
	const char *type;
	const char *category;
	const char *label;
	bool entry;
	const ZexParamDef *params;
	int param_count;
	const char *slots; // extra connection slots besides "next"
	ZexEmitFn emit;
};

static void _emit_chain(ZexCtx &c, String p_id, const String &p_in, Vector<String> &r_out);

static void _emit_branch(ZexCtx &c, const String &p_id, const String &p_in, Vector<String> &r_out) {
	int before = r_out.size();
	_emit_chain(c, p_id, p_in, r_out);
	if (r_out.size() == before) {
		r_out.push_back(p_in + "pass");
	}
}

#define ZEX_EMIT(m_name) static void m_name(ZexCtx &c, const Dictionary &b, const Dictionary &p, const String &in, Vector<String> &out)
#define ZEX_STR(m_key) String(p[m_key])
#define ZEX_NO_PARAMS nullptr, 0
#define ZEX_PARAMS(m_arr) m_arr, (int)(sizeof(m_arr) / sizeof(m_arr[0]))

static String _bid(const Dictionary &b) {
	return String(b.get("id", ""));
}

static void _need_body(ZexCtx &c, const String &p_prefix) {
	if (!c.extends_type.begins_with(p_prefix)) {
		c.warn(vformat("This block expects the script to extend %s (currently: %s).", p_prefix, c.extends_type));
	}
}

// ----- statements -----

static const ZexParamDef P_PRINT[] = { { "text", "string", "Hello", false, "" } };
ZEX_EMIT(_emit_print) {
	out.push_back(in + "print(" + _q(ZEX_STR("text")) + ")");
}

static const ZexParamDef P_SET_VAR[] = { { "name", "ident", "", true, "" }, { "value", "expr", "0", true, "" } };
ZEX_EMIT(_emit_set_var) {
	out.push_back(in + ZEX_STR("name") + " = " + ZEX_STR("value"));
}

static const ZexParamDef P_CHANGE_VAR[] = { { "name", "ident", "", true, "" }, { "delta", "expr", "1", true, "" } };
ZEX_EMIT(_emit_change_var) {
	out.push_back(in + ZEX_STR("name") + " += " + ZEX_STR("delta"));
}

static const ZexParamDef P_CALL_FN[] = { { "name", "ident", "", true, "" }, { "args", "expr", "", false, "" } };
ZEX_EMIT(_emit_call_fn) {
	out.push_back(in + ZEX_STR("name") + "(" + ZEX_STR("args") + ")");
}

static const ZexParamDef P_IF[] = { { "condition", "expr", "", true, "" } };
ZEX_EMIT(_emit_if) {
	out.push_back(in + "if " + ZEX_STR("condition") + ":");
	_emit_branch(c, String(b.get("then", "")), in + "\t", out);
	String else_id = b.get("else", "");
	if (!else_id.is_empty()) {
		out.push_back(in + "else:");
		_emit_branch(c, else_id, in + "\t", out);
	}
}

static const ZexParamDef P_REPEAT[] = { { "count", "expr", "3", true, "" } };
ZEX_EMIT(_emit_repeat) {
	out.push_back(in + "for _i_" + _bid(b) + " in range(" + ZEX_STR("count") + "):");
	_emit_branch(c, String(b.get("body", "")), in + "\t", out);
}

static const ZexParamDef P_WHILE[] = { { "condition", "expr", "", true, "" } };
ZEX_EMIT(_emit_while) {
	out.push_back(in + "while " + ZEX_STR("condition") + ":");
	_emit_branch(c, String(b.get("body", "")), in + "\t", out);
	// Yield one frame per iteration so a loop can never freeze the game.
	out.push_back(in + "\tawait get_tree().process_frame");
}

static const ZexParamDef P_WAIT[] = { { "seconds", "number", "1.0", true, "" } };
ZEX_EMIT(_emit_wait) {
	out.push_back(in + "await get_tree().create_timer(" + ZEX_STR("seconds") + ").timeout");
}

static const ZexParamDef P_EMIT_SIGNAL[] = { { "name", "ident", "", true, "" } };
ZEX_EMIT(_emit_emit_signal) {
	c.add_signal(ZEX_STR("name"));
	out.push_back(in + "emit_signal(" + _q(ZEX_STR("name")) + ")");
}

static const ZexParamDef P_MOVE[] = { { "speed", "number", "200", true, "" }, { "dimension", "enum", "2d", true, "2d,3d" } };
ZEX_EMIT(_emit_move) {
	_need_body(c, "CharacterBody");
	String d = "_dir_" + _bid(b);
	out.push_back(in + "var " + d + " = Input.get_vector(\"ui_left\", \"ui_right\", \"ui_up\", \"ui_down\")");
	if (ZEX_STR("dimension") == "3d") {
		out.push_back(in + "velocity.x = " + d + ".x * (" + ZEX_STR("speed") + ")");
		out.push_back(in + "velocity.z = " + d + ".y * (" + ZEX_STR("speed") + ")");
	} else {
		out.push_back(in + "velocity.x = " + d + ".x * (" + ZEX_STR("speed") + ")");
		out.push_back(in + "velocity.y = " + d + ".y * (" + ZEX_STR("speed") + ")");
	}
	out.push_back(in + "move_and_slide()");
}

static const ZexParamDef P_PLATFORM_MOVE[] = { { "speed", "number", "250", true, "" } };
ZEX_EMIT(_emit_platform_move) {
	_need_body(c, "CharacterBody2D");
	out.push_back(in + "velocity.x = Input.get_axis(\"ui_left\", \"ui_right\") * (" + ZEX_STR("speed") + ")");
	out.push_back(in + "move_and_slide()");
}

static const ZexParamDef P_GRAVITY[] = { { "strength", "number", "980", true, "" }, { "dimension", "enum", "2d", true, "2d,3d" } };
ZEX_EMIT(_emit_gravity) {
	_need_body(c, "CharacterBody");
	if (ZEX_STR("dimension") == "3d") {
		out.push_back(in + "velocity.y -= (" + ZEX_STR("strength") + ") * get_physics_process_delta_time()");
	} else {
		out.push_back(in + "velocity.y += (" + ZEX_STR("strength") + ") * get_physics_process_delta_time()");
	}
}

static const ZexParamDef P_JUMP[] = { { "force", "number", "400", true, "" }, { "action", "string", "ui_accept", true, "" }, { "dimension", "enum", "2d", true, "2d,3d" } };
ZEX_EMIT(_emit_jump) {
	_need_body(c, "CharacterBody");
	out.push_back(in + "if Input.is_action_just_pressed(" + _q(ZEX_STR("action")) + ") and is_on_floor():");
	out.push_back(in + "\tvelocity.y = " + String(ZEX_STR("dimension") == "3d" ? "" : "-") + "(" + ZEX_STR("force") + ")");
}

static const ZexParamDef P_RUN[] = { { "speed_var", "ident", "", true, "" }, { "walk", "number", "200", true, "" }, { "run", "number", "350", true, "" }, { "action", "string", "ui_select", true, "" } };
ZEX_EMIT(_emit_run) {
	out.push_back(in + ZEX_STR("speed_var") + " = (" + ZEX_STR("run") + ") if Input.is_action_pressed(" + _q(ZEX_STR("action")) + ") else (" + ZEX_STR("walk") + ")");
}

static const ZexParamDef P_CAMERA[] = { { "camera_path", "path", "", true, "" }, { "smooth", "number", "0.1", true, "" } };
ZEX_EMIT(_emit_camera) {
	String cam = "_cam_" + _bid(b);
	out.push_back(in + "var " + cam + " = get_node(" + _q(ZEX_STR("camera_path")) + ")");
	out.push_back(in + cam + ".global_position = " + cam + ".global_position.lerp(global_position, " + ZEX_STR("smooth") + ")");
}

static const ZexParamDef P_IF_GROUP[] = { { "group", "string", "enemy", true, "" } };
ZEX_EMIT(_emit_if_group) {
	out.push_back(in + "if body.is_in_group(" + _q(ZEX_STR("group")) + "):");
	_emit_branch(c, String(b.get("then", "")), in + "\t", out);
	String else_id = b.get("else", "");
	if (!else_id.is_empty()) {
		out.push_back(in + "else:");
		_emit_branch(c, else_id, in + "\t", out);
	}
}

static const ZexParamDef P_CALL_NODE[] = { { "path", "path", "", true, "" }, { "method", "ident", "", true, "" }, { "args", "expr", "", false, "" } };
ZEX_EMIT(_emit_call_node) {
	out.push_back(in + "get_node(" + _q(ZEX_STR("path")) + ")." + ZEX_STR("method") + "(" + ZEX_STR("args") + ")");
}

static const ZexParamDef P_DAMAGE[] = { { "var", "ident", "health", true, "" }, { "amount", "number", "10", true, "" } };
ZEX_EMIT(_emit_damage) {
	c.add_signal("died");
	out.push_back(in + ZEX_STR("var") + " -= " + ZEX_STR("amount"));
	out.push_back(in + "if " + ZEX_STR("var") + " <= 0:");
	out.push_back(in + "\temit_signal(\"died\")");
}

static const ZexParamDef P_HEAL[] = { { "var", "ident", "health", true, "" }, { "amount", "number", "10", true, "" }, { "max_value", "number", "100", true, "" } };
ZEX_EMIT(_emit_heal) {
	out.push_back(in + ZEX_STR("var") + " = min(" + ZEX_STR("var") + " + (" + ZEX_STR("amount") + "), " + ZEX_STR("max_value") + ")");
}

static const ZexParamDef P_ANIM[] = { { "player_path", "path", "AnimationPlayer", true, "" }, { "name", "string", "idle", true, "" } };
ZEX_EMIT(_emit_anim) {
	out.push_back(in + "get_node(" + _q(ZEX_STR("player_path")) + ").play(" + _q(ZEX_STR("name")) + ")");
}

static const ZexParamDef P_SOUND[] = { { "player_path", "path", "AudioStreamPlayer", true, "" } };
ZEX_EMIT(_emit_sound) {
	out.push_back(in + "get_node(" + _q(ZEX_STR("player_path")) + ").play()");
}

static const ZexParamDef P_SPAWN[] = { { "scene", "path", "", true, "" } };
ZEX_EMIT(_emit_spawn) {
	if (!ZEX_STR("scene").begins_with("res://")) {
		c.error(vformat("Block %s (spawn): scene must be a res:// path.", _bid(b)));
		return;
	}
	String inst = "_inst_" + _bid(b);
	out.push_back(in + "var " + inst + " = load(" + _q(ZEX_STR("scene")) + ").instantiate()");
	out.push_back(in + "get_parent().add_child(" + inst + ")");
	out.push_back(in + inst + ".global_position = global_position");
}

static const ZexParamDef P_DESTROY[] = { { "target", "path", "", false, "" } };
ZEX_EMIT(_emit_destroy) {
	if (ZEX_STR("target").is_empty()) {
		out.push_back(in + "queue_free()");
	} else {
		out.push_back(in + "get_node(" + _q(ZEX_STR("target")) + ").queue_free()");
	}
}

static const ZexParamDef P_SCENE[] = { { "scene", "path", "", true, "" } };
ZEX_EMIT(_emit_scene) {
	if (!ZEX_STR("scene").begins_with("res://")) {
		c.error(vformat("Block %s (change_scene): scene must be a res:// path.", _bid(b)));
		return;
	}
	out.push_back(in + "get_tree().change_scene_to_file(" + _q(ZEX_STR("scene")) + ")");
}

static const ZexParamDef P_UI_TEXT[] = { { "path", "path", "", true, "" }, { "text", "string", "", false, "" } };
ZEX_EMIT(_emit_ui_text) {
	out.push_back(in + "get_node(" + _q(ZEX_STR("path")) + ").text = " + _q(ZEX_STR("text")));
}

// ----- entry blocks (compiled by compile(), the emit function is unused) -----
static const ZexParamDef P_EVENT[] = { { "event", "enum", "process", true, "process,physics_process,input_action" }, { "action", "string", "ui_accept", false, "" } };
static const ZexParamDef P_FUNCTION[] = { { "name", "ident", "", true, "" }, { "args", "idents", "", false, "" } };
static const ZexParamDef P_BUTTON[] = { { "path", "path", "", true, "" } };

static const ZexBlockDef ZEX_BLOCKS[] = {
	// Entry points.
	{ "start", "Basic", "Start (when the scene starts)", true, ZEX_NO_PARAMS, "", nullptr },
	{ "event", "Basic", "Event (every frame / input)", true, ZEX_PARAMS(P_EVENT), "", nullptr },
	{ "function", "Basic", "Function (define)", true, ZEX_PARAMS(P_FUNCTION), "", nullptr },
	{ "body_entered", "Game", "Collision: body entered", true, ZEX_NO_PARAMS, "", nullptr },
	{ "button_pressed", "Game", "UI: button pressed", true, ZEX_PARAMS(P_BUTTON), "", nullptr },
	// Basic.
	{ "print", "Basic", "Print message", false, ZEX_PARAMS(P_PRINT), "", _emit_print },
	{ "set_variable", "Basic", "Set variable", false, ZEX_PARAMS(P_SET_VAR), "", _emit_set_var },
	{ "change_variable", "Basic", "Change variable by", false, ZEX_PARAMS(P_CHANGE_VAR), "", _emit_change_var },
	{ "call_function", "Basic", "Call function", false, ZEX_PARAMS(P_CALL_FN), "", _emit_call_fn },
	{ "if", "Basic", "If condition", false, ZEX_PARAMS(P_IF), "then,else", _emit_if },
	{ "repeat", "Basic", "Repeat N times", false, ZEX_PARAMS(P_REPEAT), "body", _emit_repeat },
	{ "while", "Basic", "While condition", false, ZEX_PARAMS(P_WHILE), "body", _emit_while },
	{ "wait", "Basic", "Wait seconds", false, ZEX_PARAMS(P_WAIT), "", _emit_wait },
	{ "emit_signal", "Basic", "Emit signal", false, ZEX_PARAMS(P_EMIT_SIGNAL), "", _emit_emit_signal },
	// Game.
	{ "player_move", "Game", "Player movement (4 directions)", false, ZEX_PARAMS(P_MOVE), "", _emit_move },
	{ "platform_move", "Game", "Player movement (left/right)", false, ZEX_PARAMS(P_PLATFORM_MOVE), "", _emit_platform_move },
	{ "gravity", "Game", "Apply gravity", false, ZEX_PARAMS(P_GRAVITY), "", _emit_gravity },
	{ "jump", "Game", "Jump", false, ZEX_PARAMS(P_JUMP), "", _emit_jump },
	{ "run", "Game", "Run (hold button)", false, ZEX_PARAMS(P_RUN), "", _emit_run },
	{ "camera_follow", "Game", "Camera follow", false, ZEX_PARAMS(P_CAMERA), "", _emit_camera },
	{ "if_body_in_group", "Game", "If colliding body is in group", false, ZEX_PARAMS(P_IF_GROUP), "then,else", _emit_if_group },
	{ "call_on_node", "Game", "Object interaction: call method", false, ZEX_PARAMS(P_CALL_NODE), "", _emit_call_node },
	{ "damage", "Game", "Damage", false, ZEX_PARAMS(P_DAMAGE), "", _emit_damage },
	{ "heal", "Game", "Heal", false, ZEX_PARAMS(P_HEAL), "", _emit_heal },
	{ "play_animation", "Game", "Play animation", false, ZEX_PARAMS(P_ANIM), "", _emit_anim },
	{ "play_sound", "Game", "Play sound", false, ZEX_PARAMS(P_SOUND), "", _emit_sound },
	{ "spawn", "Game", "Spawn object", false, ZEX_PARAMS(P_SPAWN), "", _emit_spawn },
	{ "destroy", "Game", "Destroy object", false, ZEX_PARAMS(P_DESTROY), "", _emit_destroy },
	{ "change_scene", "Game", "Switch scene", false, ZEX_PARAMS(P_SCENE), "", _emit_scene },
	{ "ui_set_text", "Game", "UI: set text", false, ZEX_PARAMS(P_UI_TEXT), "", _emit_ui_text },
};

static const ZexBlockDef *_find_def(const String &p_type) {
	for (const ZexBlockDef &d : ZEX_BLOCKS) {
		if (p_type == d.type) {
			return &d;
		}
	}
	return nullptr;
}

// Applies defaults and validates every parameter. Returns false (and records errors) when the block cannot be compiled.
static bool _normalize_params(ZexCtx &c, const Dictionary &p_block, const ZexBlockDef *p_def, Dictionary &r_params) {
	Dictionary given = p_block.get("params", Dictionary());
	const String id = _bid(p_block);
	bool ok = true;
	for (int i = 0; i < p_def->param_count; i++) {
		const ZexParamDef &pd = p_def->params[i];
		String raw = given.has(pd.name) ? String(given[pd.name]) : String(pd.def);
		String type = pd.type;
		if (raw.is_empty()) {
			if (pd.required) {
				c.error(vformat("Block %s (%s): parameter '%s' is required.", id, p_def->type, pd.name));
				ok = false;
			}
			r_params[pd.name] = raw;
			continue;
		}
		bool valid = true;
		if (type == "ident") {
			valid = ZexBlockCompiler::is_safe_identifier(raw);
		} else if (type == "idents") {
			Vector<String> parts = raw.split(",");
			for (const String &part : parts) {
				if (!ZexBlockCompiler::is_safe_identifier(part.strip_edges())) {
					valid = false;
				}
			}
		} else if (type == "expr" || type == "number") {
			valid = ZexBlockCompiler::is_safe_expression(raw);
		} else if (type == "path") {
			valid = _is_safe_path(raw);
		} else if (type == "enum") {
			valid = String(pd.options).split(",").has(raw);
		}
		if (!valid) {
			c.error(vformat("Block %s (%s): parameter '%s' has an invalid or unsafe value.", id, p_def->type, pd.name));
			ok = false;
		}
		r_params[pd.name] = raw;
	}
	return ok;
}

static void _emit_chain(ZexCtx &c, String p_id, const String &p_in, Vector<String> &r_out) {
	Vector<String> entered;
	while (!p_id.is_empty()) {
		if (!c.by_id.has(p_id)) {
			c.error(vformat("Connection points to a block that does not exist: '%s'.", p_id));
			break;
		}
		if (c.chain_stack.has(p_id)) {
			c.error(vformat("Block '%s' creates a loop in the connections. Use a Repeat or While block instead.", p_id));
			break;
		}
		Dictionary blk = c.by_id[p_id];
		String type = blk.get("type", "");
		const ZexBlockDef *def = _find_def(type);
		if (def == nullptr) {
			c.error(vformat("Block '%s' has an unknown type '%s'.", p_id, type));
			break;
		}
		if (def->entry) {
			c.error(vformat("Block '%s' (%s) starts a script and cannot be placed inside another chain.", p_id, type));
			break;
		}
		c.chain_stack.insert(p_id);
		entered.push_back(p_id);

		Dictionary params;
		if (_normalize_params(c, blk, def, params)) {
			def->emit(c, blk, params, p_in, r_out);
		}
		p_id = String(blk.get("next", ""));
	}
	for (const String &e : entered) {
		c.chain_stack.erase(e);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------------------------------------------------

Dictionary ZexBlockCompiler::compile(const Dictionary &p_graph) {
	ZexCtx c;
	Dictionary result;

	String extends_type = p_graph.get("extends", "Node");
	if (!is_safe_identifier(extends_type)) {
		c.error("The 'extends' class name is invalid.");
		extends_type = "Node";
	}
	c.extends_type = extends_type;

	Array blocks = p_graph.get("blocks", Array());
	Vector<String> order;
	for (int i = 0; i < blocks.size(); i++) {
		if (blocks[i].get_type() != Variant::DICTIONARY) {
			c.error(vformat("Block #%d is not valid.", i));
			continue;
		}
		Dictionary b = blocks[i];
		String id = b.get("id", "");
		if (!is_safe_identifier(id)) {
			c.error(vformat("Block #%d has an invalid id.", i));
			continue;
		}
		if (c.by_id.has(id)) {
			c.error(vformat("Duplicate block id '%s'.", id));
			continue;
		}
		c.by_id[id] = b;
		order.push_back(id);
	}

	// Which blocks are connected from another block.
	HashSet<String> referenced;
	for (const String &id : order) {
		Dictionary b = c.by_id[id];
		const char *slots[] = { "next", "then", "else", "body" };
		for (const char *s : slots) {
			String target = b.get(s, "");
			if (!target.is_empty()) {
				referenced.insert(target);
			}
		}
	}

	// Header parts.
	Array declared_signals = p_graph.get("signals", Array());
	for (int i = 0; i < declared_signals.size(); i++) {
		String s = declared_signals[i];
		if (is_safe_identifier(s)) {
			c.add_signal(s);
		} else {
			c.error(vformat("Signal name '%s' is invalid.", s));
		}
	}

	Vector<String> var_lines;
	Array vars = p_graph.get("variables", Array());
	HashSet<String> var_names;
	for (int i = 0; i < vars.size(); i++) {
		Dictionary v = vars[i];
		String name = v.get("name", "");
		String value = v.get("value", "null");
		if (!is_safe_identifier(name)) {
			c.error(vformat("Variable name '%s' is invalid.", name));
			continue;
		}
		if (var_names.has(name)) {
			c.error(vformat("Variable '%s' is defined twice.", name));
			continue;
		}
		if (!is_safe_expression(value)) {
			c.error(vformat("Variable '%s' has an invalid or unsafe initial value.", name));
			continue;
		}
		var_names.insert(name);
		var_lines.push_back("var " + name + " = " + value);
	}

	// Entry points -> functions.
	Vector<String> func_order;
	HashMap<String, Vector<String>> func_bodies;
	HashSet<String> defined_names;

	for (const String &id : order) {
		Dictionary b = c.by_id[id];
		String type = b.get("type", "");
		const ZexBlockDef *def = _find_def(type);
		if (def == nullptr) {
			c.error(vformat("Block '%s' has an unknown type '%s'.", id, type));
			continue;
		}
		if (!def->entry) {
			if (!referenced.has(id)) {
				c.warn(vformat("Block '%s' (%s) is not connected to anything and will never run.", id, type));
			}
			continue;
		}
		Dictionary p;
		if (!_normalize_params(c, b, def, p)) {
			continue;
		}

		String sig;
		String body_indent = "\t";
		Vector<String> prefix_lines;
		if (type == "start") {
			sig = "func _ready():";
		} else if (type == "event") {
			String ev = p["event"];
			if (ev == "process") {
				sig = "func _process(delta):";
			} else if (ev == "physics_process") {
				sig = "func _physics_process(delta):";
			} else {
				sig = "func _unhandled_input(event):";
				prefix_lines.push_back("\tif event.is_action_pressed(" + _q(String(p["action"])) + "):");
				body_indent = "\t\t";
			}
		} else if (type == "function") {
			String name = p["name"];
			if (defined_names.has(name)) {
				c.error(vformat("Function '%s' is defined twice.", name));
				continue;
			}
			defined_names.insert(name);
			String args = String(p["args"]).replace(" ", "");
			sig = "func " + name + "(" + args.replace(",", ", ") + "):";
		} else if (type == "body_entered") {
			sig = "func _on_zex_" + id + "(body):";
			c.ready_lines.push_back("\tbody_entered.connect(_on_zex_" + id + ")");
		} else if (type == "button_pressed") {
			sig = "func _on_zex_" + id + "():";
			c.ready_lines.push_back("\tget_node(" + _q(String(p["path"])) + ").pressed.connect(_on_zex_" + id + ")");
		}

		if (!func_bodies.has(sig)) {
			func_order.push_back(sig);
			func_bodies[sig] = Vector<String>();
		}
		Vector<String> &dst = func_bodies[sig];
		for (const String &pl : prefix_lines) {
			dst.push_back(pl);
		}
		int before = dst.size();
		_emit_chain(c, String(b.get("next", "")), body_indent, dst);
		if (dst.size() == before && !prefix_lines.is_empty()) {
			dst.push_back(body_indent + "pass");
		}
	}

	// Connections made in _ready() need that function to exist.
	if (!c.ready_lines.is_empty() && !func_bodies.has("func _ready():")) {
		func_order.push_back("func _ready():");
		func_bodies["func _ready():"] = Vector<String>();
	}

	// Assemble source.
	String code = "# Generated by ZEX INTERACTIVE Block Scripting. Edit the blocks, not this file.\n";
	code += "extends " + extends_type + "\n\n";
	for (const String &s : c.extra_signals) {
		code += "signal " + s + "\n";
	}
	if (!c.extra_signals.is_empty()) {
		code += "\n";
	}
	for (const String &v : var_lines) {
		code += v + "\n";
	}
	if (!var_lines.is_empty()) {
		code += "\n";
	}
	for (const String &sig : func_order) {
		code += sig + "\n";
		Vector<String> body;
		if (sig == "func _ready():") {
			for (const String &r : c.ready_lines) {
				body.push_back(r);
			}
		}
		for (const String &l : func_bodies[sig]) {
			body.push_back(l);
		}
		if (body.is_empty()) {
			body.push_back("\tpass");
		}
		for (const String &l : body) {
			code += l + "\n";
		}
		code += "\n";
	}

	result["code"] = code;
	result["errors"] = c.errors;
	result["warnings"] = c.warnings;
	result["ok"] = c.errors.is_empty();
	return result;
}

Dictionary ZexBlockCompiler::build_script(const Dictionary &p_graph) {
	Dictionary r = compile(p_graph);
	if (!bool(r["ok"])) {
		return r;
	}
	Ref<GDScript> script;
	script.instantiate();
	script->set_source_code(r["code"]);
	Error err = script->reload();
	if (err != OK) {
		Array errors = r["errors"];
		errors.push_back("The generated script did not compile. Check your expressions (variable names, operators).");
		r["errors"] = errors;
		r["ok"] = false;
		return r;
	}
	r["script"] = script;
	return r;
}

Dictionary ZexBlockCompiler::apply_to_node(Node *p_node, const Dictionary &p_graph) {
	Dictionary r = build_script(p_graph);
	ERR_FAIL_NULL_V(p_node, r);
	if (bool(r["ok"])) {
		Ref<Script> s = r["script"];
		p_node->set_script(s);
	}
	return r;
}

TypedArray<Dictionary> ZexBlockCompiler::get_catalog() {
	TypedArray<Dictionary> out;
	for (const ZexBlockDef &d : ZEX_BLOCKS) {
		Dictionary e;
		e["type"] = d.type;
		e["category"] = d.category;
		e["label"] = d.label;
		e["entry"] = d.entry;
		Array slots;
		slots.push_back("next");
		String extra = d.slots;
		if (!extra.is_empty()) {
			for (const String &s : extra.split(",")) {
				slots.push_back(s);
			}
		}
		e["slots"] = slots;
		Array params;
		for (int i = 0; i < d.param_count; i++) {
			Dictionary pd;
			pd["name"] = d.params[i].name;
			pd["type"] = d.params[i].type;
			pd["default"] = d.params[i].def;
			pd["required"] = d.params[i].required;
			Array options;
			String opt_str = d.params[i].options;
			if (!opt_str.is_empty()) {
				for (const String &o : opt_str.split(",")) {
					options.push_back(o);
				}
			}
			pd["options"] = options;
			params.push_back(pd);
		}
		e["params"] = params;
		out.push_back(e);
	}
	return out;
}

void ZexBlockCompiler::_bind_methods() {
	ClassDB::bind_static_method("ZexBlockCompiler", D_METHOD("compile", "graph"), &ZexBlockCompiler::compile);
	ClassDB::bind_static_method("ZexBlockCompiler", D_METHOD("build_script", "graph"), &ZexBlockCompiler::build_script);
	ClassDB::bind_static_method("ZexBlockCompiler", D_METHOD("apply_to_node", "node", "graph"), &ZexBlockCompiler::apply_to_node);
	ClassDB::bind_static_method("ZexBlockCompiler", D_METHOD("get_catalog"), &ZexBlockCompiler::get_catalog);
	ClassDB::bind_static_method("ZexBlockCompiler", D_METHOD("is_safe_expression", "expression"), &ZexBlockCompiler::is_safe_expression);
	ClassDB::bind_static_method("ZexBlockCompiler", D_METHOD("is_safe_identifier", "identifier"), &ZexBlockCompiler::is_safe_identifier);
}
