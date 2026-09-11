# Unreal side — VerticalGeofenceSim

Blueprint-only UE 5.4+ project. It is a fake high-rise full of fake phones: it produces the
BLE scan + barometer stream a worker's phone would see, and streams/records it using the
protocol in [`docs/bridge-protocol.md`](../docs/bridge-protocol.md). No estimation happens here.

## One-time setup on this Mac

1. **Git LFS** (binary `.uasset`/`.umap` files are routed through LFS by `.gitattributes`):
   ```bash
   brew install git-lfs && git lfs install
   ```
   Do this **before** the first commit that contains `Content/`.
2. **Unreal Engine 5.4 or newer** from the Epic Games Launcher → Unreal Engine → Library → `+`.
   Apple Silicon native build. ~40 GB. No Xcode needed for a Blueprint-only project.
3. **Metal shader toolchain** — the headless bootstrap runs without it, but the editor GUI needs it to
   compile shaders (first run logged `Missing Mac Metal toolchain (macos SDK not found)`; only the
   Command Line Tools are installed and they lack the `metal` compiler). Install **Xcode** from the
   App Store (~15 GB), open it once to accept the licence, then:
   ```bash
   sudo xcode-select -s /Applications/Xcode.app && xcodebuild -downloadComponent MetalToolchain
   ```
   `xcrun -f metal` should then print a path. Without this the editor opens with black/missing shaders.
4. **Open the project**: the `.uproject` and `Config/` already exist. Double-click
   `VerticalGeofenceSim/VerticalGeofenceSim.uproject` (or Launcher → Library → My Projects → Browse).
   First open compiles shaders for a few minutes. Plugins `JsonBlueprintUtilities`, `HTTPBlueprint`,
   `PythonScriptPlugin`, `EditorScriptingUtilities` are pre-enabled in the `.uproject`.
5. **Generate the tower** (Phase 1) — either headless from the repo root:
   ```bash
   tools/ue-bootstrap.sh
   ```
   or inside the editor: Output Log → command dropdown **Python** →
   `exec(open('/ABS/PATH/unreal/VerticalGeofenceSim/Scripts/bootstrap.py').read())`.
   This creates `/Game/Maps/L_Tower` with slabs (hoist + stair holes), the shaft cage, floor labels,
   beacon markers from `Data/DT_BeaconLayout.csv`, the lobby reference station, `Cam_Cutaway`, lights
   and a PlayerStart. Idempotent — rerun after editing `Scripts/tower_geometry.py` or the CSV.
