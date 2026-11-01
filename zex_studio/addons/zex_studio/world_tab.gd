@tool
extends ScrollContainer
## ZEX World Studio: terrain generation and sculpting, texture painting, foliage, roads, rivers, water and object placement.

const UI := preload("res://addons/zex_studio/ui.gd")
const SPLAT_SIZE := 256
const LAYER_PARAMS := ["tex_r", "tex_g", "tex_b", "tex_a", "tex_base"]
const SPLAT_SHADER := """
shader_type spatial;
uniform sampler2D splat : filter_linear;
uniform sampler2D tex_base : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D tex_r : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D tex_g : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D tex_b : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D tex_a : source_color, repeat_enable, filter_linear_mipmap;
uniform vec4 col_base : source_color = vec4(0.35, 0.25, 0.15, 1.0);
uniform vec4 col_r : source_color = vec4(0.25, 0.55, 0.2, 1.0);
uniform vec4 col_g : source_color = vec4(0.85, 0.78, 0.55, 1.0);
uniform vec4 col_b : source_color = vec4(0.45, 0.45, 0.47, 1.0);
uniform vec4 col_a : source_color = vec4(0.95, 0.97, 1.0, 1.0);
uniform float tiling = 32.0;
void fragment() {
	vec4 w = texture(splat, UV);
	float base_w = clamp(1.0 - (w.r + w.g + w.b + w.a), 0.0, 1.0);
	vec2 uv = UV * tiling;
	vec3 c = texture(tex_base, uv).rgb * col_base.rgb * base_w;
	c += texture(tex_r, uv).rgb * col_r.rgb * w.r;
	c += texture(tex_g, uv).rgb * col_g.rgb * w.g;
	c += texture(tex_b, uv).rgb * col_b.rgb * w.b;
	c += texture(tex_a, uv).rgb * col_a.rgb * w.a;
	ALBEDO = c;
	ROUGHNESS = 0.95;
}
"""

var terrain: Node3D
var status: Label
var vb: VBoxContainer
var res_spin: SpinBox
var cell_spin: SpinBox
var seed_spin: SpinBox
var freq_spin: SpinBox
var amp_spin: SpinBox
var oct_spin: SpinBox
var height_path: LineEdit
var max_height: SpinBox
var tool_option: OptionButton
var mode_option: OptionButton
var radius_slider: HSlider
var strength_slider: HSlider
var soft_slider: HSlider
var layer_option: OptionButton
var tex_path: LineEdit
var tiling_spin: SpinBox
var mesh_path: LineEdit
var count_spin: SpinBox
var fseed_spin: SpinBox
var min_scale: SpinBox
var max_scale: SpinBox
var slope_spin: SpinBox
var yaw_check: CheckButton
var road_width: SpinBox
var water_level: SpinBox
var lake_radius: SpinBox
var grid_spin: SpinBox
var group_edit: LineEdit
var splat_img: Image
var splat_tex: ImageTexture
var material: ShaderMaterial
var dragging := false
var has_hit := false
var last_hit := Vector3.ZERO


