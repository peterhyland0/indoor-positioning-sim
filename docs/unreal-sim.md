# Unreal simulator — architecture and usage

C++ project (`unreal/VerticalGeofenceSim`, module `VerticalGeofenceSim`) plus a Python bootstrap that
generates the level. No Blueprint logic; the editor is used only to look at things.

## Runtime classes (`Source/VerticalGeofenceSim`)

| Class | Role |
|---|---|
| `USimConfig` (`SimConfig.h`) | Every tunable + the single seeded `FRandomStream`. `Content/Sim/Bridge/DA_SimConfig` is the editable asset; the subsystem duplicates it per run so play never dirties it. `RandNormal`, `RandBool`, `StepWeather`, `ScanPeriod`, `FloorZCm`, `FloorFromZCm`. |
| `USimSubsystem` | Game-instance lifetime owner of the runtime config copy and the bridge. `USimSubsystem::Get(WorldContext)`. |
| `ASimBuilding` (`SimBuilding.h`) | **The runner.** One placed in `L_Tower`. `BeginPlay` spawns hoist, reference station, beacons (`DT_BeaconLayout`) and workers (`DT_Shifts`), then drives a fixed **20 Hz step timer** (`USimConfig::StepSeconds`). Owns `RunCommand`, scenario playback, gust state, worker selection. |
| `ASimBeacon` | Position + radio properties. `Kill/Revive/SetBattery/Nudge` recolour the sphere and emit bridge events. |
| `ASimReferenceStation` | Lobby barometer: `SitePressure + drift + noise`. |
| `ASimWorker` | Route executor (`WalkTo`, `WaitFor`, `Wander`, `BoardHoist`, `TakeStairs` from `DT_Routes`). Holds ground truth (`TruthFloor`, `bOnHoist`) and the last estimate. Text label above the head: white = no estimate, green = estimate matches truth, red = mismatch. |
| `UPhoneSensorsComponent` | The phone. Scan cadence by platform/app-state; per beacon a **multi line trace on the Radio channel** counting every slab/partition crossed; RSSI = log-distance path loss − material loss − body loss − slow fade + Gaussian noise; packet loss; sensitivity floor. Barometer from height + drift + gust + noise. iOS-background mode emits region enter/exit events instead of RSSI. |
| `ASimHoist` | Car with `Idle → Moving → Dwell` state machine serving a floor queue; riders are moved with the car. |
| `USimBridge` | WebSocket client (engine `WebSockets` module, reconnect with backoff) + always-on JSONL recording under `Saved/Sessions/`. `SendSession`, `SendScan`, `SendEvent`; inbound `estimate` messages update worker labels. Protocol: [`bridge-protocol.md`](bridge-protocol.md). |
| `ASimGameMode` / `ASimPlayerController` / `ASimHUD` | Spectator pawn, key bindings, cameras, text HUD. |
| `SSimPanel` | Slate sabotage panel (top-right). Every button calls `ASimBuilding::RunCommand`. |

Determinism: all randomness goes through `USimConfig::Rng`; movement, scans and events advance only
inside the fixed step. Same seed ⇒ byte-identical JSONL (verified).

## Data (`Data/*.csv` → `Content/Data/DT_*`)

Edit the CSV, rerun `Scripts/bootstrap.py`. Row structs are in `SimTypes.h`; column names must match.

- `DT_BeaconLayout` — id, floor, x, y (metres, building-local), tx power, interval, battery, alive
- `DT_Shifts` — worker id, platform, phone state, app state, route, target floor, start offset
- `DT_Routes` — ordered segments per route (`Honest`, `LobbyCheat`, `FloorHopper`)
- `DT_Waypoints` — named floor-local XY (`LobbyEntrance`, `LobbyHoistDoor`, `FloorWorkArea`, …)
- `DT_Materials`, `DT_SimConfigDefaults` — reference only (live values are on `USimConfig` / `DA_SimConfig`)

## Controls in Play

| Key | Action |
|---|---|
| Space | pause / resume |
| R | reset (reload level, new recording) |
| T | draw beacon→phone traces for the selected worker (green clear · orange through a slab · red not heard) |
| Tab | select next worker |
| 1 / 2 / 3 | cutaway camera / free camera (WASD + right-drag) / follow selected worker |
| P | show / hide the sabotage panel |

## Commands

Same commands from the panel, the console (`~` then `Sim <cmd>`), scenario files, or `-ExecCmds="Sim <cmd>"`:

```
killbeacon <id> | revivebeacon <id> | battery <id> <0-1> | nudge <id> <metres>
storm on|off | slam <floor>
pocket <worker> on|off | background <worker> on|off | select <worker>
pause | resume | debug on|off | list
```

## Headless / scripted runs

```bash
E="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"
P=unreal/VerticalGeofenceSim/VerticalGeofenceSim.uproject
"$E" "$P" -game -nullrhi -unattended -nosplash -SimRunSeconds=600 -SimSeed=42 -SimScenario=scenarios/beacon-failure.json
```

| Flag | Effect |
|---|---|
| `-SimRunSeconds=N` | quit after N seconds of sim time |
| `-SimSeed=N` | override `DA_SimConfig.Seed` |
| `-SimScenario=file.json` | `[{"t": sec, "cmd": "..."}, …]` executed at those sim times |
| `-SimShotAt=N` | save `Saved/Screenshots/MacEditor/sim_shot.png` at sim time N (use `-windowed`, not `-nullrhi`) |
| `-SimCamera=follow` | start on the follow camera |

Recordings land in `unreal/VerticalGeofenceSim/Saved/Sessions/<seed>-<timestamp>.jsonl`; copy the
keepers to `sessions/`. Replay one into an estimator with `node tools/replay-client.mjs`.

## Building

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" VerticalGeofenceSimEditor Mac Development \
  -Project="$PWD/unreal/VerticalGeofenceSim/VerticalGeofenceSim.uproject" -WaitMutex -NoHotReload
```

~20 s incremental. The editor must be restarted to load a new binary (or use Live Coding, Ctrl+Alt+F11).

## Level generation (`Scripts/bootstrap.py`)

Idempotent. Run headless via `tools/ue-bootstrap.sh` (editor closed) or in the editor console with
`py /abs/path/Scripts/bootstrap.py` (PIE must be stopped). Creates `L_Tower` (slabs with hoist/stair
holes, shaft cage, floor labels, beacon markers, camera, lights, sky), the grey-box materials,
`DA_SimConfig`, the `SimBuilding` runner, and imports the CSVs into DataTables.
`Scripts/tower_geometry.py` is pure Python — `python3 tower_geometry.py` self-tests the slab tiling.
Gotcha: `unreal.Rotator(roll, pitch, yaw)` — positional order differs from C++; use keywords.
