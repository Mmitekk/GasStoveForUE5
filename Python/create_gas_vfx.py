# create_gas_vfx.py — full one-click VFX build (UE 5.2). Delegates to build_gas_stove_vfx.main().
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

try:
    from build_gas_stove_vfx import main as _main
except Exception as e:
    print("[GasVFX] import builder failed: %s" % e)
    _main = None


def create_gas_stove_vfx():
    if _main is None:
        print("[GasVFX] builder missing, run Python/build_gas_stove_vfx.py directly.")
        return
    return _main()


if __name__ == "__main__":
    create_gas_stove_vfx()
