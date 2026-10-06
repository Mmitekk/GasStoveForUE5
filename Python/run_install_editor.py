# Run INSIDE Unreal Editor: Tools -> Execute Python Script -> run_install_editor.py
# Installs this pack into ANOTHER (closed!) UE project.
#
# How to pick the target (first non-comment line of install_target.txt):
#   - a number  -> installs into N-th project from the auto-scan list below
#   - full path -> installs there, e.g.  D:\MyProject\MyProject.uproject
# First run with empty config just prints the numbered list of found projects.
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import install_gas_burner as inst
try:
    # Editor caches python modules per session: force fresh code on re-runs.
    import importlib
    inst = importlib.reload(inst)
except Exception:
    pass

# Where to look for UE projects (extra roots via "scan: <dir>" lines in config).
SCAN_ROOTS = [r"H:\Unreal Projects", r"D:\Unreal Projects", r"D:\GitHub"]

# Pack payload locations (first hit wins): this project, dev project, git clone.
SOURCE_CANDIDATES = [
    os.path.dirname(_HERE),
    r"H:\Unreal Projects\UE52VFX",
    r"D:\GitHub\GasStoveUE5",
]
PAYLOAD_PROBE = os.path.join("Source", "UE52VFX", "GasStoveBurnerFlame.h")


def _config_lines():
    cfg = os.path.join(_HERE, "install_target.txt")
    if not os.path.isfile(cfg):
        return [], cfg
    try:
        with open(cfg, encoding="utf-8") as f:
            return [l.strip() for l in f], cfg
    except Exception as e:
        print("[GasBurner] cannot read install_target.txt: %s" % e)
    return [], cfg


def _scan_projects(extra_roots):
    found = []
    for root in list(SCAN_ROOTS) + list(extra_roots):
        if not root or not os.path.isdir(root):
            continue
        try:
            entries = sorted(os.listdir(root))
        except Exception:
            continue
        for entry in entries:
            full = os.path.join(root, entry)
            if not os.path.isdir(full):
                continue
            try:
                for fn in sorted(os.listdir(full)):
                    if fn.endswith(".uproject"):
                        found.append(os.path.join(full, fn))
            except Exception:
                continue
    # dedupe, keep order
    seen, out = set(), []
    for p in found:
        if p.lower() not in seen:
            seen.add(p.lower())
            out.append(p)
    return out


def _find_source():
    for root in SOURCE_CANDIDATES:
        if root and os.path.isfile(os.path.join(root, PAYLOAD_PROBE)):
            return root
    return None


lines, cfg = _config_lines()
extra_roots = [l.split(":", 1)[1].strip().strip('"') for l in lines
               if l.lower().startswith("scan:")]
choice = ""
for l in lines:
    if l and not l.startswith("#") and not l.lower().startswith("scan:"):
        choice = l.strip('"')
        break

projects = _scan_projects(extra_roots)
source = _find_source()
if source is None:
    print("[GasBurner] Pack payload not found (looked in: %s)." % SOURCE_CANDIDATES)
    print("[GasBurner] Run this script from the UE52VFX project or the git clone.")
else:
    if not choice:
        print("[GasBurner] Found UE projects (write the NUMBER into install_target.txt):")
        for i, p in enumerate(projects, 1):
            print("[GasBurner]   %d) %s" % (i, p))
        if not projects:
            print("[GasBurner]   (none found — check SCAN_ROOTS or add 'scan: <dir>' line)")
    else:
        target = choice
        if choice.isdigit():
            idx = int(choice) - 1
            if idx < 0 or idx >= len(projects):
                print("[GasBurner] Number %s out of range (1..%d)." % (choice, len(projects)))
                target = ""
            else:
                target = projects[idx]
        if target:
            src_dir = os.path.abspath(source)
            tgt_dir = os.path.abspath(os.path.join(os.path.dirname(target), ""))
            if tgt_dir == src_dir or tgt_dir.startswith(src_dir + os.sep):
                print("[GasBurner] Target is the pack itself — pick ANOTHER project.")
            else:
                print("[GasBurner] Source: %s" % src_dir)
                print("[GasBurner] Target: %s  (make sure it is CLOSED!)" % target)
                try:
                    inst.install_from(src_dir, target)
                except inst.InstallError as e:
                    print("[GasBurner] ERROR: %s" % e)
                except Exception as e:
                    print("[GasBurner] ERROR: %s" % e)
