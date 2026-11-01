@tool
extends HSplitContainer
## Block Scripting editor: drag blocks into a graph, connect them, compile to real GDScript, attach and run.

const UI := preload("res://addons/zex_studio/ui.gd")
const ENTRY_COLOR := Color(0.95, 0.75, 0.2)
const FLOW_COLOR := Color(0.6, 0.85, 1.0)

var graph: GraphEdit
var palette: ItemList
var code_view: TextEdit
var status: Label
var extends_edit: LineEdit
var vars_edit: TextEdit
var name_edit: LineEdit
var catalog := {}
var nodes := {}
var counter := 0


func _ready() -> void:
	for e in ZexBlockCompiler.get_catalog():
		catalog[e["type"]] = e
	_build_ui()


func _build_ui() -> void:
	var left := VBoxContainer.new()
	left.custom_minimum_size = Vector2(230, 0)
	add_child(left)
	palette = ItemList.new()
	palette.size_flags_vertical = Control.SIZE_EXPAND_FILL
	for type in catalog:
		var idx := palette.add_item("[%s] %s" % [catalog[type]["category"], catalog[type]["label"]])
		palette.set_item_metadata(idx, type)
	palette.item_activated.connect(_on_palette_activated)
	left.add_child(palette)
	left.add_child(UI.button("Add selected block", _on_add_pressed))

	var center := HSplitContainer.new()
	center.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	add_child(center)

	var graph_col := VBoxContainer.new()
	graph_col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	center.add_child(graph_col)
	graph = GraphEdit.new()
	graph.size_flags_vertical = Control.SIZE_EXPAND_FILL
	graph.right_disconnects = true
	graph.connection_request.connect(_on_connection_request)
	graph.disconnection_request.connect(_on_disconnection_request)
	graph.delete_nodes_request.connect(_on_delete_nodes_request)
	graph_col.add_child(graph)
	var bar := HBoxContainer.new()
	bar.add_child(UI.button("Compile", _on_compile))
	bar.add_child(UI.button("Attach to selected node", _on_attach))
	bar.add_child(UI.button("Attach + Run scene", _on_run))
	bar.add_child(UI.button("Clear", _clear_graph))
	graph_col.add_child(bar)

	var right := VBoxContainer.new()
	right.custom_minimum_size = Vector2(300, 0)
	center.add_child(right)
	extends_edit = UI.line("Node", "extends (e.g. CharacterBody2D)")
	right.add_child(UI.row("Extends", extends_edit))
	vars_edit = TextEdit.new()
	vars_edit.placeholder_text = "Variables, one per line:\nhealth=100\nspeed=200"
	vars_edit.custom_minimum_size = Vector2(0, 80)
	right.add_child(vars_edit)
	name_edit = UI.line("my_logic", "graph name")
	right.add_child(UI.row("Name", name_edit))
	var save_bar := HBoxContainer.new()
	save_bar.add_child(UI.button("Save", _on_save))
	save_bar.add_child(UI.button("Load", _on_load))
	right.add_child(save_bar)
	code_view = TextEdit.new()
	code_view.editable = false
	code_view.size_flags_vertical = Control.SIZE_EXPAND_FILL
	code_view.placeholder_text = "Generated GDScript appears here"
	right.add_child(code_view)
	status = UI.status_label()
	right.add_child(status)


func _on_palette_activated(index: int) -> void:
	_add_block(String(palette.get_item_metadata(index)))


func _on_add_pressed() -> void:
	var sel := palette.get_selected_items()
	if sel.size() > 0:
		_add_block(String(palette.get_item_metadata(sel[0])))


func _add_block(type: String, id: String = "", params: Dictionary = {}, pos: Vector2 = Vector2.ZERO) -> GraphNode:
	if not catalog.has(type):
		return null
	var e: Dictionary = catalog[type]
	if id == "":
		counter += 1
		id = "b%d" % counter
	else:
		var n := id.trim_prefix("b")
		if n.is_valid_int():
			counter = maxi(counter, n.to_int())
	var gn := GraphNode.new()
	gn.name = id
	gn.title = String(e["label"])
	if pos == Vector2.ZERO:
		pos = graph.scroll_offset + Vector2(40, 40 + nodes.size() * 14)
	gn.position_offset = pos
	var slots: Array = e["slots"]
	var is_entry: bool = e["entry"]
	var color := ENTRY_COLOR if is_entry else FLOW_COLOR
	for i in slots.size():
		var l := Label.new()
		l.text = String(slots[i]) + "  >"
		l.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		gn.add_child(l)
		gn.set_slot(i, (i == 0 and not is_entry), 0, color, true, 0, color)
	var editors := {}
	for p in e["params"]:
		var row := HBoxContainer.new()
		var lab := Label.new()
		lab.text = String(p["name"])
		lab.custom_minimum_size = Vector2(90, 0)
		row.add_child(lab)
		var value := String(params.get(p["name"], p["default"]))
		var ed: Control
		if String(p["type"]) == "enum":
			var ob := OptionButton.new()
			for o in p["options"]:
				ob.add_item(String(o))
			for i in ob.item_count:
				if ob.get_item_text(i) == value:
					ob.select(i)
			ed = ob
		else:
			var le := LineEdit.new()
			le.text = value
			le.custom_minimum_size = Vector2(130, 40)
			ed = le
		row.add_child(ed)
		gn.add_child(row)
		editors[String(p["name"])] = ed
	gn.set_meta("type", type)
	gn.set_meta("editors", editors)
	graph.add_child(gn)
	nodes[id] = gn
	return gn


