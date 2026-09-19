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

`npm run score -- sessions/*.jsonl` on the four canonical 15-minute recordings (seed 42, 7–9 workers each,
behaving by trade — electricians stationary on one floor, plumbers on the riser across two, carpenters on
material runs, labourers constantly moving, a supervisor touring — two hoists, columns and fit-out drywall
on the lower floors, $45/h). Hoist rides are excluded
from the payroll numbers; "punches" counts every clock-in and clock-out; a "spurious" punch is one fired
during or just after a ride to a floor that is neither where the ride started nor where it ended.

| Session | Estimator | Floor accuracy (stationary) | Punches | Spurious / hoist ride | Clock-in on right floor | Lobby min billed | Payroll misattributed |
|---|---|---|---|---|---|---|---|
| honest | Site GPS | 80.9% | 9 | 0.00 | 0.0% | 18.5 | $19.18 |
| honest | Nearest beacon | 86.8% | 2086 | 28.00 | 64.0% | 0.4 | $4.40 |
| honest | Smoothed beacons | 96.7% | 71 | 0.94 | 62.5% | 3.3 | $10.75 |
| honest | **Fused** (proposed) | 97.0% | 55 | 0.22 | 81.3% | 2.7 | $9.10 |
| lobby-cheat | Site GPS | 59.4% | 8 | 0.00 | 0.0% | 39.9 | $32.88 |
| lobby-cheat | Nearest beacon | 92.6% | 1524 | 28.58 | 65.1% | 0.2 | $2.81 |
| lobby-cheat | Smoothed beacons | 97.7% | 38 | 0.25 | 87.0% | 1.8 | $7.05 |
| lobby-cheat | **Fused** (proposed) | 97.8% | 32 | 0.00 | 100.0% | 1.8 | $5.59 |
| beacon-failure | Site GPS | 81.0% | 9 | 0.00 | 0.0% | 18.5 | $19.18 |
| beacon-failure | Nearest beacon | 81.3% | 2440 | 28.72 | 65.3% | 0.1 | $4.89 |
| beacon-failure | Smoothed beacons | 96.9% | 65 | 0.83 | 64.9% | 2.7 | $9.74 |
| beacon-failure | **Fused** (proposed) | 97.0% | 55 | 0.22 | 81.3% | 2.7 | $8.94 |
| phones-and-weather | Site GPS | 43.2% | 7 | 0.00 | 0.0% | 23.5 | $29.78 |
| phones-and-weather | Nearest beacon | 83.6% | 1307 | 19.59 | 69.3% | 0.1 | $3.15 |
| phones-and-weather | Smoothed beacons | 90.4% | 57 | 0.29 | 81.3% | 4.0 | $13.64 |
| phones-and-weather | **Fused** (proposed) | 93.8% | 57 | 0.29 | 87.5% | 4.0 | $11.92 |

What the table says, in Ralco's terms:

- **A site-level geofence (what GPS gives you) bills the lobby.** Workers queue for the hoist, fetch materials,
  and in `lobby-cheat` three of them deliberately hang about: 18–40 lobby minutes per 15-minute window are
  credited to a work floor — $19–33 for one small crew. Any floor-aware estimator brings that to a few
  minutes, and those minutes are the exit-dwell after stepping off the hoist, not loitering.
- **Nearest-beacon (beacon-per-floor as shipped) can't survive the hoist.** 1,300–2,400 punches in 15
  minutes and 20–29 spurious punches per ride: the log is unusable even though its dollar figure looks fine.
- **Fused is the one you'd ship.** Best or within a fraction of a point of the best stationary floor accuracy everywhere (94–98%), 0–0.29 spurious
  punches per ride, 81–100% of clock-ins on the right floor, and it holds 97% accuracy through a dead
  beacon and a moved one (`beacon-failure`) because the barometer carries the floor. Its remaining cost is
  the ~10 s to settle after a ride plus the deliberate dwell before punching.

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
