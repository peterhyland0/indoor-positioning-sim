# Bridge protocol (Unreal ⇄ estimator)

Version `v: 1`. One JSON object per message. Same objects go over the WebSocket
(`ws://localhost:8080`, Unreal is the client) and into the JSONL recording
(`unreal/VerticalGeofenceSim/Saved/Sessions/<seed>-<timestamp>.jsonl`, one object per line).

Times `t` are simulation seconds since Play, not wall clock. Distances in metres,
pressure in hPa, RSSI in dBm.

## Outbound (Unreal → estimator)

### `session` — once, first message

```json
{"v":1,"type":"session","seed":42,
 "floorHeight":3.8,"numFloors":15,
 "config":{"slabLossDb":18,"drywallLossDb":4,"glassLossDb":2,"bodyLossDb":8,
           "pathLossExponent":2.2,"rssiSigmaDb":5,"packetLoss":0.1,
           "maxRangeM":40,"rxSensitivityDbm":-95,"drywallPercent":0,
           "weatherDrift":true,"hoistSpeed":1.0,"hoistDwellSec":20},
 "beacons":[{"id":"F12-A","floor":12,"x":14.0,"y":9.5,"z":47.1,"txPowerDbm":-59}],
 "workers":[{"id":"w07","platform":"android","route":"Honest","targetFloor":12}]}
```

### `scan` — per worker, per scan timer fire

```json
{"v":1,"type":"scan","t":1723.4,
 "worker":"w07","platform":"android","appState":"fg","phoneState":"inPocket",
 "scans":[{"b":"F12-A","rssi":-68},{"b":"F12-B","rssi":-71},{"b":"F13-A","rssi":-79}],
 "regionEvents":[],
 "pressure":1009.42,"refPressure":1013.85,
 "truth":{"floor":12,"onHoist":false,"z":44.1}}
```

- `appState`: `fg` | `bg`.
- `scans`: beacons heard this scan. Empty array is valid (nothing in range / packets lost).
- `regionEvents`: only populated for iOS in `bg`, where the OS gives enter/exit and no RSSI:
  `[{"b":"F12-A","event":"enter"}]`. When populated, `scans` is empty.
- `refPressure`: the lobby reference station's reading at the same `t` (for differential altimetry).
- `truth`: ground truth for **scoring only**. Estimators must not read it.

### `event` — scenario changes, mostly from the sabotage panel

```json
{"v":1,"type":"event","t":900.0,"name":"beaconKilled","payload":{"b":"F12-A"}}
```

`name` values: `beaconKilled`, `beaconRevived`, `beaconLowBattery`, `beaconMoved`
(`payload: {b, x, y, z}`), `stormStart`, `stormStop`, `gust` (`payload: {floor}`),
`workerPhoneState` (`payload: {worker, phoneState}`), `workerAppState`
(`payload: {worker, appState}`), `paused`, `resumed`, `reset`.

## Inbound (estimator → Unreal)

```json
{"v":1,"type":"estimate","worker":"w07","estFloor":13,"punch":null}
```

- `estFloor`: integer floor, or `null` if the estimator has no opinion yet.
- `punch`: `"in"` | `"out"` | `null`. Unreal flashes a marker on the worker label.

Unreal ignores anything it doesn't understand. Missing server = no error; the sim
keeps recording to JSONL and retries the socket with backoff.

## Scan cadence (what the sim emulates)

| Platform | Foreground              | Background                                  |
|----------|-------------------------|---------------------------------------------|
| Android  | every 1.0 s             | one batch every 8 s                          |
| iOS      | every 1.0 s (ranging)   | `regionEvents` only, no RSSI, no periodic scan |
