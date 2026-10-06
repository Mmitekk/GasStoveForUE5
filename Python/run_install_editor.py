# Run INSIDE Unreal Editor: Tools -> Execute Python Script -> run_install_editor.py
# Installs this pack into ANOTHER (closed!) UE project.
# Target path: first non-comment line of install_target.txt next to this file.
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import install_gas_burner as inst


def _read_target():
    cfg = os.path.join(_HERE, "install_target.txt")
    if not os.path.isfile(cfg):
        return ""
    try:
        with open(cfg, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    return line.strip('"')
    except Exception as e:
        print("[GasBurner] cannot read install_target.txt: %s" % e)
    return ""


tgt = _read_target()
if not tgt:
    print("[GasBurner] Write the target .uproject path into install_target.txt first.")
else:
    try:
        inst.install_from(os.path.dirname(_HERE), tgt)
    except inst.InstallError as e:
        print("[GasBurner] ERROR: %s" % e)
    except Exception as e:
        print("[GasBurner] ERROR: %s" % e)
