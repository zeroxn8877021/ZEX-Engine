@tool
extends ScrollContainer
## ZEX Build Center: project packages (.zexproj), build packages (.zexbuild), export presets, validation, build log and history.

const UI := preload("res://addons/zex_studio/ui.gd")

var logger := ZexLog.new()
var vb: VBoxContainer
var status: Label
var pkg_path: LineEdit
var restore_dir: LineEdit
var build_source: LineEdit
var android_id: LineEdit
var version_name: LineEdit
var version_code: SpinBox
var preset_option: OptionButton
var out_edit: LineEdit
var history: ItemList
var presets: Array = []


func _ready() -> void:
	horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	vb = VBoxContainer.new()
	vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	add_child(vb)

	_section("Project packages (.zexproj): backup, share, restore")
	vb.add_child(UI.button("Backup this project now", _on_backup))
	pkg_path = UI.line("user://zex_backups/", "path of a .zexproj / .zexbuild file")
	vb.add_child(UI.row("Package file", pkg_path))
	vb.add_child(UI.button("Validate package (integrity check)", _on_validate))
	restore_dir = UI.line("user://restored_project", "folder to restore into")
	vb.add_child(UI.row("Restore into", restore_dir))
	vb.add_child(UI.button("Restore / import package", _on_restore))

	_section("Build packages (.zexbuild)")
	build_source = UI.line("user://builds", "folder with your exported build")
	vb.add_child(UI.row("Build folder", build_source))
	android_id = UI.line("com.example.mygame", "Android package id")
	vb.add_child(UI.row("Package id", android_id))
	version_name = UI.line("1.0.0", "version name")
	vb.add_child(UI.row("Version name", version_name))
	version_code = UI.spin(1, 2000000000, 1, 1)
	vb.add_child(UI.row("Version code", version_code))
	vb.add_child(UI.button("Create .zexbuild package", _on_make_zexbuild))

	_section("Export (APK / AAB / Windows / Linux / Web) using your export presets")
	preset_option = OptionButton.new()
	preset_option.custom_minimum_size = Vector2(0, 48)
	vb.add_child(UI.row("Preset", preset_option))
	out_edit = UI.line("user://builds/game.apk", "output file")
	vb.add_child(UI.row("Output", out_edit))
	var bar := HBoxContainer.new()
	bar.add_child(UI.button("Validate preset", _on_validate_preset))
	bar.add_child(UI.button("Export debug", _on_export_debug))
	bar.add_child(UI.button("Export release", _on_export_release))
	vb.add_child(bar)
	vb.add_child(UI.button("Reload presets", _load_presets))

	_section("Build history")
	history = ItemList.new()
	history.custom_minimum_size = Vector2(0, 160)
	vb.add_child(history)
	vb.add_child(UI.button("Refresh history", _refresh_history))

	status = UI.status_label()
	vb.add_child(status)
	_load_presets()
	_refresh_history()


func _section(title: String) -> void:
	var l := Label.new()
	l.text = title
	l.add_theme_font_size_override("font_size", 18)
	vb.add_child(HSeparator.new())
	vb.add_child(l)


func _project_id() -> String:
	return String(ProjectSettings.get_setting("application/config/name", "project"))


func _safe_name() -> String:
	return _project_id().validate_filename().replace(" ", "_")


func _record(operation: String, path: String, fmt: String, r: Dictionary) -> void:
	var ok: bool = r.get("ok", r.get("valid", false))
	var err := ""
	if not ok:
		err = String(r.get("error", ""))
		if r.has("errors") and not (r["errors"] as Array).is_empty():
			err = String((r["errors"] as Array)[0])
	logger.record_operation(path, fmt, operation, _project_id(), "ok" if ok else "error", err)


func _on_backup() -> void:
	var stamp := Time.get_datetime_string_from_system().replace(":", "-")
	var out := "user://zex_backups/%s_%s.zexproj" % [_safe_name(), stamp]
	var r: Dictionary = ZexPackage.create_package("res://", out, "zexproj", {"note": "manual backup"})
	_record("export_project", out, "zexproj", r)
	if r.get("ok", false):
		pkg_path.text = out
		status.text = "Backup created: %s\n%d files, SHA-256 %s" % [ProjectSettings.globalize_path(out), r["file_count"], String(r["sha256"]).left(16)]
	else:
		status.text = "Backup failed: " + String(r.get("error", ""))
	_refresh_history()


func _on_validate() -> void:
	var r: Dictionary = ZexPackage.validate_package(pkg_path.text.strip_edges())
	_record("import_validate", pkg_path.text, "zexproj", r)
	if r["valid"]:
		var m: Dictionary = r["manifest"]
		status.text = "Package is valid.\nKind: %s | files: %d | engine: %s" % [m.get("kind", ""), (m.get("files", []) as Array).size(), (m.get("engine", {}) as Dictionary).get("version", "")]
	else:
		status.text = "Package is NOT valid:\n" + "\n".join(r["errors"])


func _on_restore() -> void:
	var r: Dictionary = ZexPackage.extract_package(pkg_path.text.strip_edges(), restore_dir.text.strip_edges(), false)
	_record("import_restore", pkg_path.text, "zexproj", r)
	status.text = ("Restored %d files into %s" % [r["file_count"], ProjectSettings.globalize_path(restore_dir.text)]) if r["ok"] else "Restore failed: " + String(r["error"])


