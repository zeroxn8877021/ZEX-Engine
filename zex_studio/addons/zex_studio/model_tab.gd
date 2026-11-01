@tool
extends HSplitContainer
## 3D Model Previewer: GLB/GLTF (and imported FBX/OBJ) preview with orbit/pan/zoom, animations, skeleton, materials and model info.

const UI := preload("res://addons/zex_studio/ui.gd")

var path_edit: LineEdit
var info: Label
var status: Label
var anim_list: ItemList
var skeleton_check: CheckButton
var tex_rect: TextureRect
var container: SubViewportContainer
var viewport: SubViewport
var pivot: Node3D
var cam: Camera3D
var model: Node3D
var anim_player: AnimationPlayer
var skeletons: Array = []
var skel_lines: MeshInstance3D
var yaw := 0.6
var pitch := 0.3
var dist := 4.0
var target := Vector3.ZERO
var logger := ZexLog.new()


func _ready() -> void:
	var left := VBoxContainer.new()
	left.custom_minimum_size = Vector2(300, 0)
	add_child(left)
	path_edit = UI.line("res://model.glb", "res:// path or file path")
	left.add_child(UI.row("Model", path_edit))
	left.add_child(UI.button("Load / preview", _on_load_pressed))
	left.add_child(UI.button("Import into project (res://models)", _on_import_pressed))
	var hint := UI.status_label()
	hint.text = "Drag a .glb / .gltf file onto the editor window to preview and import it. FBX and OBJ files: import them into the project and use their res:// path."
	left.add_child(hint)
	skeleton_check = CheckButton.new()
	skeleton_check.text = "Show skeleton"
	skeleton_check.toggled.connect(func(_on: bool): _rebuild_skeleton_lines())
	left.add_child(skeleton_check)
	var bar := HBoxContainer.new()
	bar.add_child(UI.button("Stop animation", _on_stop_pressed))
	bar.add_child(UI.button("Reset view", _frame_model))
	left.add_child(bar)
	anim_list = ItemList.new()
	anim_list.custom_minimum_size = Vector2(0, 90)
	anim_list.item_activated.connect(_on_animation_activated)
	left.add_child(anim_list)
	tex_rect = TextureRect.new()
	tex_rect.custom_minimum_size = Vector2(0, 96)
	tex_rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	tex_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	left.add_child(tex_rect)
	info = UI.status_label()
	left.add_child(info)
	status = UI.status_label()
	left.add_child(status)

	container = SubViewportContainer.new()
	container.stretch = true
	container.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	container.custom_minimum_size = Vector2(300, 300)
	add_child(container)
	viewport = SubViewport.new()
	viewport.own_world_3d = true
	viewport.size = Vector2i(512, 512)
	container.add_child(viewport)
	pivot = Node3D.new()
	viewport.add_child(pivot)
	cam = Camera3D.new()
	viewport.add_child(cam)
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-50, 30, 0)
	viewport.add_child(light)
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.12, 0.12, 0.14)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.6, 0.6, 0.65)
	var we := WorldEnvironment.new()
	we.environment = env
	viewport.add_child(we)
	container.gui_input.connect(_on_view_input)
	_update_camera()
	get_window().files_dropped.connect(_on_files_dropped)


func _exit_tree() -> void:
	if get_window() != null and get_window().files_dropped.is_connected(_on_files_dropped):
		get_window().files_dropped.disconnect(_on_files_dropped)


func _update_camera() -> void:
	var offset := Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)) * dist
	cam.position = target + offset
	cam.look_at(target)


func _pan(delta: Vector2) -> void:
	target += (-cam.global_transform.basis.x * delta.x + cam.global_transform.basis.y * delta.y) * dist * 0.002
	_update_camera()


func _on_view_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		if event.button_mask & MOUSE_BUTTON_MASK_LEFT:
			yaw -= event.relative.x * 0.01
			pitch = clampf(pitch + event.relative.y * 0.01, -1.4, 1.4)
			_update_camera()
		elif event.button_mask & (MOUSE_BUTTON_MASK_RIGHT | MOUSE_BUTTON_MASK_MIDDLE):
			_pan(event.relative)
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			dist = maxf(0.2, dist * 0.9)
			_update_camera()
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			dist *= 1.1
			_update_camera()
	elif event is InputEventMagnifyGesture:
		dist = maxf(0.2, dist / event.factor)
		_update_camera()
	elif event is InputEventPanGesture:
		_pan(event.delta * 12.0)


func _on_load_pressed() -> void:
	load_model(path_edit.text)


func _on_files_dropped(files: PackedStringArray) -> void:
	if files.is_empty():
		return
	var path := files[0]
	var ext := path.get_extension().to_lower()
	if not (ext in ["glb", "gltf", "fbx", "obj"]):
		status.text = "Unsupported file type: .%s" % ext
		return
	path_edit.text = path
	_import_file(path)
	if ext == "glb" or ext == "gltf":
		load_model(path)


func _on_import_pressed() -> void:
	var p := path_edit.text.strip_edges()
	if p.begins_with("res://"):
		status.text = "That file is already inside the project."
	else:
		_import_file(p)


func _import_file(src: String) -> void:
	var dir := ProjectSettings.globalize_path("res://models")
	DirAccess.make_dir_recursive_absolute(dir)
	var dest := dir.path_join(src.get_file())
	var err := DirAccess.copy_absolute(src, dest)
	var project_id := String(ProjectSettings.get_setting("application/config/name", "project"))
	if err == OK:
		EditorInterface.get_resource_filesystem().scan()
		logger.record_operation(dest, src.get_extension().to_lower(), "import_model", project_id, "ok")
		status.text = "Imported into res://models/%s (it appears in the FileSystem dock)." % src.get_file()
	else:
		logger.record_operation(src, src.get_extension().to_lower(), "import_model", project_id, "error", "copy failed (%d)" % err)
		status.text = "Import failed (error %d)." % err