func _ready() -> void:
	horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	vb = VBoxContainer.new()
	vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	add_child(vb)

	_section("1. Terrain")
	vb.add_child(UI.button("Create terrain in the open 3D scene", _on_create_terrain))
	res_spin = UI.spin(17, 513, 1, 65)
	vb.add_child(UI.row("Resolution", res_spin))
	cell_spin = UI.spin(0.1, 20, 0.1, 1.0)
	vb.add_child(UI.row("Cell size", cell_spin))
	seed_spin = UI.spin(0, 99999, 1, 1)
	vb.add_child(UI.row("Seed", seed_spin))
	freq_spin = UI.spin(0.005, 1.0, 0.005, 0.05)
	vb.add_child(UI.row("Frequency", freq_spin))
	amp_spin = UI.spin(0, 200, 0.5, 8)
	vb.add_child(UI.row("Mountain height", amp_spin))
	oct_spin = UI.spin(1, 8, 1, 4)
	vb.add_child(UI.row("Hills / detail", oct_spin))
	var gen_bar := HBoxContainer.new()
	gen_bar.add_child(UI.button("Generate", _on_generate))
	gen_bar.add_child(UI.button("Subdivide x2", _on_subdivide))
	gen_bar.add_child(UI.button("Flatten all", _on_flatten_all))
	vb.add_child(gen_bar)
	height_path = UI.line("res://heightmap.png", "heightmap path")
	vb.add_child(UI.row("Heightmap", height_path))
	max_height = UI.spin(1, 500, 1, 30)
	vb.add_child(UI.row("Max height", max_height))
	var hm_bar := HBoxContainer.new()
	hm_bar.add_child(UI.button("Import heightmap", _on_import_heightmap))
	hm_bar.add_child(UI.button("Export heightmap", _on_export_heightmap))
	vb.add_child(hm_bar)

	_section("2. Brush (drag in the 3D view)")
	tool_option = UI.options(["Sculpt", "Paint texture", "Off"], 2)
	vb.add_child(UI.row("Tool", tool_option))
	mode_option = UI.options(["Raise", "Lower", "Smooth", "Flatten"], 0)
	vb.add_child(UI.row("Sculpt mode", mode_option))
	radius_slider = UI.slider(1, 60, 0.5, 8)
	vb.add_child(UI.row("Brush size", radius_slider))
	strength_slider = UI.slider(0.05, 5.0, 0.05, 0.6)
	vb.add_child(UI.row("Brush strength", strength_slider))
	soft_slider = UI.slider(0, 1, 0.05, 0.5)
	vb.add_child(UI.row("Softness", soft_slider))

	_section("3. Texture painting (Grass, Sand, Rock, Snow, Mud)")
	layer_option = UI.options(["Grass", "Sand", "Rock", "Snow", "Mud / Eraser"], 0)
	vb.add_child(UI.row("Layer", layer_option))
	tex_path = UI.line("", "optional custom texture (res://...)")
	vb.add_child(UI.row("Texture", tex_path))
	tiling_spin = UI.spin(1, 256, 1, 32)
	vb.add_child(UI.row("Tiling", tiling_spin))
	var paint_bar := HBoxContainer.new()
	paint_bar.add_child(UI.button("Apply texture to layer", _on_apply_texture))
	paint_bar.add_child(UI.button("Fill all with layer", _on_fill_layer))
	vb.add_child(paint_bar)

	_section("4. Foliage (trees, grass, bushes, rocks)")
	mesh_path = UI.line("", "Mesh resource, e.g. res://tree.tres")
	vb.add_child(UI.row("Mesh", mesh_path))
	count_spin = UI.spin(1, 20000, 1, 200)
	vb.add_child(UI.row("Density (count)", count_spin))
	fseed_spin = UI.spin(0, 99999, 1, 1)
	vb.add_child(UI.row("Seed", fseed_spin))
	min_scale = UI.spin(0.1, 10, 0.1, 0.8)
	vb.add_child(UI.row("Min scale", min_scale))
	max_scale = UI.spin(0.1, 10, 0.1, 1.4)
	vb.add_child(UI.row("Max scale", max_scale))
	slope_spin = UI.spin(0, 90, 1, 35)
	vb.add_child(UI.row("Max slope", slope_spin))
	yaw_check = CheckButton.new()
	yaw_check.text = "Random rotation"
	yaw_check.button_pressed = true
	vb.add_child(yaw_check)
	vb.add_child(UI.button("Scatter foliage", _on_scatter))

	_section("5. Roads, rivers, lakes and water")
	road_width = UI.spin(0.5, 40, 0.5, 4)
	vb.add_child(UI.row("Width", road_width))
	var path_bar := HBoxContainer.new()
	path_bar.add_child(UI.button("Road from Path3D", _on_make_road))
	path_bar.add_child(UI.button("River from Path3D", _on_make_river))
	vb.add_child(path_bar)
	vb.add_child(UI.button("Add Path3D (draw a curve)", _on_add_path))
	water_level = UI.spin(-100, 500, 0.5, 2)
	vb.add_child(UI.row("Water level", water_level))
	lake_radius = UI.spin(1, 500, 1, 20)
	vb.add_child(UI.row("Lake radius", lake_radius))
	var water_bar := HBoxContainer.new()
	water_bar.add_child(UI.button("Water surface", _on_add_water))
	water_bar.add_child(UI.button("Lake at last brush spot", _on_add_lake))
	vb.add_child(water_bar)

	_section("6. Object placement")
	grid_spin = UI.spin(0.1, 20, 0.1, 1)
	vb.add_child(UI.row("Grid size", grid_spin))
	var place_bar := HBoxContainer.new()
	place_bar.add_child(UI.button("Snap to grid", _on_snap_grid))
	place_bar.add_child(UI.button("Snap to surface", _on_snap_surface))
	place_bar.add_child(UI.button("Duplicate", _on_duplicate))
	vb.add_child(place_bar)
	group_edit = UI.line("props", "group name")
	vb.add_child(UI.row("Group name", group_edit))
	vb.add_child(UI.button("Add selected objects to group", _on_group))

	status = UI.status_label()
	vb.add_child(status)


