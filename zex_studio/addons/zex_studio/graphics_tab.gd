@tool
extends ScrollContainer
## Graphics presets (Very Low .. Ultra), separate for Android and PC, plus a live performance readout.

const UI := preload("res://addons/zex_studio/ui.gd")
const LEVELS := ["Very Low", "Low", "Medium", "High", "Ultra"]

var android_option: OptionButton
var pc_option: OptionButton
var preview: Label
var stats: Label
var status: Label
var monitor := ZexPerfMonitor.new()


func _ready() -> void:
	horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	var vb := VBoxContainer.new()
	vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	add_child(vb)

	android_option = UI.options(LEVELS, 1)
	android_option.item_selected.connect(func(_i: int): _update_preview())
	vb.add_child(UI.row("Android quality", android_option))
	pc_option = UI.options(LEVELS, 3)
	pc_option.item_selected.connect(func(_i: int): _update_preview())
	vb.add_child(UI.row("PC quality", pc_option))
	preview = UI.status_label()
	vb.add_child(preview)
	vb.add_child(UI.button("Apply to project settings and save", _on_apply))
	vb.add_child(UI.button("Add FPS / memory overlay to the open scene", _on_add_monitor))
	status = UI.status_label()
	vb.add_child(status)

	vb.add_child(HSeparator.new())
	var t := Label.new()
	t.text = "Editor performance (live)"
	vb.add_child(t)
	stats = UI.status_label()
	vb.add_child(stats)

	var timer := Timer.new()
	timer.wait_time = 1.0
	timer.autostart = true
	timer.timeout.connect(_update_stats)
	add_child(timer)
	_update_preview()
	_update_stats()


func _update_preview() -> void:
	var a: Dictionary = ZexGraphics.get_preset(android_option.selected, "android")
	var p: Dictionary = ZexGraphics.get_preset(pc_option.selected, "pc")
	preview.text = "Android: MSAA %d, shadows %d, anisotropic %dx, SSAO %s, glow %s, scale %.2f\nPC: MSAA %d, shadows %d, anisotropic %dx, SSAO %s, SSR %s, glow %s" % [
		a["msaa_3d"], a["directional_shadow_size"], a["anisotropic_level"], a["ssao"], a["glow"], a["scaling_3d_scale"],
		p["msaa_3d"], p["directional_shadow_size"], p["anisotropic_level"], p["ssao"], p["ssr"], p["glow"]]


func _on_apply() -> void:
	ZexGraphics.apply_to_project_settings(android_option.selected, "android")
	ZexGraphics.apply_to_project_settings(pc_option.selected, "pc")
	var err := ProjectSettings.save()
	status.text = "Saved: Android = %s, PC = %s." % [LEVELS[android_option.selected], LEVELS[pc_option.selected]] if err == OK else "Could not save project settings (error %d)." % err


func _on_add_monitor() -> void:
	var root := EditorInterface.get_edited_scene_root()
	if root == null:
		status.text = "Open a scene first."
		return
	var m := ZexPerfMonitor.new()
	m.name = "ZexPerfMonitor"
	m.overlay_enabled = true
	root.add_child(m)
	m.owner = root
	status.text = "Monitor added. It shows FPS, memory and draw calls while the game runs."


func _update_stats() -> void:
	var s: Dictionary = monitor.get_snapshot()
	if s.is_empty():
		return
	stats.text = "FPS %d | memory %.1f MB | objects %d | draw calls %d | video memory %.1f MB" % [
		int(s["fps"]), s["static_memory_mb"], int(s["objects"]), int(s["draw_calls"]), s["video_memory_mb"]]
