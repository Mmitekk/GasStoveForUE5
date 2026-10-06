# Run INSIDE THE TARGET project's editor:
#   Tools -> Execute Python Script -> pick this file (browse to the pack:
#   H:/Unreal Projects/UE52VFX/Python/ or the git clone).
# Installs the pack into THE CURRENTLY OPEN project. Nothing to configure,
# no paths, no numbers. After install: Ctrl+Alt+F11 (Live Coding) to compile.
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import install_gas_burner as inst

# Pack payload locations (first hit wins, except the open project itself).
SOURCE_CANDIDATES = [
    os.path.dirname(_HERE),
    r"H:\Unreal Projects\UE52VFX",
    r"D:\GitHub\GasStoveUE5",
]
PAYLOAD_PROBE = os.path.join("Source", "UE52VFX", "GasStoveBurnerFlame.h")


def _current_uproject():
    try:
        import unreal
        path = unreal.Paths.get_project_file_path()
        if path:
            return os.path.abspath(path)
    except Exception as e:
        print("[GasBurner] cannot detect open project: %s" % e)
    return ""


def _find_source(exclude_dir):
    for root in SOURCE_CANDIDATES:
        if not root or not os.path.isfile(os.path.join(root, PAYLOAD_PROBE)):
            continue
        if exclude_dir and os.path.abspath(root) == exclude_dir:
            continue
        return os.path.abspath(root)
    return None


tgt = _current_uproject()
if not tgt or not os.path.isfile(tgt):
    print("[GasBurner] Open a project first, then run this script from its editor.")
else:
    print("[GasBurner] This project: %s" % tgt)
    proj_dir = os.path.dirname(tgt)
    source = _find_source(proj_dir)
    if source is None:
        print("[GasBurner] Pack payload not found (UE52VFX project / git clone missing).")
    else:
        print("[GasBurner] Pack source: %s" % source)
        try:
            inst.install_from(source, tgt, update_uproject=False)
            print("[GasBurner] DONE here. Next: Ctrl+Alt+F11 (Live Coding compile),")
            print("[GasBurner] then drag 'Gas Stove Burner Flame' onto a burner.")
        except inst.InstallError as e:
            print("[GasBurner] ERROR: %s" % e)
        except Exception as e:
            print("[GasBurner] ERROR: %s" % e)