func _section(title: String) -> void:
	var l := Label.new()
	l.text = title
	l.add_theme_font_size_override("font_size", 18)
	vb.add_child(HSeparator.new())
	vb.add_child(l)


func set_terrain(t: Object) -> void:
	terrain = t as Node3D
	if terrain == null:
		return
	res_spin.value = terrain.get("resolution")
	cell_spin.value = terrain.get("cell_size")
	if terrain.material_override is ShaderMaterial and terrain.material_override.shader != null and terrain.material_override.shader.code == SPLAT_SHADER:
		material = terrain.material_override
		splat_tex = material.get_shader_parameter("splat")
		if splat_tex != null:
			splat_img = splat_tex.get_image()
	status.text = "Terrain selected: %s" % terrain.name


func _need_terrain() -> bool:
	if terrain == null or not is_instance_valid(terrain):
		status.text = "Select a ZexTerrain node first (or press Create terrain)."
		return false
	return true


func _on_create_terrain() -> void:
	var root := EditorInterface.get_edited_scene_root()
	if root == null:
		status.text = "Open or create a 3D scene first."
		return
	var t := ZexTerrain.new()
	t.name = "ZexTerrain"
	root.add_child(t)
	t.owner = root
	set_terrain(t)
	EditorInterface.get_selection().clear()
	EditorInterface.get_selection().add_node(t)
	status.text = "Terrain created. Press Generate, then set Tool to Sculpt or Paint and drag in the 3D view."


func _on_generate() -> void:
	if not _need_terrain():
		return
	terrain.set_resolution(int(res_spin.value))
	terrain.set_cell_size(cell_spin.value)
	terrain.generate_noise(int(seed_spin.value), freq_spin.value, amp_spin.value, int(oct_spin.value))
	_reset_splat()
	status.text = "Generated a %d x %d terrain." % [terrain.get_resolution(), terrain.get_resolution()]


func _on_subdivide() -> void:
	if _need_terrain():
		terrain.subdivide()
		status.text = "Subdivided. Resolution is now %d." % terrain.get_resolution()


func _on_flatten_all() -> void:
	if _need_terrain():
		terrain.flatten_all(0.0)
		status.text = "Terrain flattened."


func _on_import_heightmap() -> void:
	if not _need_terrain():
		return
	var img := Image.load_from_file(ProjectSettings.globalize_path(height_path.text))
	if img == null or img.is_empty():
		status.text = "Could not read the heightmap image."
		return
	var err: int = terrain.load_heightmap(img, max_height.value)
	status.text = "Heightmap imported." if err == OK else "Heightmap import failed (error %d)." % err


