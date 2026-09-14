# indoor-positioning-sim — vertical geofencing simulator

Simulates the problem construction-compliance apps like [Ralco](https://ralco.ai) have to solve:
**which floor of a high-rise is each worker on**, so clock-ins are attributed to the floor the work
is on and not the lobby fourteen floors below. Beacon-per-floor tracking looks simple until the
building is an unfinished concrete shell with open slab edges, a hoist shaft, dead beacons,
phones in pockets and iOS background scanning limits. This project reproduces those failure modes
and lets estimators be tested and scored against them.

## Architecture

```
Unreal Engine (unreal/)                      TypeScript (packages/, apps/)
┌──────────────────────────────┐  WebSocket  ┌──────────────────────────────────────┐
│ grey-box tower, hoist, stairs│ ──────────▶ │ apps/server (Node + ws + Postgres)   │
│ beacons, workers on routes   │  BLE scans  │   runs every estimator in parallel   │
│ RF model via line traces     │  barometer  │   punch engine · scorer · persistence│
│ barometer w/ weather drift   │  gnd truth  │                                      │
│ iOS/Android scan cadence     │ ◀────────── │   estimate → worker labels           │
│ sabotage panel               │  estimates  │   command  ← dashboard sabotage bar  │
└──────────────┬───────────────┘  commands   └───────────────┬──────────────────────┘
               ▼                                             ▼  /ui feed + REST
      sessions/*.jsonl                          apps/dashboard (Next.js + MUI)
      (recorded runs, also                      floor board · punch log · timelines
       replayed in the browser)                 scorecard · A/B toggle · replay scrubber
```

- `packages/protocol` — zod schemas for the bridge; an `Observation` type that cannot carry ground truth
- `packages/estimator` — `gps` (site perimeter), `nearest` (strongest beacon), `smoothed` (EWMA + hysteresis),
  `fused` (beacons + differential barometer + transit detection); punch engine; scorer; `npm run score`
- `apps/server` — ingest, estimators, JSONL replay, Postgres (Supabase) persistence, dashboard feed
- `apps/dashboard` — Live (needs the server), Replay (runs the estimators in the browser, no server), Sessions (history)

Docs: [`docs/unreal-sim.md`](docs/unreal-sim.md) · [`docs/estimator.md`](docs/estimator.md) · [`docs/bridge-protocol.md`](docs/bridge-protocol.md) · [`db/README.md`](db/README.md)

## Results

`npm run score -- sessions/*.jsonl` on the four canonical 15-minute recordings (seed 42, 4 workers each,
$45/h). Hoist rides are excluded from the payroll numbers; "punches" counts every clock-in and clock-out.

| Session | Estimator | Floor accuracy (stationary) | Punches | Spurious / hoist ride | Clock-in on right floor | Lobby min billed | Payroll misattributed |
|---|---|---|---|---|---|---|---|
| honest | Site GPS | 87.8% | 4 | 0.00 | 0.0% | 6.3 | $4.70 |
| honest | Nearest beacon | 79.8% | 1354 | 43.69 | 58.6% | 0.1 | $2.39 |
| honest | Smoothed beacons | 86.2% | 32 | 0.77 | 55.6% | 1.4 | $6.46 |
| honest | **Fused** (proposed) | 95.0% | 20 | 0.23 | 91.7% | 1.2 | $2.02 |
| lobby-cheat | Site GPS | 51.1% | 4 | 0.00 | 0.0% | 24.7 | $18.51 |
| lobby-cheat | Nearest beacon | 92.8% | 917 | 35.11 | 62.0% | 0.4 | $1.62 |
| lobby-cheat | Smoothed beacons | 97.3% | 20 | 0.44 | 75.0% | 1.0 | $3.32 |
| lobby-cheat | **Fused** (proposed) | 97.1% | 14 | 0.11 | 100.0% | 1.0 | $1.84 |
| beacon-failure | Site GPS | 87.8% | 4 | 0.00 | 0.0% | 6.4 | $4.77 |
| beacon-failure | Nearest beacon | 70.1% | 1452 | 45.92 | 55.1% | 0.2 | $3.32 |
| beacon-failure | Smoothed beacons | 83.9% | 38 | 0.77 | 57.1% | 1.5 | $7.46 |
| beacon-failure | **Fused** (proposed) | 94.9% | 20 | 0.23 | 91.7% | 1.3 | $2.11 |
| phones-and-weather | Site GPS | 33.9% | 4 | 0.00 | 0.0% | 5.8 | $15.55 |
| phones-and-weather | Nearest beacon | 90.4% | 564 | 20.00 | 72.2% | 0.0 | $1.49 |
| phones-and-weather | Smoothed beacons | 87.1% | 30 | 0.45 | 76.5% | 0.8 | $7.30 |
| phones-and-weather | **Fused** (proposed) | 89.5% | 28 | 0.27 | 87.5% | 0.8 | $5.55 |

What the table says, in Ralco's terms:

- **A site-level geofence (what GPS gives you) bills the lobby.** In `lobby-cheat` it credits 24.7 lobby
  minutes to the work floor — the "clocked in from the lobby, fourteen floors below" story, $18.51 per 4
  workers per 15 minutes. Any floor-aware estimator brings that to ≈1 minute.
- **Nearest-beacon (beacon-per-floor as shipped) can't survive the hoist.** 1,354 punches in 15 minutes and
  ~44 spurious punches per hoist ride: the log is unusable even though its dollar figure looks fine.
- **Fused is the one you'd ship.** Best stationary floor accuracy everywhere (95–97%), 0.1–0.3 spurious
  punches per ride, 88–100% of clock-ins on the right floor, and it holds 95% accuracy through a dead
  beacon and a moved one (`beacon-failure`) because the barometer carries the floor. Its remaining cost is
  the ~10 s it takes to settle after stepping off the hoist plus deliberate dwell before punching.

## Quick start

```bash
npm install
npm test                                   # protocol, estimator, server
npm run score -- sessions/*.jsonl          # the table above
npm run dashboard                          # http://localhost:3000 — Replay page works with nothing else running
npm run server                             # ws://localhost:8080 — then press Play in Unreal for the Live page
```

Postgres history: put the Supabase connection string in `apps/server/.env` (`db/README.md`).

## Failure modes modelled

- **Floor bleed** — BLE leaks through the hoist shaft, stairwell and open slab edges; a floor-12 beacon is heard on 11 and 13.
- **The hoist ride** — a worker passes every floor on the way up; naive zone entry/exit fires a punch per floor.
- **Beacon health** — dead battery, moved beacon, coverage holes.
- **Phone realities** — pocket attenuation, Android scan batching in background, iOS background = region enter/exit only.
- **Barometer as a second signal** — ~0.45 hPa per floor, with weather drift and a lobby reference station for differential altimetry.

## Status

- Unreal simulator (C++): complete — see [`docs/unreal-sim.md`](docs/unreal-sim.md).
- Estimators, scorer, server, dashboard: complete and tested; Postgres schema live on Supabase.
- Not yet: real phone data (the RF model is simulated and calibrated to published figures, not measured).

## Reading

- Differential barometric altimetry for floor recognition — https://arxiv.org/pdf/2601.02184
- Multi-floor PDR with Viterbi floor detection — https://www.ncbi.nlm.nih.gov/pmc/articles/PMC8271586/
- Floor-level detection in multi-building environments — https://www.ncbi.nlm.nih.gov/pmc/articles/PMC11189499/
