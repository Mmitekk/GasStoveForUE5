#!/usr/bin/env python3
"""Install AGasStoveBurnerFlame (gas stove blue flame) into another UE5 project.

Run with plain Windows Python (NOT inside Unreal):
    python install_gas_burner.py --target "D:\\MyProject\\MyProject.uproject"
or without args — will ask for the path interactively.

What it does:
  1. Reads target .uproject -> finds game module name (e.g. MyGame).
  2. Copies GasStoveBurnerFlame.h/.cpp into Source/<Module>/,
     replacing UE52VFX_API with <MODULE>_API.
  3. Adds "Niagara" to the module Build.cs if missing.
  4. Adds Niagara plugin entry to .uproject if missing.
  5. Copies materials + textures (PNG + .uasset) to Content/VFX/GasStove/.
  6. Copies helper scripts (build/diag) to <Target>/Python/.
Existing files are backed up as *.bak. Safe to re-run.
"""
import argparse
import json
import os
import re
import shutil
import sys

SRC_PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CODE_FILES = ["GasStoveBurnerFlame.h", "GasStoveBurnerFlame.cpp"]
CODE_SRC_DIR = os.path.join(SRC_PROJECT, "Source", "UE52VFX")
SRC_API_MACRO = "UE52VFX_API"

CONTENT_PAYLOAD = [
    "Content/VFX/GasStove/M_GasFlame.uasset",
    "Content/VFX/GasStove/M_GasGlow.uasset",
    "Content/VFX/GasStove/Textures/T_GasFlame_Tongue.png",
    "Content/VFX/GasStove/Textures/T_GasFlame_Glow.png",
    "Content/VFX/GasStove/Textures/T_FlameNoise.png",
    "Content/VFX/GasStove/Textures/T_GasFlame_Tongue.uasset",
    "Content/VFX/GasStove/Textures/T_GasFlame_Glow.uasset",
    "Content/VFX/GasStove/Textures/T_FlameNoise.uasset",
]

HELPER_SCRIPTS = ["build_gas_stove_vfx.py", "diag_gas_flame.py"]


class InstallError(Exception):
    pass


def fail(msg):
    raise InstallError(msg)


def copy_with_backup(src, dst):
    if not os.path.isfile(src):
        print("  SKIP (missing in source): %s" % src)
        return False
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.path.isfile(dst):
        shutil.copy2(dst, dst + ".bak")
        print("  backup: %s.bak" % dst)
    shutil.copy2(src, dst)
    # clear read-only flag just in case (Perforce etc.)
    try:
        os.chmod(dst, 0o666)
    except Exception:
        pass
    print("  copied: %s" % dst)
    return True


