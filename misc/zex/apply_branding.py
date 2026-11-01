#!/usr/bin/env python3
"""ZEX INTERACTIVE branding step. Run from the repository root before building (the GitHub workflow does this).

It is idempotent: running it twice changes nothing the second time. It
  * renames the engine and Android app, fixes Android orientation (landscape works),
  * replaces "Godot" with "ZEX" in editor UI strings and rewrites the About dialog (keeping all Godot license credits),
  * generates the ZEX launcher icons, splash, app icon and editor logo from misc/zex/branding/zex_logo.svg.
"""
import glob
import os
import re
import sys

ROOT = os.getcwd()
changed = []


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def patch(path, old, new, regex=False):
    """Replace old with new if present. Missing file or already patched -> no-op."""
    if not os.path.exists(path):
        return
    text = read(path)
    if regex:
        new_text, n = re.subn(old, new, text, count=1, flags=re.M)
    else:
        n = 1 if old in text else 0
        new_text = text.replace(old, new, 1) if n else text
    if n and new_text != text:
        write(path, new_text)
        changed.append(path)


# ---------------------------------------------------------------- engine + Android
patch("version.py", 'name = "Godot Engine"', 'name = "ZEX INTERACTIVE Engine"')
patch("platform/android/java/editor/build.gradle", 'applicationId "org.godotengine.editor.v4"', 'applicationId "com.zexinteractive.engine"')
patch("platform/android/java/editor/build.gradle", 'editorAppName: "Godot Engine 4"', 'editorAppName: "ZEX INTERACTIVE"')
patch("platform/android/java/editor/src/main/res/values/strings.xml", "Godot Play window", "ZEX Play window")
patch("platform/android/java/editor/src/main/AndroidManifest.xml", 'android:screenOrientation="user">', 'android:screenOrientation="fullSensor">')
kt = "platform/android/java/editor/src/main/java/org/godotengine/editor/BaseGodotEditor.kt"
patch(kt, "requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE", "requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE")
patch(kt, r"requestedOrientation = ActivityInfo\.SCREEN_ORIENTATION_USER$", "requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR", regex=True)

# ---------------------------------------------------------------- editor UI strings
keep = r"(?! Physics| [0-9]| %s|'|\.x)"
word = re.compile(r"(?<![\w./\"-])Godot" + keep + r"(?![A-Za-z_./])")
bulk = 0
for f in glob.glob("editor/**/*.cpp", recursive=True):
    if "project_upgrade" in f:
        continue
    lines = read(f).split("\n")
    dirty = False
    for i, line in enumerate(lines):
        if re.search(r"\b(TTR|TTRC|TTRN|RTR)\(", line) and "Godot" in line and "godotengine" not in line and "online documentation" not in line and "based on Godot Engine" not in line and not line.strip().startswith("//"):
            new = word.sub("ZEX", line.replace("Godot Engine", "ZEX Engine"))
            if new != line:
                lines[i] = new
                dirty = True
                bulk += 1
    if dirty:
        write(f, "\n".join(lines))
        changed.append(f)

# Documentation lines really are Godot's documentation.
sd = "editor/script/script_editor_plugin.cpp"
patch(sd, "in ZEX online documentation.", "in Godot online documentation.")
patch(sd, 'TTRC("Open ZEX online documentation.")', 'TTRC("Open Godot online documentation.")')

# ---------------------------------------------------------------- About dialog (Godot credits and license are kept)
ab = "editor/gui/editor_about.cpp"
patch(ab, '''String(U"© 2014-present ") + TTR("ZEX Engine contributors") + ".\\n" +
					String(U"© 2007-2014 Juan Linietsky, Ariel Manzur.\\n"));''',
      '''String(U"© 2026 ZEX INTERACTIVE.\\n") +
					String(U"Based on Godot Engine: © 2014-present Godot Engine contributors,\\n") +
					String(U"© 2007-2014 Juan Linietsky, Ariel Manzur.\\n"));''')
patch(ab, 'set_title(TTRC("Thanks from the ZEX community!"));', 'set_title(TTRC("About ZEX INTERACTIVE Engine"));')
patch(ab, 'TTRC("ZEX Engine relies on a number of third-party free and open source libraries, all compatible with the terms of its MIT license.',
      'TTRC("ZEX INTERACTIVE Engine is based on Godot Engine and relies on a number of third-party free and open source libraries, all compatible with the terms of the MIT license.')
patch(ab, 'sc->set_name(TTRC("Authors"));', 'sc->set_name(TTRC("Godot Authors"));')
patch(ab, 'sc->set_name(TTRC("Donors"));', 'sc->set_name(TTRC("Godot Donors"));')
if os.path.exists(ab) and "// ZEX INTERACTIVE tab" not in read(ab):
    patch(ab, '''	{
		ScrollContainer *sc = memnew(ScrollContainer);
		sc->set_name(TTRC("Godot Authors"));''', '''	{
		// ZEX INTERACTIVE tab (first tab).
		ScrollContainer *sc = memnew(ScrollContainer);
		sc->set_name("ZEX INTERACTIVE");
		sc->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		sc->set_v_size_flags(Control::SIZE_EXPAND);
		tc->add_child(sc);

		VBoxContainer *vb = memnew(VBoxContainer);
		vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		sc->add_child(vb);

		Label *zex_title = memnew(Label);
		zex_title->set_text("ZEX INTERACTIVE Engine");
		zex_title->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		zex_title->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		vb->add_child(zex_title);

		Label *zex_text = memnew(Label);
		zex_text->set_text(
				"An Android-first game engine and editor.\\n\\n"
				"Created and maintained by ZEX INTERACTIVE.\\n\\n"
				"ZEX INTERACTIVE Engine is built on Godot Engine, which is free and open source software released under the MIT license. "
				"Godot Engine is Copyright (c) 2014-present Godot Engine contributors and (c) 2007-2014 Juan Linietsky, Ariel Manzur. "
				"The full license text is available in the License tab, and the people who built Godot are credited in the Godot Authors and Godot Donors tabs.");
		zex_text->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		zex_text->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
		zex_text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		zex_text->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		zex_text->set_custom_minimum_size(Size2(360, 0) * EDSCALE);
		vb->add_child(zex_text);
	}

	{
		ScrollContainer *sc = memnew(ScrollContainer);
		sc->set_name(TTRC("Godot Authors"));''')

