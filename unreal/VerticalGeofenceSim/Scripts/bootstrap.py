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
def make_material(name, rgb, emissive=0.0):
    path = f"/Game/Sim/Materials/{name}"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.MaterialFactoryNew()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(name, "/Game/Sim/Materials", unreal.Material, factory)
    mel = unreal.MaterialEditingLibrary
    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
    col.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if emissive > 0:
        em = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 200)
        em.set_editor_property("constant", unreal.LinearColor(rgb[0] * emissive, rgb[1] * emissive, rgb[2] * emissive, 1.0))
        mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(path)
    return mat


def make_materials():
    mats = {
        "M_Concrete": make_material("M_Concrete", (0.45, 0.45, 0.45)),
        "M_ShaftCage": make_material("M_ShaftCage", (0.85, 0.75, 0.2)),
        "M_BeaconAlive": make_material("M_BeaconAlive", (0.1, 0.9, 0.2), emissive=3.0),
        "M_BeaconLow": make_material("M_BeaconLow", (1.0, 0.65, 0.0), emissive=3.0),
        "M_BeaconDead": make_material("M_BeaconDead", (0.9, 0.1, 0.1), emissive=3.0),
        "M_Reference": make_material("M_Reference", (0.2, 0.5, 1.0), emissive=2.0),
        "M_Worker": make_material("M_Worker", (0.9, 0.9, 0.9)),
    }
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


def spawn_text(text, pos_m, size=120.0, folder="Tower/Labels", color=(1, 1, 1)):
    loc = unreal.Vector(pos_m[0] * M_TO_CM, pos_m[1] * M_TO_CM, pos_m[2] * M_TO_CM)
    actor = actor_ss.spawn_actor_from_class(unreal.TextRenderActor, loc, unreal.Rotator(0, -90, 0))
    actor.set_actor_label(f"Label_{text}")
    actor.set_folder_path(folder)
    trc = actor.text_render
    trc.set_text(unreal.Text(text))
    trc.set_world_size(size)
    trc.set_text_render_color(unreal.Color(int(color[0] * 255), int(color[1] * 255), int(color[2] * 255), 255))
    trc.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    actor.set_editor_property("tags", [unreal.Name(GEN_TAG)])
    return actor


def build_tower(spec: tg.TowerSpec, mats):
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    for box in tg.all_boxes(spec):
        mat = mats["M_ShaftCage"] if box.material == "Open" else mats["M_Concrete"]
        profile = "RadioTransparent" if box.material == "Open" else "RadioBlocker"
        spawn_box(box, cube, mat, profile, folder="Tower/Slabs" if "Slab" in box.tags else "Tower/Shaft")
    for f in range(0, spec.num_floors + 1):
        spawn_text("LOBBY" if f == 0 else f"F{f}", tg.floor_label(spec, f))
    log(f"tower built: {spec.num_floors} floors + lobby")


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
    cam = actor_ss.spawn_actor_from_class(unreal.CameraActor, cam_loc, unreal.Rotator(0, 90, 0))
    cam.set_actor_label("Cam_Cutaway")
    cam.set_folder_path("Tower/Cameras")
    cam.set_editor_property("tags", [unreal.Name("CutawayCamera"), unreal.Name(GEN_TAG)])
    try:
        cam.camera_component.set_editor_property("field_of_view", 60.0)
    except Exception as e:  # noqa: BLE001
        log(f"camera fov not set: {e}")

    # Light + sky so the grey boxes are visible. Both MOVABLE: fully dynamic lighting, no lightmap
    # build, no "LIGHTING NEEDS TO BE REBUILT" banner.
    sun = actor_ss.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, h * M_TO_CM), unreal.Rotator(-50, 30, 0))
    sun.set_actor_label("Sun")
    sun.set_folder_path("Tower/Lighting")
    sun.set_editor_property("tags", [unreal.Name(GEN_TAG)])
    try:
        sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sun.light_component.set_editor_property("intensity", 6.0)
    except Exception as e:  # noqa: BLE001
        log(f"sun props not set: {e}")
    sky = actor_ss.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, h * M_TO_CM))
    sky.set_actor_label("Sky")
    sky.set_folder_path("Tower/Lighting")
    sky.set_editor_property("tags", [unreal.Name(GEN_TAG)])
    try:
        sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky.light_component.set_editor_property("intensity", 1.5)
        sky.light_component.set_editor_property("real_time_capture", True)
    except Exception as e:  # noqa: BLE001
        log(f"sky light props not set: {e}")

    # A sky for the sky light to capture (without one, real-time capture is black and the scene goes dark).
    atmo = actor_ss.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    atmo.set_actor_label("SkyAtmosphere")
    atmo.set_folder_path("Tower/Lighting")
    atmo.set_editor_property("tags", [unreal.Name(GEN_TAG)])

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
        unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(loc, unreal.Rotator(0, 90, 0))
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
)]


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
    if eal.does_asset_exist(path):
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
    spec = tg.TowerSpec()
    make_folders()
    mats = make_materials()
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
