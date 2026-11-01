@tool
extends VBoxContainer
## Main ZEX Studio panel: one tab per tool.

var editor_plugin: EditorPlugin
var world_tab: Control


func _ready() -> void:
	custom_minimum_size = Vector2(0, 380)
	if not ClassDB.class_exists("ZexBlockCompiler"):
		var l := Label.new()
		l.text = "ZEX Studio needs the ZEX INTERACTIVE Engine (ZEX classes were not found)."
		add_child(l)
		return
	var tabs := TabContainer.new()
	tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	add_child(tabs)
	var defs := [
		["Blocks", "blocks_tab.gd"],
		["World", "world_tab.gd"],
		["Models", "model_tab.gd"],
		["Build", "build_tab.gd"],
		["Logs", "logs_tab.gd"],
		["Graphics", "graphics_tab.gd"],
		["Account", "account_tab.gd"],
	]
	for d in defs:
		var tab: Control = load("res://addons/zex_studio/" + String(d[1])).new()
		tab.name = String(d[0])
		tabs.add_child(tab)
		if String(d[0]) == "World":
			world_tab = tab