func _on_export_heightmap() -> void:
	if not _need_terrain():
		return
	var img: Image = terrain.export_heightmap(max_height.value)
	if img == null:
		status.text = "Export failed."
		return
	img.convert(Image.FORMAT_L8)
	var err := img.save_png(ProjectSettings.globalize_path(height_path.text))
	status.text = "Heightmap saved (8-bit)." if err == OK else "Could not save the heightmap (error %d)." % err


func _reset_splat() -> void:
	splat_img = Image.create_empty(SPLAT_SIZE, SPLAT_SIZE, false, Image.FORMAT_RGBA8)
	splat_img.fill(Color(0, 0, 0, 0))
	splat_tex = ImageTexture.create_from_image(splat_img)
	if material != null:
		material.set_shader_parameter("splat", splat_tex)


func _ensure_material() -> void:
	if terrain == null:
		return
	if material != null and terrain.material_override == material:
		return
	var shader := Shader.new()
	shader.code = SPLAT_SHADER
	material = ShaderMaterial.new()
	material.shader = shader
	var white_img := Image.create_empty(1, 1, false, Image.FORMAT_RGBA8)
	white_img.fill(Color.WHITE)
	var white := ImageTexture.create_from_image(white_img)
	for p in LAYER_PARAMS:
		material.set_shader_parameter(p, white)
	if splat_img == null:
		_reset_splat()
	material.set_shader_parameter("splat", splat_tex)
	material.set_shader_parameter("tiling", tiling_spin.value)
	terrain.material_override = material


func _on_apply_texture() -> void:
	if not _need_terrain():
		return
	_ensure_material()
	material.set_shader_parameter("tiling", tiling_spin.value)
	var idx := layer_option.selected
	if tex_path.text.strip_edges() == "":
		status.text = "Tiling updated. Enter a texture path to use a custom texture for this layer."
		return
	var tex = load(tex_path.text.strip_edges())
	if not (tex is Texture2D):
		status.text = "That path is not a texture."
		return
	material.set_shader_parameter(LAYER_PARAMS[idx], tex)
	status.text = "Custom texture set for %s." % layer_option.get_item_text(idx)


func _on_fill_layer() -> void:
	if not _need_terrain():
		return
	_ensure_material()
	var idx := layer_option.selected
	var c := Color(0, 0, 0, 0)
	if idx < 4:
		c[idx] = 1.0
	splat_img.fill(c)
	splat_tex.update(splat_img)
	status.text = "Filled the terrain with %s." % layer_option.get_item_text(idx)


func handle_3d_input(camera: Camera3D, event: InputEvent) -> int:
	if terrain == null or not is_instance_valid(terrain) or tool_option.selected == 2:
		return EditorPlugin.AFTER_GUI_INPUT_PASS
	var pos := Vector2.ZERO
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		dragging = event.pressed
		if not event.pressed:
			return EditorPlugin.AFTER_GUI_INPUT_STOP
		pos = event.position
	elif event is InputEventMouseMotion and dragging:
		pos = event.position
	else:
		return EditorPlugin.AFTER_GUI_INPUT_PASS
	var hit = _raycast(camera, pos)
	if hit != null:
		has_hit = true
		last_hit = hit
		if tool_option.selected == 0:
			terrain.apply_brush(hit, mode_option.selected, radius_slider.value, strength_slider.value, soft_slider.value)
		else:
			_ensure_material()
			_paint(hit)
	return EditorPlugin.AFTER_GUI_INPUT_STOP