def install_from(source_root, target):
    """Install the pack from source_root (git clone or UE52VFX project) into
    target .uproject. Returns True. Raises InstallError on problems.
    Import-safe: no input()/sys.exit() inside (editor-friendly)."""
    if not target or not os.path.isfile(target):
        raise InstallError("target .uproject not found: %r" % target)
    if not target.lower().endswith(".uproject"):
        raise InstallError("not a .uproject file: %r" % target)
    code_src_dir = os.path.join(source_root, "Source", "UE52VFX")

    proj_dir = os.path.dirname(os.path.abspath(target))
    with open(target, "r", encoding="utf-8") as f:
        try:
            uproj = json.load(f)
        except Exception as e:
            fail("cannot parse .uproject: %s" % e)

    modules = uproj.get("Modules") or []
    runtime = [m for m in modules if m.get("Type") == "Runtime"] or modules
    if not runtime:
        fail("no modules in .uproject")
    mod_name = runtime[0]["Name"]
    mod_macro = mod_name.upper() + "_API"
    print("Target module: %s  (API macro %s)" % (mod_name, mod_macro))

    eng = uproj.get("EngineAssociation", "?")
    print("Target engine: %s" % eng)
    if str(eng) != "5.2":
        print("WARNING: source project is UE 5.2; .uasset copies are safest on 5.2.")

    # ---- 1. code ----
    print("== Code ==")
    mod_src = os.path.join(proj_dir, "Source", mod_name)
    if not os.path.isdir(mod_src):
        fail("module source dir missing: %s" % mod_src)
    for fn in CODE_FILES:
        src = os.path.join(code_src_dir, fn)
        if not os.path.isfile(src):
            fail("source file missing: %s" % src)
        dst = os.path.join(mod_src, fn)
        if os.path.isfile(dst):
            shutil.copy2(dst, dst + ".bak")
            print("  backup: %s.bak" % dst)
        with open(src, "r", encoding="utf-8") as f:
            text = f.read()
        text = re.sub(r"\b%s\b" % re.escape(SRC_API_MACRO), mod_macro, text)
        with open(dst, "w", encoding="utf-8", newline="") as f:
            f.write(text)
        print("  installed (+macro %s): %s" % (mod_macro, dst))

    # ---- 2. Build.cs ----
    print("== Build.cs ==")
    build_files = [os.path.join(mod_src, fn) for fn in os.listdir(mod_src)
                   if fn.endswith(".Build.cs")]
    if not build_files:
        print("  WARNING: no .Build.cs found, add \"Niagara\" to dependencies manually.")
    for bf in build_files:
        with open(bf, "r", encoding="utf-8") as f:
            bt = f.read()
        if '"Niagara"' in bt or "'Niagara'" in bt:
            print("  Niagara already in %s" % bf)
            continue
        m = re.search(r"(PublicDependencyModuleNames\.AddRange\(new string\[\]\s*\{)([^}]*)\}", bt)
        if not m:
            print("  WARNING: cannot patch %s, add \"Niagara\" manually." % bf)
            continue
        bt = bt[:m.end(2)] + ', "Niagara"' + bt[m.end(2):]
        shutil.copy2(bf, bf + ".bak")
        with open(bf, "w", encoding="utf-8", newline="") as f:
            f.write(bt)
        print("  patched +Niagara: %s" % bf)

    # ---- 3. .uproject plugins ----
    print("== Plugins ==")
    plugins = uproj.get("Plugins") or []
    if not any(p.get("Name") == "Niagara" for p in plugins):
        plugins.append({"Name": "Niagara", "Enabled": True})
        uproj["Plugins"] = plugins
        shutil.copy2(target, target + ".bak")
        with open(target, "w", encoding="utf-8") as f:
            json.dump(uproj, f, indent="\t")
            f.write("\n")
        print("  added Niagara plugin entry (+.uproject.bak)")
    else:
        print("  Niagara plugin entry present")

    # ---- 4. content ----
    print("== Content ==")
    ok = True
    for rel in CONTENT_PAYLOAD:
        src = os.path.join(source_root, *rel.split("/"))
        dst = os.path.join(proj_dir, *rel.split("/"))
        ok = copy_with_backup(src, dst) and ok

    # ---- 5. helper scripts ----
    print("== Helper scripts ==")
    for fn in HELPER_SCRIPTS:
        src = os.path.join(source_root, "Python", fn)
        dst = os.path.join(proj_dir, "Python", fn)
        copy_with_backup(src, dst)

    print("")
    print("DONE. Next steps in target project:")
    print("  1. Rebuild C++ (Visual Studio / Live Coding Ctrl+Alt+F11).")
    print("  2. Open editor, drag 'Gas Stove Burner Flame' onto a burner.")
    print("  3. bLit=false by default -> ignite via SetLit/SetGasLevel.")
    if str(eng) != "5.2":
        print("  4. Non-5.2 engine: delete copied M_*.uasset, copy PNGs only,")
        print("     then run Python/build_gas_stove_vfx.py in-editor to rebuild.")
    if not ok:
        print("NOTE: some source files were missing (see SKIP above).")
    return True


def main():
    ap = argparse.ArgumentParser(description="Install gas burner flame VFX into a UE project.")
    ap.add_argument("--target", help="Path to target .uproject file")
    ap.add_argument("--source", default=SRC_PROJECT,
                    help="Pack source root (this project or a git clone)")
    args = ap.parse_args()

    target = args.target
    if not target:
        try:
            target = input("Path to target .uproject: ").strip().strip('"')
        except EOFError:
            target = ""
    try:
        install_from(args.source, target)
    except InstallError as e:
        print("ERROR: %s" % e)
        sys.exit(1)


if __name__ == "__main__":
    main()
