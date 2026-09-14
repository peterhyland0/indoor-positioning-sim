# Estimators, punch engine, scorer

`packages/estimator` is pure TypeScript with no I/O. The same code runs in the CLI (`npm run score`), the
server (live and replay) and the dashboard's Replay page (in the browser), so every number agrees.

## What an estimator sees

Only an `Observation` (`packages/protocol/src/observation.ts`): a scan message with `truth` removed by
type. Per phone report it gets the beacons heard with RSSI (or, for iOS in the background, region
enter/exit events and nothing else), the phone's pressure, and the lobby reference station's pressure at
the same instant. It also knows the installed beacon layout (`SessionInfo`) — that is legitimate site
knowledge, not ground truth.

## The four estimators (`src/estimators/`)

| Name | What it is | Why it is here |
|---|---|---|
| `gps` | Site-perimeter geofence: any report ⇒ credit the worker's **assigned** floor | The before-state vertical geofencing replaces; the one fooled by lobby loitering |
| `nearest` | Floor of the strongest beacon in the latest scan; no memory | Beacon-per-floor as it ships out of the box |
| `smoothed` | Per-floor EWMA of max RSSI (τ 5 s); switch floors only when a challenger leads by 6 dB for 8 s | What smoothing alone buys — and what it can't fix (long hoist rides) |
| `fused` | `smoothed` evidence **plus** a differential barometer: `h = (ref − p) / 0.1205` → floor prior N(h / floorHeight, 0.5 floors); combined score per floor; **vertical-transit detector** (|dh/dt| > 0.3 m/s over 3 s ⇒ `transit`: floor frozen, punches suppressed) | The proposal |

Every constant lives in `DEFAULT_SMOOTHED` / `DEFAULT_FUSED` and can be swept with the scorer.

Why the barometer works here: weather drift is common-mode between the phone and the lobby reference, so
the *difference* is height above the lobby to ~0.25 m. Bleed through the hoist shaft can move beacon
evidence to the wrong floor; it cannot move the air pressure. And the rate of change of height is a
clean "in the hoist / on the stairs" signal, which is exactly when a floor should not be committed.

## Punch engine (`src/punch.ts`)

Estimator-agnostic; one per worker; a zone is a floor. Clock **in** after `enterDwellSec` (20 s) stable on
a floor; clock **out** after `exitDwellSec` (30 s) away from it, or after `transitTimeoutSec` (90 s) in
transit. Never punches during transit. A punch is `verified` when the estimate's confidence ≥ 0.7.
`nearest` runs with `NAIVE_PUNCH` (zero dwell) to reproduce plain zone-entry/exit behaviour.
Clock-ins on the lobby (floor 0) are emitted and flagged in the dashboard, never credited to a work floor.

## Scorer (`src/score.ts`)

| Metric | Definition |
|---|---|
| `floorAccuracy` (+ off-/on-hoist splits) | share of reports after a 30 s warm-up with `est.floor === truth.floor` |
| `punches` | every clock-in and clock-out |
| `punchAccuracy` | share of **clock-ins** on the true floor at that moment (clock-outs happen after leaving by definition) |
| `spuriousPerHoistRide` | punches during a ride, or within 10 s after it, to a floor other than the destination, ÷ rides |
| `timeToCorrectSec` | median seconds from stepping off the hoist to the first correct estimate |
| `misattributedMinutes` / `misattributedDollars` | minutes clocked in on a floor the worker is not on × $45/h. **Hoist rides excluded** — a ride is paid time with no meaningful floor |
| `uncreditedMinutes` | minutes on a work floor with no clock-in at all (the cost of dwell) |
| `lobbyMinutesCredited` | minutes billed to a work floor while the worker was in the lobby — the lobby-cheat headline |

Interval attributed to one report is capped at 10 s so background phones (8 s Android batches, sparse iOS
region events) don't dominate.

## Tuning

`npm run score -- sessions/*.jsonl --estimator fused --json` gives machine-readable metrics. A sweep of the
punch dwells on the four sessions (fused): 20 s in / 30 s out gives the fewest spurious punches (0.21 per
ride) and the highest clock-in accuracy (93%) at the cost of ~8 uncredited minutes across 16 worker-shifts;
10 s / 10 s halves the uncredited time but triples spurious punches. The defaults favour a clean punch log.

## Limitations (say these out loud)

- The RF model is simulated: log-distance path loss, a per-slab loss, Gaussian noise and a slow fade. It is
  calibrated to published numbers, not measured on a site. Real multipath is worse and less stationary.
- The barometer model assumes a reference station in the lobby on the same weather; a real deployment
  needs one per site (or a beacon with a barometer) and has to handle HVAC pressurisation between floors.
- iOS background behaviour is emulated as region enter/exit only; the real constraints (20-region limit,
  delayed delivery) are harsher.
- No real phone traces yet. The bridge protocol and `tools/replay-client.mjs` are designed so a recorded
  phone log can be replayed through the same estimators when one exists.