func _raycast(camera: Camera3D, screen_pos: Vector2):
	var origin_g := camera.project_ray_origin(screen_pos)
	var dir_g := camera.project_ray_normal(screen_pos)
	var inv := terrain.global_transform.affine_inverse()
	var o := inv * origin_g
	var d := (inv.basis * dir_g).normalized()
	var extent: float = terrain.get_extent()
	var step: float = maxf(float(terrain.get("cell_size")) * 0.5, 0.1)
	var t := 0.0
	while t < 4000.0:
		var p := o + d * t
		if p.x >= 0.0 and p.z >= 0.0 and p.x <= extent and p.z <= extent:
			var h: float = terrain.get_height_at(Vector2(p.x, p.z))
			if p.y <= h:
				return Vector3(p.x, h, p.z)
		t += step
	return null


func _paint(hit: Vector3) -> void:
	if splat_img == null:
		return
	var extent: float = terrain.get_extent()
	var w_px := splat_img.get_width()
	var h_px := splat_img.get_height()
	var cx := hit.x / extent * w_px
	var cz := hit.z / extent * h_px
	var rad: float = maxf(radius_slider.value / extent * w_px, 1.0)
	var hard := 1.0 - soft_slider.value
	var layer := layer_option.selected
	var x0 := maxi(0, int(cx - rad))
	var x1 := mini(w_px - 1, int(cx + rad))
	var z0 := maxi(0, int(cz - rad))
	var z1 := mini(h_px - 1, int(cz + rad))
	for y in range(z0, z1 + 1):
		for x in range(x0, x1 + 1):
			var d := Vector2(x - cx, y - cz).length() / rad
			if d >= 1.0:
				continue
			var f := 1.0
			if d > hard:
				f = 1.0 - smoothstep(hard, 1.0, d)
			var c := splat_img.get_pixel(x, y)
			var w: Array = [c.r, c.g, c.b, c.a]
			var amount: float = strength_slider.value * f * 0.25
			if layer >= 4:
				for i in 4:
					w[i] = maxf(0.0, w[i] - amount)
			else:
				w[layer] = minf(1.0, w[layer] + amount)
				var total: float = w[0] + w[1] + w[2] + w[3]
				if total > 1.0:
					var excess := total - 1.0
					var others: float = total - w[layer]
					if others > 0.0:
						for i in 4:
							if i != layer:
								w[i] = maxf(0.0, w[i] - excess * w[i] / others)
			splat_img.set_pixel(x, y, Color(w[0], w[1], w[2], w[3]))
	splat_tex.update(splat_img)


func _on_scatter() -> void:
	if not _need_terrain():
		return
	var mesh = load(mesh_path.text.strip_edges())
	if not (mesh is Mesh):
		status.text = "The mesh path must point to a Mesh resource (.tres, .res or an imported .obj)."
		return
	var root := EditorInterface.get_edited_scene_root()
	var f := ZexFoliage.new()
	f.name = "Foliage"
	terrain.get_parent().add_child(f)
	f.owner = root
	var n: int = f.scatter(terrain, mesh, int(count_spin.value), int(fseed_spin.value), min_scale.value, max_scale.value, yaw_check.button_pressed, slope_spin.value, Rect2())
	status.text = "Placed %d instances." % n


func _on_add_path() -> void:
	var root := EditorInterface.get_edited_scene_root()
	if root == null:
		status.text = "Open a 3D scene first."
		return
	var p := Path3D.new()
	p.name = "RoadPath"
	p.curve = Curve3D.new()
	p.curve.add_point(Vector3(0, 0.2, 0))
	p.curve.add_point(Vector3(10, 0.2, 5))
	p.curve.add_point(Vector3(20, 0.2, 0))
	root.add_child(p)
	p.owner = root
	EditorInterface.get_selection().clear()
	EditorInterface.get_selection().add_node(p)
	status.text = "Path3D added. Move its points with the Path3D tools, then press Road or River."


func _on_make_road() -> void:
	_make_path_mesh("Road", Color(0.18, 0.18, 0.2), 0.15)


func _on_make_river() -> void:
	_make_path_mesh("River", Color(0.1, 0.4, 0.7, 0.65), 0.05)