func _on_make_zexbuild() -> void:
	var pid := android_id.text.strip_edges()
	if not _valid_package_id(pid):
		status.text = "Package id must look like com.company.game (letters, digits, underscores; at least two parts)."
		return
	var out := "user://zex_builds/%s_%s.zexbuild" % [_safe_name(), version_name.text.strip_edges()]
	var meta := {"android_package": pid, "version_name": version_name.text.strip_edges(), "version_code": int(version_code.value)}
	var r: Dictionary = ZexPackage.create_package(build_source.text.strip_edges(), out, "zexbuild", meta)
	_record("export_build", out, "zexbuild", r)
	status.text = ("Build package created: %s" % ProjectSettings.globalize_path(out)) if r.get("ok", false) else "Build package failed: " + String(r.get("error", ""))
	_refresh_history()


func _valid_package_id(pid: String) -> bool:
	var re := RegEx.new()
	re.compile("^[a-zA-Z][a-zA-Z0-9_]*(\\.[a-zA-Z][a-zA-Z0-9_]*)+$")
	return re.search(pid) != null


func _load_presets() -> void:
	presets.clear()
	preset_option.clear()
	var cf := ConfigFile.new()
	if cf.load("res://export_presets.cfg") != OK:
		status.text = "No export presets yet. Create them in Project > Export, then press Reload presets."
		return
	for section in cf.get_sections():
		if section.begins_with("preset.") and not section.ends_with(".options"):
			var opts := {}
			var opt_section := section + ".options"
			if cf.has_section(opt_section):
				for k in cf.get_section_keys(opt_section):
					opts[k] = cf.get_value(opt_section, k)
			presets.append({"name": cf.get_value(section, "name", ""), "platform": cf.get_value(section, "platform", ""), "options": opts})
			preset_option.add_item("%s (%s)" % [cf.get_value(section, "name", ""), cf.get_value(section, "platform", "")])


func _validate(preset: Dictionary) -> Array:
	var problems := []
	var opts: Dictionary = preset["options"]
	if String(preset["platform"]) == "Android":
		if not _valid_package_id(String(opts.get("package/unique_name", "")).replace("$genname", "game")):
			problems.append("Android package id (package/unique_name) is missing or invalid.")
		if int(opts.get("version/code", 0)) < 1:
			problems.append("Version code must be 1 or higher.")
		if String(opts.get("version/name", "")) == "":
			problems.append("Version name is empty.")
		if not bool(opts.get("gradle_build/use_gradle_build", false)) and String(preset.get("name", "")).to_lower().contains("aab"):
			problems.append("AAB export needs 'Use Gradle Build' turned on.")
	return problems


func _current_preset() -> Dictionary:
	if preset_option.selected < 0 or preset_option.selected >= presets.size():
		return {}
	return presets[preset_option.selected]


func _on_validate_preset() -> void:
	var p := _current_preset()
	if p.is_empty():
		status.text = "No preset selected."
		return
	var problems := _validate(p)
	status.text = "Preset looks good." if problems.is_empty() else "Problems:\n- " + "\n- ".join(problems)


func _on_export_debug() -> void:
	_export(false)


func _on_export_release() -> void:
	_export(true)


func _export(release: bool) -> void:
	var p := _current_preset()
	if p.is_empty():
		status.text = "No preset selected."
		return
	var problems := _validate(p)
	if not problems.is_empty():
		logger.add_entry("build", "validation_failed", "error", {"preset": p["name"], "problems": problems})
		status.text = "Fix these first:\n- " + "\n- ".join(problems)
		_refresh_history()
		return
	var out := ProjectSettings.globalize_path(out_edit.text.strip_edges())
	DirAccess.make_dir_recursive_absolute(out.get_base_dir())
	var args := ["--headless", "--path", ProjectSettings.globalize_path("res://"), "--export-release" if release else "--export-debug", String(p["name"]), out]
	logger.add_entry("build", "export_started", "ok", {"preset": p["name"], "release": release})
	var output := []
	var code := OS.execute(OS.get_executable_path(), args, output, true)
	var text := "\n".join(output)
	if code == 0 and FileAccess.file_exists(out):
		logger.record_operation(out, out.get_extension(), "export_build", _project_id(), "ok")
		status.text = "Export finished: %s" % out
	else:
		logger.add_entry("build", "export_failed", "error", {"preset": p["name"], "exit_code": code, "output": text.left(1500)})
		status.text = "Export failed (exit code %d). On Android phones the editor cannot start a second editor process to export; use a PC or the GitHub build.\n%s" % [code, text.left(600)]
	_refresh_history()


func _refresh_history() -> void:
	history.clear()
	for e in logger.query({"category": "build", "limit": 25}):
		history.add_item("%s  %s  [%s]" % [e.get("iso", ""), e.get("action", ""), e.get("status", "")])
	for e in logger.query({"category": "export", "limit": 25}):
		history.add_item("%s  %s  [%s]" % [e.get("iso", ""), e.get("action", ""), e.get("status", "")])
