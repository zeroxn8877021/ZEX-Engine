# ZEX Block Scripting

Blocks are plain data (a Dictionary / JSON) that `ZexBlockCompiler` turns into real GDScript, compiled by the normal
GDScript engine. There is no separate interpreter, so block logic runs at normal script speed and can use signals,
nodes and everything else in the engine.

## Graph format
```json
{
  "extends": "CharacterBody2D",
  "variables": [ { "name": "health", "value": "100" } ],
  "signals": [],
  "blocks": [
    { "id": "ev", "type": "event", "params": { "event": "physics_process" }, "next": "g" },
    { "id": "g",  "type": "gravity", "next": "m" },
    { "id": "m",  "type": "player_move", "params": { "speed": "300" }, "next": "j" },
    { "id": "j",  "type": "jump" }
  ]
}
```
* `next` links to the following block. `if` / `if_body_in_group` also use `then` and `else`; `repeat` / `while` use `body`.
* Entry blocks (`start`, `event`, `function`, `body_entered`, `button_pressed`) begin a function; several entries of the
  same kind are merged into one function.

## Using it from a script
```gdscript
var result = ZexBlockCompiler.build_script(graph)   # { ok, code, errors, warnings, script }
if result.ok:
    ZexBlockCompiler.apply_to_node($Player, graph)  # attaches the compiled script
else:
    print(result.errors)
var palette = ZexBlockCompiler.get_catalog()          # list of blocks for a UI palette
```

## Blocks (30)
Basic: start, event, function, print, set_variable, change_variable, call_function, if, repeat, while, wait, emit_signal.
Game: player_move, platform_move, gravity, jump, run, camera_follow, body_entered, if_body_in_group, call_on_node, damage,
heal, play_animation, play_sound, spawn, destroy, change_scene, button_pressed, ui_set_text.

## Adding a block
Add one `ZexParamDef` array, one `ZEX_EMIT(...)` function and one row in `ZEX_BLOCKS` in `zex_block_compiler.cpp`.
The catalog, validation and compiler pick it up automatically.

## Safety
All identifiers, node paths and expressions are validated before they reach the generated script. Expressions cannot contain
`;`, comments, line breaks, backslashes or calls such as `OS.`, `load(`, `FileAccess`, `execute` or `set_script`, so a graph shared
inside a `.zexproj` package cannot hide extra code. Loops (`while`) yield one frame per iteration so they cannot freeze the game.
