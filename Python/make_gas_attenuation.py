# make_gas_attenuation.py — создаёт ATN_GasStove (Sound Attenuation) для конфорок:
# сфера ~20 см, затухание до 3.5 м, пространственный звук.
# Запуск: Tools -> Execute Python Script (или через UnrealEditor-Cmd).
import unreal

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/VFX/GasStove"
NAME = "ATN_GasStove"
PATH = "%s/%s" % (DIR, NAME)

if not EAL.does_directory_exist(DIR):
    EAL.make_directory(DIR)
if EAL.does_asset_exist(PATH):
    EAL.delete_asset(PATH)

factory = unreal.SoundAttenuationFactory()
att = AT.create_asset(NAME, DIR, unreal.SoundAttenuation, factory)
if att is None:
    print("[GASATN] ERROR: cannot create %s" % PATH)
else:
    s = att.get_editor_property("attenuation")
    for name, val in (
        ("attenuate", True),
        ("spatialize", True),
        ("attenuation_shape", unreal.AttenuationShape.SPHERE),
        ("attenuation_shape_extents", unreal.Vector(20.0, 20.0, 20.0)),
        ("falloff_distance", 350.0),
        ("distance_algorithm", unreal.AttenuationDistanceModel.NATURAL_SOUND),
    ):
        try:
            s.set_editor_property(name, val)
        except Exception as e:
            print("[GASATN] %s set failed: %s" % (name, e))
    att.set_editor_property("attenuation", s)
    EAL.save_asset(PATH, False)
    print("[GASATN] OK: %s (falloff 350cm, spatial)" % PATH)
