# setup_gas_material.py — full M_GasFlame builder (UE 5.2).
# Run: Tools -> Execute Python Script. Delegates to build_gas_stove_vfx.build_material().
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

try:
    from build_gas_stove_vfx import build_material as _build
except Exception as e:
    print("[GasVFX] import builder failed: %s" % e)
    _build = None


def setup_gas_flame_material():
    if _build is None:
        print("[GasVFX] builder missing, run Python/build_gas_stove_vfx.py directly.")
        return None
    return _build()


if __name__ == "__main__":
    setup_gas_flame_material()
