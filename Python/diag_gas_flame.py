# diag_gas_flame.py — диагностика черного пламени. Запуск: Tools -> Execute Python Script.
# Вывод скопировать и прислать. Ничего не меняет, только читает.
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def dump_mat(path):
    print("=== %s  exists=%s" % (path, EAL.does_asset_exist(path)))
    m = EAL.load_asset(path)
    if m is None:
        print("  LOAD FAILED")
        return
    for prop in ("blend_mode", "shading_model", "two_sided", "material_domain"):
        try:
            print("  %s = %s" % (prop, m.get_editor_property(prop)))
        except Exception as e:
            print("  %s read fail: %s" % (prop, e))
    try:
        print("  num_expressions = %s" % MEL.get_num_material_expressions(m))
    except Exception as e:
        print("  num_expressions fail: %s" % e)
    try:
        print("  used_textures = %s" % MEL.get_used_textures(m))
    except Exception as e:
        print("  used_textures fail: %s" % e)
    try:
        print("  scalar_params = %s" % MEL.get_scalar_parameter_names(m))
    except Exception as e:
        print("  scalar_params fail: %s" % e)
    for mp, name in ((unreal.MaterialProperty.MP_EMISSIVE_COLOR, "emissive"),
                     (unreal.MaterialProperty.MP_OPACITY, "opacity"),
                     (unreal.MaterialProperty.MP_BASE_COLOR, "base_color")):
        try:
            node = MEL.get_material_property_input_node(m, mp)
            print("  %s <- %s" % (name, node.get_class().get_name() if node else "NOTHING"))
        except Exception as e:
            print("  %s walk fail: %s" % (name, e))


def dump_tex(path):
    print("=== %s  exists=%s" % (path, EAL.does_asset_exist(path)))
    t = EAL.load_asset(path)
    if t is None:
        print("  LOAD FAILED")
        return
    for prop in ("compression_settings", "srgb", "lod_bias"):
        try:
            print("  %s = %s" % (prop, t.get_editor_property(prop)))
        except Exception as e:
            print("  %s read fail: %s" % (prop, e))
    for fn in ("get_size_x", "get_size_y", "blueprint_get_size_x", "blueprint_get_size_y"):
        try:
            print("  %s = %s" % (fn, getattr(t, fn)()))
            break
        except Exception:
            continue


def dump_actors():
    try:
        sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actors = sub.get_all_level_actors()
    except Exception as e:
        print("=== actors: level read fail: %s" % e)
        return
    found = 0
    for a in actors:
        try:
            cls = a.get_class().get_name()
        except Exception:
            continue
        if cls != "GasStoveBurnerFlame":
            continue
        found += 1
        try:
            loc = a.get_actor_location()
            print("=== actor %s at (%.1f, %.1f, %.1f)" % (a.get_actor_label(), loc.x, loc.y, loc.z))
        except Exception as e:
            print("=== actor (label/loc fail: %s)" % e)
        try:
            inst = a.get_components_by_class(unreal.InstancedStaticMeshComponent)
            for c in inst:
                try:
                    print("  ISM mesh=%s" % c.get_editor_property("static_mesh"))
                except Exception as e:
                    print("  ISM mesh read fail: %s" % e)
                try:
                    print("  ISM override_mats=%s" % c.get_editor_property("override_materials"))
                except Exception as e:
                    print("  ISM mats read fail: %s" % e)
                try:
                    print("  ISM instance_count=%s" % c.get_instance_count())
                except Exception as e:
                    print("  ISM count fail: %s" % e)
        except Exception as e:
            print("  ISM enum fail: %s" % e)
        try:
            sm = a.get_components_by_class(unreal.StaticMeshComponent)
            for c in sm:
                if c.get_class().get_name() == "InstancedStaticMeshComponent":
                    continue
                try:
                    print("  SM %s mesh=%s mats=%s" % (c.get_name(),
                          c.get_editor_property("static_mesh"),
                          c.get_editor_property("override_materials")))
                except Exception as e:
                    print("  SM read fail: %s" % e)
        except Exception as e:
            print("  SM enum fail: %s" % e)
    if not found:
        print("=== actors: NO GasStoveBurnerFlame in current level")


dump_mat("/Game/VFX/GasStove/M_GasFlame")
dump_mat("/Game/VFX/GasStove/M_GasGlow")
dump_tex("/Game/VFX/GasStove/Textures/T_GasFlame_Tongue")
dump_tex("/Game/VFX/GasStove/Textures/T_GasFlame_Glow")
dump_actors()
print("=== DIAG DONE")