func _on_connection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	if from_node == to_node:
		return
	for c in graph.get_connection_list():
		if c["from_node"] == from_node and c["from_port"] == from_port:
			graph.disconnect_node(c["from_node"], c["from_port"], c["to_node"], c["to_port"])
	graph.connect_node(from_node, from_port, to_node, to_port)


func _on_disconnection_request(from_node: StringName, from_port: int, to_node: StringName, to_port: int) -> void:
	graph.disconnect_node(from_node, from_port, to_node, to_port)


func _on_delete_nodes_request(names: Array) -> void:
	for n in names:
		for c in graph.get_connection_list():
			if c["from_node"] == n or c["to_node"] == n:
				graph.disconnect_node(c["from_node"], c["from_port"], c["to_node"], c["to_port"])
		var key := String(n)
		if nodes.has(key):
			nodes[key].queue_free()
			nodes.erase(key)


func _clear_graph() -> void:
	graph.clear_connections()
	for id in nodes:
		nodes[id].queue_free()
	nodes.clear()
	counter = 0
	code_view.text = ""
	status.text = "Cleared."


func build_graph() -> Dictionary:
	var by_id := {}
	var blocks := []
	for id in nodes:
		var gn: GraphNode = nodes[id]
		var params := {}
		var editors: Dictionary = gn.get_meta("editors")
		for pname in editors:
			var ed: Control = editors[pname]
			if ed is OptionButton:
				params[pname] = (ed as OptionButton).get_item_text((ed as OptionButton).selected)
			else:
				params[pname] = (ed as LineEdit).text
		var b := {"id": id, "type": String(gn.get_meta("type")), "params": params}
		by_id[id] = b
		blocks.append(b)
	for c in graph.get_connection_list():
		var src: Dictionary = by_id.get(String(c["from_node"]), {})
		if src.is_empty():
			continue
		var slots: Array = catalog[src["type"]]["slots"]
		var port: int = c["from_port"]
		if port < slots.size():
			src[String(slots[port])] = String(c["to_node"])
	var variables := []
	for line in vars_edit.text.split("\n"):
		var parts := line.split("=", true, 1)
		if parts.size() == 2 and parts[0].strip_edges() != "":
			variables.append({"name": parts[0].strip_edges(), "value": parts[1].strip_edges()})
	return {"extends": extends_edit.text.strip_edges(), "variables": variables, "blocks": blocks}


func _show_result(r: Dictionary) -> void:
	code_view.text = String(r.get("code", ""))
	var lines := []
	for e in r.get("errors", []):
		lines.append("ERROR: " + String(e))
	for w in r.get("warnings", []):
		lines.append("Warning: " + String(w))
	if r.get("ok", false):
		lines.push_front("OK - the logic compiled.")
	status.text = "\n".join(lines)


func _on_compile() -> void:
	_show_result(ZexBlockCompiler.compile(build_graph()))


func _attach() -> bool:
	var sel := EditorInterface.get_selection().get_selected_nodes()
	if sel.is_empty():
		status.text = "Select a node in the scene first (the blocks become its script)."
		return false
	if extends_edit.text.strip_edges() == "" or extends_edit.text.strip_edges() == "Node":
		extends_edit.text = sel[0].get_class()
	var r: Dictionary = ZexBlockCompiler.apply_to_node(sel[0], build_graph())
	_show_result(r)
	if r.get("ok", false):
		status.text += "\nAttached to " + String(sel[0].name) + "."
	return r.get("ok", false)


func _on_attach() -> void:
	_attach()


func _on_run() -> void:
	if _attach():
		EditorInterface.play_current_scene()


func _graph_path() -> String:
	return "res://zex_blocks/%s.json" % name_edit.text.strip_edges().validate_filename()


func _on_save() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://zex_blocks"))
	var data := build_graph()
	var layout := {}
	for id in nodes:
		var p: Vector2 = nodes[id].position_offset
		layout[id] = [p.x, p.y]
	data["layout"] = layout
	var f := FileAccess.open(_graph_path(), FileAccess.WRITE)
	if f == null:
		status.text = "Could not save: " + str(FileAccess.get_open_error())
		return
	f.store_string(JSON.stringify(data, "\t"))
	status.text = "Saved " + _graph_path()


func _on_load() -> void:
	if not FileAccess.file_exists(_graph_path()):
		status.text = "No saved graph at " + _graph_path()
		return
	var data = JSON.parse_string(FileAccess.get_file_as_string(_graph_path()))
	if typeof(data) != TYPE_DICTIONARY:
		status.text = "That file is not a valid block graph."
		return
	_clear_graph()
	extends_edit.text = String(data.get("extends", "Node"))
	var lines := []
	for v in data.get("variables", []):
		lines.append("%s=%s" % [v.get("name", ""), v.get("value", "")])
	vars_edit.text = "\n".join(lines)
	var layout: Dictionary = data.get("layout", {})
	for b in data.get("blocks", []):
		var pos := Vector2.ZERO
		if layout.has(b["id"]):
			pos = Vector2(layout[b["id"]][0], layout[b["id"]][1])
		_add_block(String(b["type"]), String(b["id"]), b.get("params", {}), pos)
	await get_tree().process_frame
	for b in data.get("blocks", []):
		if not catalog.has(b["type"]):
			continue
		var slots: Array = catalog[b["type"]]["slots"]
		for i in slots.size():
			var key := String(slots[i])
			if b.has(key) and nodes.has(String(b[key])):
				graph.connect_node(String(b["id"]), i, String(b[key]), 0)
	status.text = "Loaded " + _graph_path()
