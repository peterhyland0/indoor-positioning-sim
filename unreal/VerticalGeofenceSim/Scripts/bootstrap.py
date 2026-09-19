"""Bootstrap the VerticalGeofenceSim content from the editor's Python.

Idempotent: safe to run again; it deletes and rebuilds the generated tower actors (tagged "Generated").

Run from a running editor:   Output Log -> Cmd dropdown "Python" ->  exec(open('<abs path>/bootstrap.py').read())
Headless (used by tools/ue-bootstrap.sh):
    UnrealEditor-Cmd VerticalGeofenceSim.uproject -run=pythonscript -script=<abs path>/bootstrap.py -unattended -nullrhi -nosplash

What it does
  1. Content folders per unreal/README.md
  2. /Game/Maps/L_Tower level: grey-box tower (slabs with hoist + stair holes, shaft cage), floor
     number labels, beacon markers from Data/DT_BeaconLayout.csv, lobby reference station marker,
     cutaway camera, light, PlayerStart
  3. Simple grey / green / amber / red materials
  4. Imports Data/*.csv into DataTables *if* the matching row structs already exist
     (S_BeaconRow, S_MaterialRow, S_ShiftRow, S_RouteRow, S_WaypointRow) - structs must be made by hand
Everything else (Blueprints, enums, structs, widgets) is authored in-editor per docs/unreal-build-guide.md.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tower_geometry as tg  # noqa: E402

import unreal  # noqa: E402

M_TO_CM = 100.0
# NOTE: unreal.Rotator(roll, pitch, yaw) - positional order differs from C++ FRotator(Pitch, Yaw, Roll); always use keywords.
PROJECT_DIR = os.path.abspath(os.path.join(HERE, ".."))
DATA_DIR = os.path.join(PROJECT_DIR, "Data")

FOLDERS = [
    "/Game/Maps",
    "/Game/Sim/Building", "/Game/Sim/Beacons", "/Game/Sim/Workers", "/Game/Sim/Hoist",
    "/Game/Sim/Bridge", "/Game/Sim/UI", "/Game/Sim/Materials",
    "/Game/Data",
]
LEVEL_PATH = "/Game/Maps/L_Tower"
GEN_TAG = "Generated"

eal = unreal.EditorAssetLibrary
actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_ss = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(msg):
    unreal.log(f"[bootstrap] {msg}")


# ---------------------------------------------------------------- folders
def make_folders():
    for f in FOLDERS:
        if not eal.does_directory_exist(f):
            eal.make_directory(f)
    log("folders ok")


# ---------------------------------------------------------------- materials
def make_material(name, rgb, emissive=0.0, roughness=0.8, metallic=0.0, noise=0.0, noise_scale=0.02):
    """Flat-colour material with optional procedural grain (noise multiplies base colour by 1 +/- noise)."""
    path = f"/Game/Sim/Materials/{name}"
    existing = unreal.load_asset(path)  # load_asset is reliable even during PIE; never overwrite
    if existing:
        return existing
    factory = unreal.MaterialFactoryNew()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(name, "/Game/Sim/Materials", unreal.Material, factory)
    mel = unreal.MaterialEditingLibrary
    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -600, 0)
    col.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    if noise > 0:
        n = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -600, 250)
        n.set_editor_property("scale", noise_scale)
        n.set_editor_property("levels", 4)
        n.set_editor_property("output_min", 1.0 - noise)
        n.set_editor_property("output_max", 1.0 + noise)
        mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 100)
        mel.connect_material_expressions(col, "", mul, "A")
        mel.connect_material_expressions(n, "", mul, "B")
        mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 400)
    r.set_editor_property("r", roughness)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if metallic > 0:
        m = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 500)
        m.set_editor_property("r", metallic)
        mel.connect_material_property(m, "", unreal.MaterialProperty.MP_METALLIC)
    if emissive > 0:
        em = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 600)
        em.set_editor_property("constant", unreal.LinearColor(rgb[0] * emissive, rgb[1] * emissive, rgb[2] * emissive, 1.0))
        mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(path)
    return mat


# Material name used by tower_geometry.Box.material -> (asset name, args)
MATERIALS = {
    "Concrete":     ("M_Concrete",     dict(rgb=(0.42, 0.41, 0.39), roughness=0.95, noise=0.18, noise_scale=0.01)),
    "Open":         ("M_ShaftCage",    dict(rgb=(0.85, 0.72, 0.18), roughness=0.5, metallic=0.6)),
    "Steel":        ("M_Steel",        dict(rgb=(0.32, 0.34, 0.36), roughness=0.45, metallic=0.85)),
    "Drywall":      ("M_Drywall",      dict(rgb=(0.80, 0.78, 0.72), roughness=0.9, noise=0.06, noise_scale=0.02)),
    "Gravel":       ("M_Gravel",       dict(rgb=(0.36, 0.33, 0.28), roughness=1.0, noise=0.25, noise_scale=0.03)),
    "Hoarding":     ("M_Hoarding",     dict(rgb=(0.10, 0.28, 0.16), roughness=0.7)),
    "Cabin":        ("M_Cabin",        dict(rgb=(0.86, 0.87, 0.85), roughness=0.55, metallic=0.2)),
    "Timber":       ("M_Timber",       dict(rgb=(0.62, 0.45, 0.25), roughness=0.85, noise=0.15, noise_scale=0.05)),
    "Plasterboard": ("M_Plasterboard", dict(rgb=(0.75, 0.74, 0.70), roughness=0.9)),
    "Blocks":       ("M_Blocks",       dict(rgb=(0.55, 0.55, 0.52), roughness=0.95, noise=0.2, noise_scale=0.04)),
}

# Asset overrides (Fab / Quixel): Data/asset_overrides.json maps a material name above, or a role
# ("hardhat_mesh", "worker_mesh", "pallet_mesh", "slab_material", ...) to an asset path. Anything listed
# there and present on disk replaces the primitive/flat-colour fallback. See unreal/README.md.
def load_overrides():
    path = os.path.join(DATA_DIR, "asset_overrides.json")
    if not os.path.exists(path):
        return {}
    import json
    with open(path) as f:
        raw = json.load(f)
    out = {}
    for key, asset_path in raw.items():
        if key.startswith("_"):
            continue
        asset = unreal.load_asset(asset_path) if asset_path else None
        if asset:
            out[key] = asset
        else:
            log(f"override '{key}' -> {asset_path}: asset not found, using fallback")
    if out:
        log(f"asset overrides active: {sorted(out)}")
    return out


def make_materials(overrides):
    mats = {}
    for key, (name, args) in MATERIALS.items():
        mats[key] = overrides.get(key) or make_material(name, **args)
    mats.update({
        "M_BeaconAlive": make_material("M_BeaconAlive", (0.1, 0.9, 0.2), emissive=3.0),
        "M_BeaconLow":   make_material("M_BeaconLow", (1.0, 0.65, 0.0), emissive=3.0),
        "M_BeaconDead":  make_material("M_BeaconDead", (0.9, 0.1, 0.1), emissive=3.0),
        "M_Reference":   make_material("M_Reference", (0.2, 0.5, 1.0), emissive=2.0),
        "M_Trousers":    make_material("M_Trousers", (0.22, 0.24, 0.30), roughness=0.9),       # work trousers
        "M_HiVis":       make_material("M_HiVis", (1.0, 0.45, 0.02), roughness=0.7),             # vest
        "M_HiVisYellow": make_material("M_HiVisYellow", (0.95, 0.9, 0.05), roughness=0.7),
        "M_HardHat":     make_material("M_HardHat", (0.95, 0.95, 0.9), roughness=0.35),
        "M_HardHatYellow": make_material("M_HardHatYellow", (0.95, 0.8, 0.05), roughness=0.35),
        "M_Skin":        make_material("M_Skin", (0.75, 0.58, 0.45), roughness=0.8),
    })
    log("materials ok")
    return mats


# ---------------------------------------------------------------- level
def open_or_create_level():
    if eal.does_asset_exist(LEVEL_PATH):
        level_ss.load_level(LEVEL_PATH)
        log("loaded existing L_Tower")
    else:
        level_ss.new_level(LEVEL_PATH)
        log("created L_Tower")


def clear_generated():
    n = 0
    for a in actor_ss.get_all_level_actors():
        if GEN_TAG in [str(t) for t in a.tags]:
            actor_ss.destroy_actor(a)
            n += 1
    log(f"removed {n} previously generated actors")


RADIO_BLOCKING = {"Concrete", "Drywall"}


def spawn_box(box: tg.Box, mesh, mat, profile="RadioBlocker", folder="Tower"):
    loc = unreal.Vector(box.cx * M_TO_CM, box.cy * M_TO_CM, box.cz * M_TO_CM)
    actor = actor_ss.spawn_actor_from_class(unreal.StaticMeshActor, loc)
    actor.set_actor_label(box.name)
    actor.set_actor_scale3d(unreal.Vector(box.sx, box.sy, box.sz))  # basic cube is 1 m
    actor.set_folder_path(folder)
    smc = actor.static_mesh_component
    smc.set_static_mesh(mesh)
    smc.set_material(0, mat)
    smc.set_collision_profile_name(profile)
    actor.set_mobility(unreal.ComponentMobility.STATIC)
    tags = [unreal.Name(t) for t in box.tags] + [unreal.Name(GEN_TAG)]
    actor.set_editor_property("tags", tags)
    return actor


def spawn_text(text, pos_m, size=150.0, folder="Tower/Labels", color=(1, 1, 1)):
    loc = unreal.Vector(pos_m[0] * M_TO_CM, pos_m[1] * M_TO_CM, pos_m[2] * M_TO_CM)
    actor = actor_ss.spawn_actor_from_class(unreal.TextRenderActor, loc, unreal.Rotator(roll=0, pitch=0, yaw=-90))
    actor.set_actor_label(f"Label_{text}")
    actor.set_folder_path(folder)
    trc = actor.text_render
    trc.set_text(unreal.Text(text))
    trc.set_world_size(size)
    trc.set_text_render_color(unreal.Color(int(color[0] * 255), int(color[1] * 255), int(color[2] * 255), 255))
    trc.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    actor.set_editor_property("tags", [unreal.Name(GEN_TAG)])
    return actor


FOLDER_BY_TAG = {"Slab": "Tower/Slabs", "ShaftWall": "Tower/Hoist", "Mast": "Tower/Hoist", "Column": "Tower/Structure",
                 "Rail": "Tower/EdgeProtection", "Partition": "Tower/FitOut", "Clutter": "Site/Clutter",
                 "Ground": "Site", "Fence": "Site", "Hut": "Site", "Crane": "Site/Crane"}


def build_tower(spec: tg.TowerSpec, mats):
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    counts = {}
    for box in tg.all_boxes(spec):
        mat = mats.get(box.material) or mats["Concrete"]
        profile = "RadioBlocker" if box.material in RADIO_BLOCKING else "RadioTransparent"
        actor = spawn_box(box, cube, mat, profile, folder=FOLDER_BY_TAG.get(box.tags[0], "Tower"))
        if box.tags[0] in ("Rail", "Mast", "Clutter", "Fence"):
            actor.static_mesh_component.set_cast_shadow(box.tags[0] != "Rail")  # 700+ rail shadows cost a lot
        counts[box.tags[0]] = counts.get(box.tags[0], 0) + 1
    for f in range(0, spec.num_floors + 1):
        spawn_text("LOBBY" if f == 0 else f"F{f}", tg.floor_label(spec, f))
    log(f"tower built: {spec.num_floors} floors + lobby, {counts}")


def build_beacon_markers(spec: tg.TowerSpec, mats):
    """Editor-time markers so the layout is visible before BP_Beacon exists. BP_Building will spawn
    real BP_Beacon actors from DT_BeaconLayout at runtime; these markers are hidden in game."""
    sphere = unreal.load_asset("/Engine/BasicShapes/Sphere")
    rows = tg.load_beacons(os.path.join(DATA_DIR, "DT_BeaconLayout.csv"))
    for row in rows:
        x, y, z = tg.beacon_position(spec, row)
        loc = unreal.Vector(x * M_TO_CM, y * M_TO_CM, z * M_TO_CM)
        actor = actor_ss.spawn_actor_from_class(unreal.StaticMeshActor, loc)
        actor.set_actor_label(f"BeaconMarker_{row['BeaconId']}")
        actor.set_actor_scale3d(unreal.Vector(0.25, 0.25, 0.25))
        actor.set_folder_path("Tower/BeaconMarkers")
        smc = actor.static_mesh_component
        smc.set_static_mesh(sphere)
        smc.set_material(0, mats["M_BeaconAlive"])
        smc.set_collision_profile_name("NoCollision")
        actor.set_actor_hidden_in_game(True)
        actor.set_editor_property("tags", [unreal.Name("BeaconMarker"), unreal.Name(f"Floor:{row['Floor']}"), unreal.Name(GEN_TAG)])
    log(f"{len(rows)} beacon markers placed")


def build_extras(spec: tg.TowerSpec, mats):
    # Reference barometer station in the lobby, by the entrance.
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    ref = spawn_box(tg.Box("ReferenceStationMarker", 1.0, 19.0, 1.2, 0.3, 0.3, 0.3, ("ReferenceStation",), "Open"),
                    cube, mats["M_Reference"], "RadioTransparent", folder="Tower/Extras")
    ref.set_actor_hidden_in_game(True)

    # Cutaway camera: south of the building looking north at the open face, framing all floors.
    h = (spec.num_floors + 1) * spec.floor_height
    cam_loc = unreal.Vector(spec.building_x / 2 * M_TO_CM, -1.6 * h * M_TO_CM, h / 2 * M_TO_CM)
    cam = actor_ss.spawn_actor_from_class(unreal.CameraActor, cam_loc, unreal.Rotator(roll=0, pitch=0, yaw=90))
    cam.set_actor_label("Cam_Cutaway")
    cam.set_folder_path("Tower/Cameras")
    cam.set_editor_property("tags", [unreal.Name("CutawayCamera"), unreal.Name(GEN_TAG)])
    try:
        cam.camera_component.set_editor_property("field_of_view", 60.0)
    except Exception as e:  # noqa: BLE001
        log(f"camera fov not set: {e}")

    # Light + sky so the grey boxes are visible. Both MOVABLE: fully dynamic lighting, no lightmap
    # build, no "LIGHTING NEEDS TO BE REBUILT" banner.
    sun = actor_ss.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, h * M_TO_CM), unreal.Rotator(roll=0, pitch=-38, yaw=-35))  # late-morning, from the south-west so the open face is lit
    sun.set_actor_label("Sun")
    sun.set_folder_path("Tower/Lighting")
    sun.set_editor_property("tags", [unreal.Name(GEN_TAG)])
    try:
        sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sun.light_component.set_editor_property("intensity", 7.0)
        sun.light_component.set_editor_property("light_color", unreal.Color(255, 244, 224, 255))
        sun.light_component.set_editor_property("cast_shadows", True)
    except Exception as e:  # noqa: BLE001
        log(f"sun props not set: {e}")
    sky = actor_ss.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, h * M_TO_CM))
    sky.set_actor_label("Sky")
    sky.set_folder_path("Tower/Lighting")
    sky.set_editor_property("tags", [unreal.Name(GEN_TAG)])
    try:
        sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky.light_component.set_editor_property("intensity", 2.5)
        sky.light_component.set_editor_property("real_time_capture", True)
    except Exception as e:  # noqa: BLE001
        log(f"sky light props not set: {e}")

    # A sky for the sky light to capture (without one, real-time capture is black and the scene goes dark).
    atmo = actor_ss.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    atmo.set_actor_label("SkyAtmosphere")
    atmo.set_folder_path("Tower/Lighting")
    atmo.set_editor_property("tags", [unreal.Name(GEN_TAG)])

    # Haze for depth: exponential height fog, light.
    fog = actor_ss.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    fog.set_actor_label("Fog")
    fog.set_folder_path("Tower/Lighting")
    fog.set_editor_property("tags", [unreal.Name(GEN_TAG)])
    try:
        fc = fog.component
        fc.set_editor_property("fog_density", 0.012)
        fc.set_editor_property("fog_height_falloff", 0.18)
        fc.set_editor_property("start_distance", 3000.0)
        fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.62, 0.70, 0.82, 1.0))
    except Exception as e:  # noqa: BLE001
        log(f"fog props not set: {e}")

    # Tell the world there is no precomputed lighting to build.
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        ws = unreal.GameplayStatics.get_actor_of_class(world, unreal.WorldSettings)
        ws.set_editor_property("force_no_precomputed_lighting", True)
    except Exception as e:  # noqa: BLE001
        log(f"world settings not set: {e}")

    # The C++ simulation runner. Spawns beacons/workers/hoist at BeginPlay and drives the fixed step.
    if hasattr(unreal, "SimBuilding"):
        runner = actor_ss.spawn_actor_from_class(unreal.SimBuilding, unreal.Vector(0, 0, 0))
        runner.set_actor_label("SimBuilding")
        runner.set_folder_path("Sim")
        runner.set_editor_property("tags", [unreal.Name("SimBuilding"), unreal.Name(GEN_TAG)])
    else:
        log("unreal.SimBuilding missing - C++ module not built; runner not placed")

    # PlayerStart in the lobby entrance so PIE has somewhere to put the spectator.
    ps = actor_ss.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(2.0 * M_TO_CM, 18.0 * M_TO_CM, 1.0 * M_TO_CM))
    ps.set_actor_label("PlayerStart_Lobby")
    ps.set_folder_path("Tower/Extras")
    ps.set_editor_property("tags", [unreal.Name(GEN_TAG)])
    log("extras placed (reference station, camera, lights, player start)")


def frame_cutaway(spec: tg.TowerSpec):
    """Point the editor viewport at the open face so the tower is visible immediately."""
    h = (spec.num_floors + 1) * spec.floor_height
    loc = unreal.Vector(spec.building_x / 2 * M_TO_CM, -1.6 * h * M_TO_CM, h / 2 * M_TO_CM)
    try:
        unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(loc, unreal.Rotator(roll=0, pitch=0, yaw=90))
        log("viewport framed on the cutaway")
    except Exception as e:  # noqa: BLE001
        log(f"viewport not framed (headless?): {e}")


# ---------------------------------------------------------------- data tables
# Row structs are native (Source/VerticalGeofenceSim/Public/SimTypes.h); the module must be built.
CSV_TO_STRUCT = {
    "DT_BeaconLayout": "BeaconRow",
    "DT_Materials": "MaterialRow",
    "DT_Shifts": "ShiftRow",
    "DT_Routes": "RouteRow",
    "DT_Waypoints": "WaypointRow",
    "DT_SimConfigDefaults": "ConfigRow",
}

# Hand-made Blueprint enums/structs from before the C++ switch; superseded by SimTypes.h.
LEGACY_ASSETS = [f"/Game/Data/{n}" for n in (
    "S_BeaconRow", "S_MaterialRow", "S_ShiftRow", "S_RouteRow", "S_WaypointRow", "S_ConfigRow",
    "S_Scan", "S_RegionEvent",
    "E_Platform", "E_PhoneState", "E_AppState", "E_RouteSegment", "E_BeaconState",
)] + ["/Game/Sim/Materials/M_Worker"]  # superseded by M_Trousers


def import_data_tables():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    done, skipped = [], []
    for dt_name, struct_name in CSV_TO_STRUCT.items():
        struct_cls = getattr(unreal, struct_name, None)
        csv_path = os.path.join(DATA_DIR, f"{dt_name}.csv")
        if struct_cls is None:
            skipped.append(f"{dt_name} (unreal.{struct_name} missing - is the C++ module built?)")
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", csv_path)
        task.set_editor_property("destination_path", "/Game/Data")
        task.set_editor_property("destination_name", dt_name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        factory = unreal.CSVImportFactory()
        settings = unreal.CSVImportSettings()
        settings.set_editor_property("import_row_struct", struct_cls.static_struct())
        factory.set_editor_property("automated_import_settings", settings)
        task.set_editor_property("factory", factory)
        tools.import_asset_tasks([task])
        done.append(dt_name)
    log(f"data tables imported: {done or 'none'}; skipped: {skipped or 'none'}")


def retire_legacy_assets():
    removed = []
    for path in LEGACY_ASSETS:
        if eal.does_asset_exist(path):
            if eal.delete_asset(path):
                removed.append(path.rsplit("/", 1)[-1])
            else:
                log(f"could not delete {path} (still referenced?)")
    if removed:
        log(f"retired legacy Blueprint types: {removed}")


def make_sim_config():
    """Content/Sim/Bridge/DA_SimConfig: the one USimConfig instance the game instance points at."""
    path = "/Game/Sim/Bridge/DA_SimConfig"
    if unreal.load_asset(path):
        log("DA_SimConfig exists")
        return
    if not hasattr(unreal, "SimConfig"):
        log("unreal.SimConfig missing - C++ module not built; DA_SimConfig not created")
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.SimConfig)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset("DA_SimConfig", "/Game/Sim/Bridge", unreal.SimConfig, factory)
    eal.save_asset(path)
    log(f"created {path}" if asset else "DA_SimConfig creation failed")


# ---------------------------------------------------------------- main
def main():
    # PIE guard - only meaningful in the interactive editor (the call segfaults in the headless commandlet,
    # which has no level viewport). Detect the commandlet via the engine command line.
    headless = "-run=" in unreal.SystemLibrary.get_command_line().lower()
    if not headless:
        try:
            if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
                log("STOP Play-In-Editor first (red square in the toolbar), then rerun. Nothing changed.")
                return
        except Exception:  # noqa: BLE001
            pass
    spec = tg.TowerSpec()
    make_folders()
    overrides = load_overrides()
    mats = make_materials(overrides)
    open_or_create_level()
    clear_generated()
    build_tower(spec, mats)
    build_beacon_markers(spec, mats)
    build_extras(spec, mats)
    level_ss.save_current_level()
    frame_cutaway(spec)
    import_data_tables()
    retire_legacy_assets()
    make_sim_config()
    log("done")


if __name__ == "__main__" or True:  # UE's -run=pythonscript execs the file without __main__
    main()