patch("editor/editor_node.cpp", 'TTRC("About Godot...")', 'TTRC("About ZEX INTERACTIVE Engine...")')
patch("editor/editor_node.cpp", 'String godot_version = "Godot v"', 'String godot_version = "ZEX v"')
patch("editor/editor_log.cpp", 'GODOT_VERSION_FULL_NAME " (c) 2007-present Juan Linietsky, Ariel Manzur & Godot Contributors."',
      'GODOT_VERSION_FULL_NAME " (c) 2026 ZEX INTERACTIVE. Based on Godot Engine (c) 2007-present Juan Linietsky, Ariel Manzur & Godot Contributors."')
patch("main/main.cpp", 'print_help_copyright("(c) 2014-present Godot Engine contributors. (c) 2007-present Juan Linietsky, Ariel Manzur.");',
      'print_help_copyright("(c) 2026 ZEX INTERACTIVE. Based on Godot Engine: (c) 2014-present Godot Engine contributors. (c) 2007-present Juan Linietsky, Ariel Manzur.");')

# ---------------------------------------------------------------- small fixes in ZEX modules
ap = "modules/zex_cloud/zex_auth_panel.cpp"
if os.path.exists(ap) and 'load_config("user://zex_cloud.cfg")' not in read(ap):
    patch(ap, "		// Resume a saved session if there is one; works offline because nothing is sent.\n",
          '		// Look for the (public) cloud config; the engine keeps working offline without it.\n'
          '		if (auth->load_config("user://zex_cloud.cfg") != OK) {\n'
          '			auth->load_config("res://zex_cloud.cfg");\n'
          '		}\n'
          "		// Resume a saved session if there is one; works offline because nothing is sent.\n")
patch("modules/zex_world_studio/zex_foliage.cpp", "rng.randf() * Math::TAU : 0.0f", "(float)(rng.randf() * Math::TAU) : 0.0f")

# ---------------------------------------------------------------- icons, splash, logo from the ZEX SVG
svg_path = "misc/zex/branding/zex_logo.svg"
if os.path.exists(svg_path):
    svg = read(svg_path)
    # Editor logo icons (vector, same artwork).
    for dest, size in (("editor/icons/Godot.svg", 16), ("editor/icons/Logo.svg", 96)):
        if os.path.exists(os.path.dirname(dest)):
            out = re.sub(r'width="\d+" height="\d+"', 'width="%d" height="%d"' % (size, size), svg, count=1)
            if not os.path.exists(dest) or read(dest) != out:
                write(dest, out)
                changed.append(dest)
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("Pillow is not installed; skipping raster icon generation (pip install pillow).")
        Image = None
    if Image is not None:
        d = re.search(r' d="([^"]+)"', svg).group(1)
        pts = [tuple(map(float, p.split())) for p in re.findall(r"[ML]([\d.\-]+ [\d.\-]+)", d)]
        S = 1024  # supersampled master (artwork coordinates are 0..512)

        def master(z_color=(0, 0, 0, 255), circle=True):
            im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
            dr = ImageDraw.Draw(im)
            if circle:
                dr.ellipse((4, 4, S - 5, S - 5), fill=(255, 255, 255, 255))
            dr.polygon([(x * 2, y * 2) for x, y in pts], fill=z_color)
            return im

        full = master()
        mono = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        mdr = ImageDraw.Draw(mono)
        mdr.polygon([(x * 2, y * 2) for x, y in pts], fill=(255, 255, 255, 255))

        res = "platform/android/java/lib/src/main/res/"
        for f in glob.glob(res + "mipmap*/icon_background.webp"):
            s = Image.open(f).size[0]
            Image.new("RGB", (s, s), (0, 0, 0)).save(f, "WEBP", lossless=True)
        for kind, src in (("foreground", full), ("monochrome", mono)):
            for f in glob.glob(res + "mipmap*/icon_%s.webp" % kind):
                s = Image.open(f).size[0]
                dsz = int(s * 0.62)
                canvas = Image.new("RGBA", (s, s), (0, 0, 0, 0))
                small = src.resize((dsz, dsz), Image.LANCZOS)
                canvas.paste(small, ((s - dsz) // 2, (s - dsz) // 2), small)
                canvas.save(f, "WEBP", lossless=True)
        if os.path.exists("main/splash.png"):
            sp = Image.new("RGBA", (800, 600), (0, 0, 0, 255))
            logo = full.resize((420, 420), Image.LANCZOS)
            sp.paste(logo, (190, 90), logo)
            sp.save("main/splash.png")
        if os.path.exists("main/app_icon.png"):
            size = Image.open("main/app_icon.png").size
            full.resize(size, Image.LANCZOS).save("main/app_icon.png")
        print("ZEX icons, splash and app icon generated.")

print("ZEX branding applied. Touched: %d item(s), UI string lines: %d" % (len(changed), bulk))