func load_model(path: String) -> void:
	path = path.strip_edges()
	var ext := path.get_extension().to_lower()
	var root: Node3D = null
	if path.begins_with("res://"):
		var res = load(path)
		if res is PackedScene:
			root = res.instantiate() as Node3D
		elif res is Mesh:
			var mi := MeshInstance3D.new()
			mi.mesh = res
			root = mi
	elif ext == "glb" or ext == "gltf":
		var doc := GLTFDocument.new()
		var state := GLTFState.new()
		if doc.append_from_file(path, state) == OK:
			root = doc.generate_scene(state) as Node3D
	else:
		status.text = "Outside the project only .glb / .gltf can be previewed. Import FBX / OBJ files into the project and use a res:// path."
		return
	if root == null:
		status.text = "Could not load that model."
		return
	_set_model(root)


func _set_model(root: Node3D) -> void:
	if model != null:
		model.queue_free()
	model = root
	pivot.add_child(model)
	var stats := {"meshes": 0, "surfaces": 0, "vertices": 0, "triangles": 0, "bones": 0, "materials": {}, "textures": {}}
	skeletons.clear()
	anim_player = null
	_collect(model, stats)
	anim_list.clear()
	if anim_player != null:
		for a in anim_player.get_animation_list():
			anim_list.add_item(String(a))
	var size := Vector3.ZERO
	if stats.has("aabb"):
		size = (stats["aabb"] as AABB).size
	info.text = "Meshes: %d  Surfaces: %d\nVertices: %d  Triangles: %d\nMaterials: %d  Textures: %d\nBones: %d  Animations: %d\nSize: %.2f x %.2f x %.2f" % [
		stats["meshes"], stats["surfaces"], stats["vertices"], stats["triangles"],
		stats["materials"].size(), stats["textures"].size(), stats["bones"], anim_list.item_count, size.x, size.y, size.z]
	if stats["materials"].size() > 0:
		info.text += "\nMaterial names: " + ", ".join(stats["materials"].values())
	tex_rect.texture = null
	for tex in stats["textures"].values():
		tex_rect.texture = tex
		break
	if stats.has("aabb"):
		target = (stats["aabb"] as AABB).get_center()
		dist = maxf(size.length() * 1.3, 0.5)
	_update_camera()
	_rebuild_skeleton_lines()
	status.text = "Loaded. Drag to orbit, pinch or wheel to zoom, two-finger drag or right mouse to pan."


func _collect(n: Node, out: Dictionary) -> void:
	if n is MeshInstance3D and n.mesh != null:
		var mi := n as MeshInstance3D
		out["meshes"] += 1
		for s in mi.mesh.get_surface_count():
			var arrays := mi.mesh.surface_get_arrays(s)
			var verts: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
			out["surfaces"] += 1
			out["vertices"] += verts.size()
			var idx = arrays[Mesh.ARRAY_INDEX]
			if idx != null and idx.size() > 0:
				out["triangles"] += int(idx.size() / 3)
			else:
				out["triangles"] += int(verts.size() / 3)
			var mat := mi.get_active_material(s)
			if mat != null:
				out["materials"][mat.get_instance_id()] = mat.resource_name if mat.resource_name != "" else "material"
				if mat is BaseMaterial3D and (mat as BaseMaterial3D).albedo_texture != null:
					var tex := (mat as BaseMaterial3D).albedo_texture
					out["textures"][tex.get_instance_id()] = tex
		var box: AABB = mi.global_transform * mi.get_aabb()
		out["aabb"] = (out["aabb"] as AABB).merge(box) if out.has("aabb") else box
	if n is Skeleton3D:
		out["bones"] += (n as Skeleton3D).get_bone_count()
		skeletons.append(n)
	if n is AnimationPlayer and anim_player == null:
		anim_player = n
	for c in n.get_children():
		_collect(c, out)


func _frame_model() -> void:
	yaw = 0.6
	pitch = 0.3
	if model != null:
		var stats := {"meshes": 0, "surfaces": 0, "vertices": 0, "triangles": 0, "bones": 0, "materials": {}, "textures": {}}
		skeletons.clear()
		anim_player = null
		_collect(model, stats)
		if stats.has("aabb"):
			target = (stats["aabb"] as AABB).get_center()
			dist = maxf((stats["aabb"] as AABB).size.length() * 1.3, 0.5)
	_update_camera()


func _on_animation_activated(index: int) -> void:
	if anim_player != null:
		anim_player.play(anim_list.get_item_text(index))
		_rebuild_skeleton_lines()


func _on_stop_pressed() -> void:
	if anim_player != null:
		anim_player.stop()


func _rebuild_skeleton_lines() -> void:
	if skel_lines != null:
		skel_lines.queue_free()
		skel_lines = null
	if not skeleton_check.button_pressed or skeletons.is_empty():
		return
	var im := ImmediateMesh.new()
	im.surface_begin(Mesh.PRIMITIVE_LINES)
	for sk in skeletons:
		for i in sk.get_bone_count():
			var parent: int = sk.get_bone_parent(i)
			if parent >= 0:
				im.surface_add_vertex(sk.global_transform * sk.get_bone_global_pose(parent).origin)
				im.surface_add_vertex(sk.global_transform * sk.get_bone_global_pose(i).origin)
	im.surface_end()
	skel_lines = MeshInstance3D.new()
	skel_lines.mesh = im
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.albedo_color = Color(1.0, 0.9, 0.2)
	mat.no_depth_test = true
	skel_lines.material_override = mat
	viewport.add_child(skel_lines)
