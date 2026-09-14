// Scores an estimator run against ground truth, in the terms a payroll/compliance product cares about.
import type { ScanMsg } from '@sim/protocol';
import type { Punch } from './punch.js';
import type { FloorEstimate } from './types.js';

export interface Tick {
  scan: ScanMsg;
  est: FloorEstimate;
  /** floor the punch engine had the worker clocked in on after this tick */
  punchedFloor: number | null;
}

export interface Metrics {
  estimator: string;
  observations: number;
  /** share of observations (after warm-up) whose estimate equals the true floor */
  floorAccuracy: number;
  floorAccuracyOffHoist: number;
  floorAccuracyOnHoist: number;
  punches: number;
  /** share of clock-in punches whose floor was the true floor at punch time */
  punchAccuracy: number;
  hoistRides: number;
  /** punches during a ride or within 10 s after it, to a floor other than the destination, per ride */
  spuriousPerHoistRide: number;
  /** median seconds from ride end to first correct estimate; null if no rides */
  timeToCorrectSec: number | null;
  /** minutes clocked in on a floor the worker was not on (hoist rides excluded) */
  misattributedMinutes: number;
  /** minutes on a work floor (>0) with no clock-in at all */
  uncreditedMinutes: number;
  /** minutes credited to a work floor while the worker was actually in the lobby */
  lobbyMinutesCredited: number;
  hourlyRate: number;
  misattributedDollars: number;
}

export interface ScoreOptions {
  warmupSec: number;
  hourlyRate: number;
  postRideGraceSec: number;
  /** cap on the interval attributed to one observation (background phones report rarely) */
  maxIntervalSec: number;
}

export const DEFAULT_SCORE: ScoreOptions = { warmupSec: 30, hourlyRate: 45, postRideGraceSec: 10, maxIntervalSec: 10 };

export function score(estimator: string, ticks: Tick[], punches: Punch[], opts: ScoreOptions = DEFAULT_SCORE): Metrics {
  // Group by worker, keep time order.
  const byWorker = new Map<string, Tick[]>();
  for (const tk of ticks) {
    let arr = byWorker.get(tk.scan.worker);
    if (!arr) byWorker.set(tk.scan.worker, (arr = []));
    arr.push(tk);
  }
  const punchesByWorker = new Map<string, Punch[]>();
  for (const p of punches) {
    let arr = punchesByWorker.get(p.worker);
    if (!arr) punchesByWorker.set(p.worker, (arr = []));
    arr.push(p);
  }

  let n = 0, correct = 0, nOff = 0, correctOff = 0, nOn = 0, correctOn = 0;
  let misattributedSec = 0, uncreditedSec = 0, lobbyCreditedSec = 0;
  let rides = 0, spurious = 0;
  const timeToCorrect: number[] = [];
  let punchCount = 0, punchCorrect = 0;

  for (const [worker, arr] of byWorker) {
    const wp = punchesByWorker.get(worker) ?? [];

    // Hoist rides from truth: rising/falling edges of onHoist.
    const rideList: { start: number; end: number; dest: number }[] = [];
    let rideStart: number | null = null;
    for (let i = 0; i < arr.length; i++) {
      const tk = arr[i]!;
      if (tk.scan.truth.onHoist && rideStart === null) rideStart = tk.scan.t;
      if (!tk.scan.truth.onHoist && rideStart !== null) {
        rideList.push({ start: rideStart, end: tk.scan.t, dest: tk.scan.truth.floor });
        rideStart = null;
      }
    }
    rides += rideList.length;

    for (const r of rideList) {
      // spurious punches: during the ride or shortly after, not to the destination
      for (const p of wp) {
        if (p.t >= r.start && p.t <= r.end + opts.postRideGraceSec && p.floor !== r.dest) spurious++;
      }
      // time to correct after the ride
      const after = arr.find((tk) => tk.scan.t >= r.end && tk.est.floor === tk.scan.truth.floor);
      if (after) timeToCorrect.push(after.scan.t - r.end);
    }

    for (const p of wp) {
      if (p.kind !== 'in') continue; // clock-outs happen after leaving by definition; judge clock-ins
      // truth at punch time: last tick at or before p.t
      let truthFloor: number | null = null;
      for (let i = arr.length - 1; i >= 0; i--) {
        if (arr[i]!.scan.t <= p.t) { truthFloor = arr[i]!.scan.truth.floor; break; }
      }
      punchCount++;
      if (truthFloor === p.floor) punchCorrect++;
    }

    for (let i = 0; i < arr.length; i++) {
      const tk = arr[i]!;
      const truth = tk.scan.truth;
      if (tk.scan.t >= opts.warmupSec) {
        n++;
        const ok = tk.est.floor === truth.floor;
        if (ok) correct++;
        if (truth.onHoist) { nOn++; if (ok) correctOn++; } else { nOff++; if (ok) correctOff++; }
      }
      const next = arr[i + 1];
      const dt = next ? Math.min(next.scan.t - tk.scan.t, opts.maxIntervalSec) : 0;
      if (dt <= 0) continue;
      // Rides are paid time with no meaningful floor; only stationary intervals can be misattributed.
      if (truth.onHoist) continue;
      const pf = tk.punchedFloor;
      if (pf !== null && pf !== truth.floor) {
        misattributedSec += dt;
        if (truth.floor === 0 && pf > 0) lobbyCreditedSec += dt;
      }
      if (pf === null && truth.floor > 0) uncreditedSec += dt;
    }
  }

  const sorted = [...timeToCorrect].sort((a, b) => a - b);
  const median = sorted.length ? sorted[Math.floor(sorted.length / 2)]! : null;
  const misattributedMinutes = misattributedSec / 60;
  return {
    estimator,
    observations: n,
    floorAccuracy: n ? correct / n : 0,
    floorAccuracyOffHoist: nOff ? correctOff / nOff : 0,
    floorAccuracyOnHoist: nOn ? correctOn / nOn : 0,
    punches: punchCount,
    punchAccuracy: punchCount ? punchCorrect / punchCount : 0,
    hoistRides: rides,
    spuriousPerHoistRide: rides ? spurious / rides : 0,
    timeToCorrectSec: median,
    misattributedMinutes,
    uncreditedMinutes: uncreditedSec / 60,
    lobbyMinutesCredited: lobbyCreditedSec / 60,
    hourlyRate: opts.hourlyRate,
    misattributedDollars: (misattributedMinutes / 60) * opts.hourlyRate,
  };
}
