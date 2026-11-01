// ZEX INTERACTIVE Engine - Copyright (c) 2026 ZEX INTERACTIVE. Released under the MIT License (see LICENSE.txt).

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"

class Node;

// ZEX BLOCK SCRIPTING compiler.
//
// A block graph is a Dictionary (JSON friendly, so it can be saved in .zexproj packages):
//   {
//     "extends": "CharacterBody2D",
//     "variables": [ { "name": "speed", "value": "200" } ],
//     "signals": [ "died" ],
//     "blocks": [
//       { "id": "b1", "type": "start", "next": "b2" },
//       { "id": "b2", "type": "print", "params": { "text": "Hello" } }
//     ]
//   }
// Blocks are linked with "next" (and "then" / "else" / "body" for branches and loops). The compiler turns the graph into
// real GDScript source, which is compiled by the normal GDScript engine, so block logic is genuine executable game logic.
// New blocks are added by adding one entry to the block table in zex_block_compiler.cpp.
//
// Safety: every identifier, path and expression coming from a block is validated; expressions cannot contain statement
// separators, comments, or calls to file / OS / script-loading APIs, so a shared graph cannot smuggle in extra code.
class ZexBlockCompiler : public RefCounted {
	GDCLASS(ZexBlockCompiler, RefCounted);

protected:
	static void _bind_methods();

public:
	// Returns { ok: bool, code: String, errors: Array[String], warnings: Array[String] }.
	static Dictionary compile(const Dictionary &p_graph);

	// compile() + GDScript compilation. Returns { ok, code, errors, warnings, script: GDScript (when ok) }.
	static Dictionary build_script(const Dictionary &p_graph);

	// build_script() and attach the result to a node. Returns the same dictionary.
	static Dictionary apply_to_node(Node *p_node, const Dictionary &p_graph);

	// Describes every available block (type, category, label, params, slots) so an editor UI can build its palette.
	static TypedArray<Dictionary> get_catalog();

	static bool is_safe_expression(const String &p_expr);
	static bool is_safe_identifier(const String &p_ident);
};
