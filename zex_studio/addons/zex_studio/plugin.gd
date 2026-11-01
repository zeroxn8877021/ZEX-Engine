@tool
extends EditorPlugin
## ZEX Studio editor plugin. Adds the "ZEX Studio" bottom panel and routes 3D input to the World Studio tab.

var panel: Control


func _enter_tree() -> void:
	panel = load("res://addons/zex_studio/studio_panel.gd").new()
	panel.name = "ZEX Studio"
	panel.set("editor_plugin", self)
	add_control_to_bottom_panel(panel, "ZEX Studio")


func _exit_tree() -> void:
	if panel != null:
		remove_control_from_bottom_panel(panel)
		panel.queue_free()
		panel = null


func _handles(object: Object) -> bool:
	return ClassDB.class_exists("ZexTerrain") and object is ZexTerrain


func _edit(object: Object) -> void:
	if panel != null and panel.world_tab != null and object != null:
		panel.world_tab.set_terrain(object)


func _forward_3d_gui_input(camera: Camera3D, event: InputEvent) -> int:
	if panel != null and panel.world_tab != null:
		return panel.world_tab.handle_3d_input(camera, event)
	return EditorPlugin.AFTER_GUI_INPUT_PASS
