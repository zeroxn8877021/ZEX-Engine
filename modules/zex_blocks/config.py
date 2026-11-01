import os
import runpy


def _apply_zex_branding():
    """Run the ZEX branding step once per build (it is idempotent). See misc/zex/apply_branding.py.

    It renames the engine and Android app, rebrands the editor UI and About dialog (keeping every Godot
    license credit), fixes Android landscape, and generates the ZEX icons when Pillow is installed.
    """
    if os.environ.get("ZEX_BRANDING_DONE") == "1":
        return
    os.environ["ZEX_BRANDING_DONE"] = "1"
    script = os.path.join("misc", "zex", "apply_branding.py")
    if not os.path.exists(script):
        return
    try:
        runpy.run_path(script, run_name="__main__")
    except Exception as e:  # Never break the build because of branding.
        print("ZEX branding step failed (continuing with an unbranded build): %s" % e)


def can_build(env, platform):
    _apply_zex_branding()
    return True


def configure(env):
    pass