6. ~~Install a Blueprint WebSocket plugin~~ — not needed: the bridge is C++ on the engine's `WebSockets`
   module. (Kept for reference.) Shortlist — check each listing for **UE 5.8 + Mac** support and price
   before installing (Fab blocks scripted access, so this was not verified automatically):
   - [WebSocket Client for Unreal Engine](https://www.fab.com/listings/028b81de-6110-4a3f-90e9-eda0db1013b8)
     (Pandoa "BlueprintWebSocket"; wraps the engine module, Win/Mac/Linux/iOS/Android;
     [docs](https://pandoa.github.io/BlueprintWebSocket/unreal-engine-5-websocket-plugin)) — first choice.
   - [Blueprint WebSockets](https://www.fab.com/listings/4c33791a-74f8-487e-9fcd-1a49593187f4)
   - [WebSocket with Blueprint](https://www.fab.com/listings/b7c82850-be49-4d98-af63-6a6a8d5fe652)
   - [[Blueprint] Web Communication Helper](https://www.fab.com/listings/d126c150-dd47-4ee7-916b-7f5545fdf4ad)
   Whatever you pick, the nodes map onto the pseudo-nodes in the build guide as: create socket →
   `WS_Create`, connect → `WS_Connect`, send text → `WS_Send`, and the `OnConnected` /
   `OnConnectionError` / `OnClosed` / `OnMessage` events.
   Fallback if none supports 5.8 on Mac yet: `HTTP Blueprint` POST with the same message schema
   (one request per message to `http://localhost:8080/ingest`); inbound estimates then need polling
   or wait for v2.
7. Build the C++ module (see `docs/unreal-sim.md` → Building), reopen the editor, press Play.

Milestone: project opens, `L_Tower` shows the tower, `git status` shows only `unreal/VerticalGeofenceSim/{Config,Content,Data,Scripts,*.uproject}`.

## What is here

```
VerticalGeofenceSim.uproject   engine 5.8, C++ module VerticalGeofenceSim
Source/VerticalGeofenceSim/    all runtime code (see docs/unreal-sim.md)
Config/                        Radio trace channel + RadioBlocker/RadioTransparent profiles, cheap
                               rendering, key bindings (DefaultInput.ini)
Data/*.csv                     source of truth for the DataTables; rerun bootstrap after editing
Scripts/tower_geometry.py      pure-Python slab/hole/shaft geometry (self-tests when run directly)
Scripts/bootstrap.py           editor Python: builds L_Tower, materials, DA_SimConfig, imports CSVs
Content/                       generated assets (.uasset/.umap via LFS)
```

Architecture, controls, commands, headless flags and build instructions: [`docs/unreal-sim.md`](../docs/unreal-sim.md).

## Default sensor-model constants (BP_SimConfig)

| Name | Default | Why |
|---|---|---|
| `FloorHeight` | 3.8 m | typical NYC commercial slab-to-slab |
| `SlabLossDb` | 18 dB | reinforced concrete floor, 2.4 GHz: published 15–25 dB |
| `DrywallLossDb` | 4 dB | gypsum partition ~3–5 dB |
| `GlassLossDb` | 2 dB | |
| `BodyLossDb` | 8 dB | phone in pocket, body between phone and beacon: 5–15 dB |
| `PathLossExponent` | 2.2 | indoor LOS 1.8–2.2; open slab is near-LOS |
| `RssiSigmaDb` | 5 dB | BLE RSSI std-dev in practice 4–6 dB |
| `FadeClampDb` | ±6 dB | slow per-pair random walk standing in for multipath |
| `PacketLoss` | 0.10 | advert collisions + scan-window misses |
| `MaxRangeM` | 40 m | |
| `RxSensitivityDbm` | −95 dBm | phone BLE receiver floor |
| `TxPowerDbm` (beacon) | −59 dBm @1 m | common iBeacon default |
| `AdvIntervalMs` | 300 ms | Ralco's "0.3 s update" |
| `SitePressure` | 1013.25 hPa | |
| `PressurePerMetre` | 1/8.3 hPa/m | ~0.12 hPa/m ⇒ ~0.45 hPa per floor |
| `BaroSigmaHpa` | 0.03 hPa | phone barometer noise |
| `GustHpa` | ±0.3 hPa for 5 s | door slam / HVAC |
| `HoistSpeed` | 1.0 m/s | |
| `HoistDwellSec` | 20 s | |

Before an interview, re-check these against the papers linked from the root README and
update the table with citations (Phase 7).

## RSSI formula (BPC_PhoneSensors, per beacon per scan)

```
rssi = TxPowerDbm
     − 10 · PathLossExponent · log10(distance_m)
     − Σ material loss for every slab/partition the beacon→phone line trace crosses
     − (PhoneState == InPocket ? BodyLossDb : 0)
     − fade[worker][beacon]          (slow random walk, clamped ±FadeClampDb)
     + Normal(0, RssiSigmaDb)
drop if rand < PacketLoss, or beacon dead / battery 0, or rssi < RxSensitivityDbm
```

Barometer, same timer:
```
pressure    = SitePressure − Z_m · PressurePerMetre + Drift(t) + Normal(0, BaroSigmaHpa) (+ gust)
refPressure = SitePressure + Drift(t) + Normal(0, BaroSigmaHpa)          (lobby reference station)
```

## Testing the bridge without the TS side

```bash
npm install
npm run echo            # prints every message from Unreal, echoes truth floor back as the estimate
```
Then Play in the editor; worker labels should turn green as estimates arrive. Recorded runs land
in `VerticalGeofenceSim/Saved/Sessions/`. Copy the good ones to `../sessions/` and commit them.

Replay a recording into any estimator later with:
```bash
node tools/replay-client.mjs sessions/honest.jsonl ws://localhost:8080 5   # 5× speed
```

## Packaging (Phase 7)

Platforms → Mac → Shipping, Package Project. Windowed by default (`DefaultGame.ini`:
`[/Script/EngineSettings.GeneralProjectSettings]` + `r.SetRes=1600x900w` in `DefaultEngine.ini`).
