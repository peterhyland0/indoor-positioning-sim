// Turns a stream of floor estimates into clock-in / clock-out events. Estimator-agnostic.
// A "zone" is a floor. Enter = stable on the floor for enterDwellSec; leave = away for exitDwellSec.
// Transit (hoist/stairs) never punches; a long transit clocks out so the ride isn't paid as work.
import type { FloorEstimate } from './types';

export interface PunchParams {
  enterDwellSec: number;
  exitDwellSec: number;
  transitTimeoutSec: number;
  /** confidence at or above which a punch is marked verified */
  verifiedConfidence: number;
}

/** Dwell that makes the engine reproduce naive zone-entry/exit behaviour. */
export const NAIVE_PUNCH: PunchParams = { enterDwellSec: 0, exitDwellSec: 0, transitTimeoutSec: Infinity, verifiedConfidence: 0.7 };
export const DEFAULT_PUNCH: PunchParams = { enterDwellSec: 20, exitDwellSec: 30, transitTimeoutSec: 90, verifiedConfidence: 0.7 };

export interface Punch {
  t: number;
  worker: string;
  kind: 'in' | 'out';
  floor: number;
  verified: boolean;
}

interface WorkerPunchState {
  punchedFloor: number | null;
  candidate: number | null;
  candidateSince: number;
  awaySince: number | null;
  transitSince: number | null;
}

export class PunchEngine {
  private workers = new Map<string, WorkerPunchState>();

  constructor(private readonly p: PunchParams = DEFAULT_PUNCH) {}

  reset(): void {
    this.workers.clear();
  }

  /** Floor the worker is currently clocked in on, or null. */
  punchedFloor(worker: string): number | null {
    return this.workers.get(worker)?.punchedFloor ?? null;
  }

  update(t: number, worker: string, est: FloorEstimate): Punch[] {
    let s = this.workers.get(worker);
    if (!s) {
      s = { punchedFloor: null, candidate: null, candidateSince: t, awaySince: null, transitSince: null };
      this.workers.set(worker, s);
    }
    const out: Punch[] = [];
    const verified = est.confidence >= this.p.verifiedConfidence;

    if (est.state === 'transit') {
      s.candidate = null;
      s.transitSince ??= t;
      if (s.punchedFloor !== null && t - s.transitSince >= this.p.transitTimeoutSec) {
        out.push({ t, worker, kind: 'out', floor: s.punchedFloor, verified: false });
        s.punchedFloor = null;
      }
      return out;
    }
    s.transitSince = null;

    const floor = est.floor;
    if (s.punchedFloor !== null && floor === s.punchedFloor) {
      s.awaySince = null;
      s.candidate = null;
      return out;
    }

    // Away from the punched floor (or nowhere at all).
    if (s.punchedFloor !== null) {
      s.awaySince ??= t;
      if (t - s.awaySince >= this.p.exitDwellSec) {
        out.push({ t, worker, kind: 'out', floor: s.punchedFloor, verified });
        s.punchedFloor = null;
        s.awaySince = null;
      }
    }

    if (floor === null) {
      s.candidate = null;
      return out;
    }
    if (s.candidate !== floor) {
      s.candidate = floor;
      s.candidateSince = t;
    }
    if (s.punchedFloor === null && t - s.candidateSince >= this.p.enterDwellSec) {
      out.push({ t, worker, kind: 'in', floor, verified });
      s.punchedFloor = floor;
      s.candidate = null;
      s.awaySince = null;
    }
    return out;
  }
}
