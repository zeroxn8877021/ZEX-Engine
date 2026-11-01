@tool
extends HSplitContainer
## ZEX Project Log viewer: search, filter by category/status, export a diagnostic report, clear logs.

const UI := preload("res://addons/zex_studio/ui.gd")

var log := ZexLog.new()
var tree: Tree
var search: LineEdit
var category: OptionButton
var status_filter: OptionButton
var days: OptionButton
var status: Label
var confirm: ConfirmationDialog


func _ready() -> void:
	var left := VBoxContainer.new()
	left.custom_minimum_size = Vector2(260, 0)
	add_child(left)
	search = UI.line("", "search the logs")
	search.text_changed.connect(func(_t: String): refresh())
	left.add_child(search)
	category = UI.options(["All categories", "project", "scene", "asset", "import", "export", "build", "recovery", "cloud", "error"], 0)
	category.item_selected.connect(func(_i: int): refresh())
	left.add_child(UI.row("Category", category))
	status_filter = UI.options(["Any status", "ok", "warning", "error"], 0)
	status_filter.item_selected.connect(func(_i: int): refresh())
	left.add_child(UI.row("Status", status_filter))
	days = UI.options(["All time", "Last 24 hours", "Last 7 days", "Last 30 days"], 0)
	days.item_selected.connect(func(_i: int): refresh())
	left.add_child(UI.row("Date", days))
	left.add_child(UI.button("Refresh", refresh))
	left.add_child(UI.button("Export diagnostic report", _on_export))
	left.add_child(UI.button("Clear all logs", _on_clear_pressed))
	status = UI.status_label()
	left.add_child(status)
	status.text = "Logs stay on this device: " + log.get_log_path()

	tree = Tree.new()
	tree.columns = 5
	tree.column_titles_visible = true
	tree.set_column_title(0, "Time")
	tree.set_column_title(1, "Category")
	tree.set_column_title(2, "Action")
	tree.set_column_title(3, "Status")
	tree.set_column_title(4, "Details")
	tree.hide_root = true
	tree.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	add_child(tree)

	confirm = ConfirmationDialog.new()
	confirm.dialog_text = "Delete every log entry on this device?"
	confirm.confirmed.connect(_on_clear_confirmed)
	add_child(confirm)
	refresh()


func _filter() -> Dictionary:
	var f := {"limit": 500}
	if category.selected > 0:
		f["category"] = category.get_item_text(category.selected)
	if status_filter.selected > 0:
		f["status"] = status_filter.get_item_text(status_filter.selected)
	if search.text.strip_edges() != "":
		f["text"] = search.text.strip_edges()
	var day_seconds := [0, 86400, 604800, 2592000]
	if days.selected > 0:
		f["from_unix"] = int(Time.get_unix_time_from_system()) - int(day_seconds[days.selected])
	return f


func refresh() -> void:
	tree.clear()
	var root := tree.create_item()
	var entries: Array = log.query(_filter())
	for e in entries:
		var item := tree.create_item(root)
		item.set_text(0, String(e.get("iso", "")))
		item.set_text(1, String(e.get("category", "")))
		item.set_text(2, String(e.get("action", "")))
		item.set_text(3, String(e.get("status", "")))
		item.set_text(4, JSON.stringify(e.get("details", {})))
		if String(e.get("status", "")) == "error":
			item.set_custom_color(3, Color(1, 0.4, 0.4))
	status.text = "%d entries. Logs stay on this device: %s" % [entries.size(), log.get_log_path()]


func _on_export() -> void:
	var path := "user://zex_diagnostics_%d.json" % int(Time.get_unix_time_from_system())
	var err: int = log.export_report(path, _filter())
	status.text = "Report saved: " + ProjectSettings.globalize_path(path) if err == OK else "Could not save the report (error %d)." % err


func _on_clear_pressed() -> void:
	confirm.popup_centered()


func _on_clear_confirmed() -> void:
	log.clear()
	refresh()
