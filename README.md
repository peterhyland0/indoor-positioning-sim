# indoor-positioning-sim — vertical geofencing simulator

Simulates the problem construction-compliance apps like [Ralco](https://ralco.ai) have to solve:
**which floor of a high-rise is each worker on**, so clock-ins are attributed to the floor the work
is on and not the lobby fourteen floors below. Beacon-per-floor tracking looks simple until the
building is an unfinished concrete shell with open slab edges, a hoist shaft, dead beacons,
phones in pockets and iOS background scanning limits. This project reproduces those failure modes
and lets estimators be tested and scored against them.

## Architecture

```
Unreal Engine (unreal/)                     TypeScript (later)
┌──────────────────────────────┐  WebSocket  ┌──────────────────────────┐
│ grey-box tower, hoist, stairs│ ──────────▶ │ Node estimator service   │
│ beacons, workers on routes   │  BLE scans  │  floor estimation        │
│ RF model via line traces     │  barometer  │  dwell / hysteresis      │
│ barometer w/ weather drift   │  gnd truth  │  punch (clock-in/out)    │
│ iOS/Android scan cadence     │ ◀────────── │  scoring vs ground truth │
│ sabotage panel               │  estimates  └────────────┬─────────────┘
└──────────────┬───────────────┘                          ▼
               ▼                               Next.js + MUI dashboard
      sessions/*.jsonl (recorded runs)         floor board · punch log · metrics
```

The Unreal side is a **sensor simulator only** — a fake building full of fake phones. Everything
that would ship in a real product (estimation, punch logic, dashboard) lives on the TypeScript
side and consumes either the live socket or a recorded `sessions/*.jsonl`.

- Protocol between the two: [`docs/bridge-protocol.md`](docs/bridge-protocol.md)
- Unreal setup and build order: [`unreal/README.md`](unreal/README.md)
- Simulator architecture, controls, headless runs: [`docs/unreal-sim.md`](docs/unreal-sim.md)
- Bridge test tooling: `npm run echo` (stand-in estimator), `node tools/replay-client.mjs <session>`

## Failure modes modelled

- **Floor bleed** — BLE leaks through the hoist shaft, stairwell and open slab edges; a floor-12 beacon is heard on 11 and 13.
- **The hoist ride** — a worker passes every floor on the way up; naive zone entry/exit fires a punch per floor.
- **Beacon health** — dead battery, moved beacon, coverage holes.
- **Phone realities** — pocket attenuation, Android scan batching in background, iOS background = region enter/exit only.
- **Barometer as a second signal** — ~0.45 hPa per floor, with weather drift and a lobby reference station for differential altimetry.

## Metrics the TS side will score

Floor attribution accuracy per punch · spurious punches per hoist ride · time-to-correct-floor
after leaving the hoist · **payroll minutes misattributed per worker per day**.

## Status

- Unreal side (C++): **working end to end** — grey-box tower, beacons, route-driven workers, hoist,
  stairs, line-trace RF model, barometer, WebSocket + JSONL bridge, HUD, cameras, sabotage panel,
  scenario scripts, deterministic headless runs. See [`docs/unreal-sim.md`](docs/unreal-sim.md).
- TypeScript estimator / dashboard: not started. It consumes `sessions/*.jsonl` or the live socket.

## Reading

- Differential barometric altimetry for floor recognition — https://arxiv.org/pdf/2601.02184
- Multi-floor PDR with Viterbi floor detection — https://www.ncbi.nlm.nih.gov/pmc/articles/PMC8271586/
- Floor-level detection in multi-building environments — https://www.ncbi.nlm.nih.gov/pmc/articles/PMC11189499/
