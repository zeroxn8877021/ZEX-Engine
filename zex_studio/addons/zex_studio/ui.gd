@tool
extends RefCounted
## Small helpers that build touch-friendly controls (48 px minimum height).

const TOUCH_HEIGHT := 48


static func button(text: String, callback: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.custom_minimum_size = Vector2(0, TOUCH_HEIGHT)
	b.pressed.connect(callback)
	return b


static func row(label_text: String, control: Control) -> HBoxContainer:
	var h := HBoxContainer.new()
	var l := Label.new()
	l.text = label_text
	l.custom_minimum_size = Vector2(120, 0)
	h.add_child(l)
	control.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	h.add_child(control)
	return h


static func slider(min_v: float, max_v: float, step: float, value: float) -> HSlider:
	var s := HSlider.new()
	s.min_value = min_v
	s.max_value = max_v
	s.step = step
	s.value = value
	s.custom_minimum_size = Vector2(160, TOUCH_HEIGHT)
	return s


static func spin(min_v: float, max_v: float, step: float, value: float) -> SpinBox:
	var s := SpinBox.new()
	s.min_value = min_v
	s.max_value = max_v
	s.step = step
	s.value = value
	s.custom_minimum_size = Vector2(0, TOUCH_HEIGHT)
	return s


static func line(text: String, placeholder: String = "") -> LineEdit:
	var l := LineEdit.new()
	l.text = text
	l.placeholder_text = placeholder
	l.custom_minimum_size = Vector2(0, TOUCH_HEIGHT)
	return l


static func options(items: Array, selected: int = 0) -> OptionButton:
	var o := OptionButton.new()
	for item in items:
		o.add_item(String(item))
	o.select(selected)
	o.custom_minimum_size = Vector2(0, TOUCH_HEIGHT)
	return o


static func scroll_box() -> Array:
	## Returns [ScrollContainer, VBoxContainer] - add controls to the VBoxContainer.
	var sc := ScrollContainer.new()
	sc.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	var vb := VBoxContainer.new()
	vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sc.add_child(vb)
	return [sc, vb]


static func status_label() -> Label:
	var l := Label.new()
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size = Vector2(200, 0)
	return l