func _make_path_mesh(kind: String, color: Color, thickness: float) -> void:
	var sel := EditorInterface.get_selection().get_selected_nodes()
	if sel.is_empty() or not (sel[0] is Path3D):
		status.text = "Select a Path3D node first (use Add Path3D), then press this button."
		return
	var path: Path3D = sel[0]
	var half: float = road_width.value * 0.5
	var poly := CSGPolygon3D.new()
	poly.name = kind
	poly.polygon = PackedVector2Array([Vector2(-half, 0.0), Vector2(-half, thickness), Vector2(half, thickness), Vector2(half, 0.0)])
	poly.mode = CSGPolygon3D.MODE_PATH
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	if color.a < 1.0:
		mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		mat.roughness = 0.05
	poly.material = mat
	path.add_child(poly)
	poly.owner = EditorInterface.get_edited_scene_root()
	poly.path_node = poly.get_path_to(path)
	status.text = "%s created along the path." % kind


func _water_material() -> StandardMaterial3D:
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.1, 0.35, 0.6, 0.6)
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.roughness = 0.05
	mat.metallic = 0.2
	return mat


func _on_add_water() -> void:
	if not _need_terrain():
		return
	var ext: float = terrain.get_extent()
	var mi := MeshInstance3D.new()
	mi.name = "Water"
	var plane := PlaneMesh.new()
	plane.size = Vector2(ext, ext)
	mi.mesh = plane
	mi.material_override = _water_material()
	terrain.get_parent().add_child(mi)
	mi.owner = EditorInterface.get_edited_scene_root()
	mi.global_transform = terrain.global_transform * Transform3D(Basis(), Vector3(ext * 0.5, water_level.value, ext * 0.5))
	status.text = "Water surface added at height %.1f." % water_level.value


func _on_add_lake() -> void:
	if not _need_terrain():
		return
	var ext: float = terrain.get_extent()
	var spot := last_hit if has_hit else Vector3(ext * 0.5, 0, ext * 0.5)
	var mi := MeshInstance3D.new()
	mi.name = "Lake"
	var disc := CylinderMesh.new()
	disc.top_radius = lake_radius.value
	disc.bottom_radius = lake_radius.value
	disc.height = 0.05
	mi.mesh = disc
	mi.material_override = _water_material()
	terrain.get_parent().add_child(mi)
	mi.owner = EditorInterface.get_edited_scene_root()
	mi.global_transform = terrain.global_transform * Transform3D(Basis(), Vector3(spot.x, water_level.value, spot.z))
	status.text = "Lake added."


func _selected_3d() -> Array:
	var out := []
	for n in EditorInterface.get_selection().get_selected_nodes():
		if n is Node3D and n != terrain:
			out.append(n)
	return out


func _on_snap_grid() -> void:
	var items := _selected_3d()
	for n in items:
		n.position = n.position.snapped(Vector3.ONE * grid_spin.value)
	status.text = "Snapped %d object(s) to the grid." % items.size()


func _on_snap_surface() -> void:
	if not _need_terrain():
		return
	var items := _selected_3d()
	for n in items:
		var local: Vector3 = terrain.to_local(n.global_position)
		var h: float = terrain.get_height_at(Vector2(local.x, local.z))
		n.global_position = terrain.to_global(Vector3(local.x, h, local.z))
	status.text = "Placed %d object(s) on the terrain surface." % items.size()


func _on_duplicate() -> void:
	var root := EditorInterface.get_edited_scene_root()
	var made := 0
	for n in _selected_3d():
		var copy: Node3D = n.duplicate()
		n.get_parent().add_child(copy)
		copy.owner = root
		copy.position += Vector3(grid_spin.value, 0, grid_spin.value)
		made += 1
	status.text = "Duplicated %d object(s)." % made


func _on_group() -> void:
	var g := group_edit.text.strip_edges()
	if g == "":
		status.text = "Enter a group name."
		return
	var items := _selected_3d()
	for n in items:
		n.add_to_group(StringName(g), true)
	status.text = "Added %d object(s) to group '%s'." % [items.size(), g]
