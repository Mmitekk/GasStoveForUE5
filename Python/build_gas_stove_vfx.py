# build_gas_stove_vfx.py — one-click build for gas burner blue flame (UE 5.2).
# Run inside Unreal Editor: Tools -> Execute Python Script -> select this file.
# What it does:
#  1. Builds M_GasFlame material (Additive/Unlit) from T_GasFlame_Tongue + T_FlameNoise.
#  2. Creates NS_GasBurner Niagara system (tries API, falls back to manual steps log).
#  3. Prints how to drop AGasStoveBurnerFlame (C++ actor) on the stove.
import unreal

PKG = "/Game/VFX/GasStove"
MAT_PATH = PKG + "/M_GasFlame"
GLOW_MAT_PATH = PKG + "/M_GasGlow"
TONGUE_TEX = PKG + "/Textures/T_GasFlame_Tongue"
NOISE_TEX = PKG + "/Textures/T_FlameNoise"
GLOW_TEX = PKG + "/Textures/T_GasFlame_Glow"
NS_PATH = PKG + "/NS_GasBurner"


def _mk(mat, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(mat, cls, x, y)


def _common_setup(mat):
    try:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    except Exception as e:
        print("[GasVFX] blend_mode set failed: %s" % e)
    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception as e:
        print("[GasVFX] shading_model set failed: %s" % e)
    for prop, val in (("two_sided", True), ("b_two_sided", True)):
        try:
            mat.set_editor_property(prop, val)
            break
        except Exception:
            continue
    try:
        # translucent additive: no depth write, no depth test issues
        mat.set_editor_property("allow_translucent_custom_depth_writes", False)
    except Exception:
        pass
    try:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    except Exception as e:
        print("[GasVFX] wipe failed: %s" % e)


def build_glow_material():
    mat = unreal.EditorAssetLibrary.load_asset(GLOW_MAT_PATH)
    if not mat:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = tools.create_asset("M_GasGlow", PKG, unreal.Material, unreal.MaterialFactoryNew())
        if not mat:
            print("[GasVFX] ERROR: cannot create M_GasGlow")
            return None
    _common_setup(mat)
    MEL = unreal.MaterialEditingLibrary
    glow_tex = unreal.EditorAssetLibrary.load_asset(GLOW_TEX)
    tex = _mk(mat, unreal.MaterialExpressionTextureSample, -600, -100)
    if glow_tex:
        try:
            tex.set_editor_property("texture", glow_tex)
        except Exception as e:
            print("[GasVFX] set glow texture failed: %s" % e)
    else:
        print("[GasVFX] WARNING: %s not found." % GLOW_TEX)
    p_int = _mk(mat, unreal.MaterialExpressionScalarParameter, -600, -300)
    p_int.set_editor_property("parameter_name", "GlowIntensity")
    p_int.set_editor_property("default_value", 6.0)
    boost = _mk(mat, unreal.MaterialExpressionMultiply, -250, -100)
    MEL.connect_material_expressions(tex, "RGB", boost, "A")
    MEL.connect_material_expressions(p_int, "", boost, "B")
    MEL.connect_material_property(boost, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(tex, "A", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(GLOW_MAT_PATH)
    print("[GasVFX] M_GasGlow built (radial blue glow for burner base).")
    return mat


def _param_scalar(mat, name, default, x, y):
    p = _mk(mat, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", default)
    return p


def build_material():
    # Wipe + rebuild in place. (Asset delete is a no-go while the actor
    # references the material — force-delete corrupts the package.)
    mat = unreal.EditorAssetLibrary.load_asset(MAT_PATH)
    if not mat:
        try:
            tools = unreal.AssetToolsHelpers.get_asset_tools()
            mat = tools.create_asset("M_GasFlame", PKG, unreal.Material, unreal.MaterialFactoryNew())
        except Exception as e:
            print("[GasVFX] ERROR: cannot create M_GasFlame: %s" % e)
            return None
        if not mat:
            print("[GasVFX] ERROR: cannot create M_GasFlame")
            return None
    _common_setup(mat)

    MEL = unreal.MaterialEditingLibrary
    tongue_tex = unreal.EditorAssetLibrary.load_asset(TONGUE_TEX)
    noise_tex = unreal.EditorAssetLibrary.load_asset(NOISE_TEX)
    if not tongue_tex:
        print("[GasVFX] ERROR: %s missing — petals would render BLACK. Reimport PNGs, then re-run." % TONGUE_TEX)
        return None
    if not noise_tex:
        print("[GasVFX] WARNING: %s not found, erosion off." % NOISE_TEX)

    tex_tongue = _mk(mat, unreal.MaterialExpressionTextureSample, -700, -120)
    tex_tongue.set_editor_property("texture", tongue_tex)
    tex_noise = None
    if noise_tex:
        tex_noise = _mk(mat, unreal.MaterialExpressionTextureSample, -700, 260)
        tex_noise.set_editor_property("texture", noise_tex)

    p_int = _param_scalar(mat, "FlameIntensity", 6.0, -700, -320)
    p_time = _param_scalar(mat, "FlameTime", 0.0, -700, -420)
    p_op = _param_scalar(mat, "OpacityBoost", 1.0, -700, -220)
    p_mid = _mk(mat, unreal.MaterialExpressionVectorParameter, -1050, -220)
    p_mid.set_editor_property("parameter_name", "TintMid")
    p_mid.set_editor_property("default_value", unreal.LinearColor(0.35, 0.65, 1.0, 1.0))

    # Flame UVs: plain TexCoord. (UV sway via Add/AppendVector breaks material
    # compile through scripting — verified by black petals. Sway will be done
    # on the C++ side by oscillating instances in Tick instead.)
    uv0 = _mk(mat, unreal.MaterialExpressionTextureCoordinate, -1050, 120)
    MEL.connect_material_expressions(uv0, "", tex_tongue, "UV")

    # color: tongueRGB * TintMid * Intensity -> boost (breath applied below)
    tint_mul = _mk(mat, unreal.MaterialExpressionMultiply, -350, -140)
    MEL.connect_material_expressions(tex_tongue, "RGB", tint_mul, "A")
    MEL.connect_material_expressions(p_mid, "", tint_mul, "B")
    boost = _mk(mat, unreal.MaterialExpressionMultiply, -150, -140)
    MEL.connect_material_expressions(tint_mul, "", boost, "A")
    MEL.connect_material_expressions(p_int, "", boost, "B")

    # (No brightness-breath node: Add-based branch breaks scripted compile.
    # Shimmer comes from scrolling noise erosion + C++ intensity flicker.)

    # alpha: (tongueA - noise*0.22) * OpacityBoost -> opacity
    alpha_boost = _mk(mat, unreal.MaterialExpressionMultiply, 150, 60)
    if tex_noise is not None:
        panner = _mk(mat, unreal.MaterialExpressionPanner, -500, 260)
        try:
            panner.set_editor_property("speed_x", 0.15)
            panner.set_editor_property("speed_y", -2.2)
        except Exception:
            pass
        MEL.connect_material_expressions(uv0, "", panner, "Coordinate")
        MEL.connect_material_expressions(panner, "", tex_noise, "UV")
        erode_k = _mk(mat, unreal.MaterialExpressionConstant, -350, 300)
        try:
            erode_k.set_editor_property("r", 0.22)
        except Exception:
            pass
        erode = _mk(mat, unreal.MaterialExpressionMultiply, -300, 220)
        MEL.connect_material_expressions(tex_noise, "R", erode, "A")
        MEL.connect_material_expressions(erode_k, "", erode, "B")
        alpha_sub = _mk(mat, unreal.MaterialExpressionSubtract, -150, 60)
        MEL.connect_material_expressions(tex_tongue, "A", alpha_sub, "A")
        MEL.connect_material_expressions(erode, "", alpha_sub, "B")
        # NOTE: no Saturate node — its input pin won't bind via scripting
        # ("Missing Saturate input" breaks whole-material compile -> black petals).
        # Negative opacity clamps to 0 in hardware blend anyway.
        MEL.connect_material_expressions(alpha_sub, "", alpha_boost, "A")
        MEL.connect_material_expressions(p_op, "", alpha_boost, "B")
    else:
        MEL.connect_material_expressions(tex_tongue, "A", alpha_boost, "A")
        MEL.connect_material_expressions(p_op, "", alpha_boost, "B")

    MEL.connect_material_property(boost, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(alpha_boost, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.connect_material_property(tint_mul, "", unreal.MaterialProperty.MP_BASE_COLOR)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MAT_PATH)

    # --- verify in-session: fail LOUD if the tongue texture didn't bind ---
    n = MEL.get_num_material_expressions(mat)
    used = MEL.get_used_textures(mat)
    print("[GasVFX] M_GasFlame built: expr=%s used_tex=%s" % (n, used))
    if not any("T_GasFlame_Tongue" in str(t) for t in used):
        print("[GasVFX] ERROR: tongue texture NOT bound — petals would be BLACK. Re-run script.")
        return None
    print("[GasVFX] M_GasFlame OK (Additive/Unlit, tongue+noise).")
    return mat


def try_create_niagara():
    # Empty Niagara system; emitter wiring is version-sensitive so we attempt + log fallback.
    if unreal.EditorAssetLibrary.does_asset_exist(NS_PATH):
        print("[GasVFX] NS_GasBurner already exists, skip create.")
        return unreal.EditorAssetLibrary.load_asset(NS_PATH)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    try:
        factory = unreal.NiagaraSystemFactoryNew()
        sys = tools.create_asset("NS_GasBurner", PKG, unreal.NiagaraSystem, factory)
        if sys:
            unreal.EditorAssetLibrary.save_asset(NS_PATH)
            print("[GasVFX] NS_GasBurner created (empty). Wire 1 sprite emitter manually, see steps below.")
            return sys
    except Exception as e:
        print("[GasVFX] Niagara auto-create skipped (%s)." % e)
    print("[GasVFX] MANUAL Niagara (2 min): Content Browser -> + -> FX -> Niagara System ->")
    print("  'New system from selected emitters' -> add 'Simple Sprite Burst' -> Finish as NS_GasBurner ->")
    print("  open it -> Emitter Spawn: Spawn Rate 24, Lifetime 0.35 -> Initialize Particle: Point,")
    print("  Sprite Size ~ (12, 28), Color blue-white -> Add Velocity: 0 -> Material: set M_GasFlame.")
    return None


def main():
    print("=== Gas Stove VFX build (UE 5.2) ===")
    if not unreal.EditorAssetLibrary.does_directory_exist(PKG):
        unreal.EditorAssetLibrary.make_directory(PKG)
    build_material()
    build_glow_material()
    # NOTE: no Niagara — the C++ instanced-mesh ring IS the flame effect and needs
    # no particle system. Niagara emitter stacks can't be scripted reliably in 5.2,
    # so we deliberately don't create an empty placeholder system anymore.
    # Optional later: hand-build smoke/sparks in-editor (5 min) and assign to the
    # actor's ExtraFX component.
    print("=== DONE. Next: ===")
    print("1) Compile C++ (AGasStoveBurnerFlame) in Visual Studio / Live Coding.")
    print("2) Drag 'Gas Stove Burner Flame' actor onto your plate burner (scale to burner radius).")
    print("3) Tune FlameCount/BurnerRadius/FlameHeight live in Details. bLit toggles gas.")

if __name__ == "__main__":
    main()
