@tool
extends VBoxContainer
## ZEX Account: login / sign-up through Supabase (works offline; the engine never needs an account).


func _ready() -> void:
	var panel := ZexAuthPanel.new()
	panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	add_child(panel)
