// Beacon-only, but with memory: per-floor evidence is an exponential moving average of the strongest
// RSSI heard on that floor, and the reported floor only changes when a challenger beats the incumbent
// by a margin for a dwell time. Removes most single-scan flapping; still fooled by long hoist rides.
import { beaconFloor, type Observation, type SessionInfo } from '@sim/protocol';
import { clamp01 } from './nearest.js';
import { type FloorEstimate, type FloorEstimator } from '../types.js';

export interface SmoothedParams {
  /** EWMA time constant, seconds */
  tauSec: number;
  /** value an unheard floor decays toward, dBm */
  floorDbm: number;
  /** challenger must beat incumbent by this much... */
  switchMarginDb: number;
  /** ...for this long */
  dwellSec: number;
  /** evidence a region "enter" event injects (iOS background) */
  regionEnterDbm: number;
}

export const DEFAULT_SMOOTHED: SmoothedParams = {
  tauSec: 5,
  floorDbm: -100,
  switchMarginDb: 6,
  dwellSec: 8,
  regionEnterDbm: -75,
};

interface WorkerState {
  evidence: Float64Array;
  lastT: number;
  incumbent: number | null;
  challenger: number | null;
  challengerSince: number;
}

export class SmoothedBeaconEstimator implements FloorEstimator {
  readonly name: string;
  protected info!: SessionInfo;
  protected workers = new Map<string, WorkerState>();

  constructor(protected readonly p: SmoothedParams = DEFAULT_SMOOTHED, name = 'smoothed') {
    this.name = name;
  }

  reset(session: SessionInfo): void {
    this.info = session;
    this.workers.clear();
  }

  protected state(worker: string, t: number): WorkerState {
    let s = this.workers.get(worker);
    if (!s) {
      s = {
        evidence: new Float64Array(this.info.numFloors + 1).fill(this.p.floorDbm),
        lastT: t,
        incumbent: null,
        challenger: null,
        challengerSince: t,
      };
      this.workers.set(worker, s);
    }
    return s;
  }

  /** Update per-floor beacon evidence from one observation. Shared with the fused estimator. */
  protected updateEvidence(s: WorkerState, obs: Observation): void {
    const dt = Math.max(0, obs.t - s.lastT);
    s.lastT = obs.t;
    const alpha = 1 - Math.exp(-dt / this.p.tauSec);

    const heard = new Float64Array(s.evidence.length).fill(this.p.floorDbm);
    for (const sc of obs.scans) {
      const f = beaconFloor(this.info, sc.b);
      if (f !== null && f < heard.length && sc.rssi > heard[f]!) heard[f] = sc.rssi;
    }
    for (const ev of obs.regionEvents) {
      const f = beaconFloor(this.info, ev.b);
      if (f !== null && f < heard.length && ev.event === 'enter') heard[f] = Math.max(heard[f]!, this.p.regionEnterDbm);
    }
    for (let f = 0; f < s.evidence.length; f++) {
      s.evidence[f]! += alpha * (heard[f]! - s.evidence[f]!);
    }
  }

  /** Hysteresis over any per-floor score array. Returns the (possibly unchanged) incumbent. */
  protected decide(s: WorkerState, score: ArrayLike<number>, t: number, marginUnits: number, dwellSec: number): number | null {
    let best = 0;
    for (let f = 1; f < score.length; f++) if (score[f]! > score[best]!) best = f;
    if (s.incumbent === null) {
      s.incumbent = best;
      s.challenger = null;
      return best;
    }
    if (best !== s.incumbent && score[best]! - score[s.incumbent]! >= marginUnits) {
      if (s.challenger !== best) {
        s.challenger = best;
        s.challengerSince = t;
      } else if (t - s.challengerSince >= dwellSec) {
        s.incumbent = best;
        s.challenger = null;
      }
    } else {
      s.challenger = null;
    }
    return s.incumbent;
  }

  update(obs: Observation): FloorEstimate {
    const s = this.state(obs.worker, obs.t);
    this.updateEvidence(s, obs);
    const floor = this.decide(s, s.evidence, obs.t, this.p.switchMarginDb, this.p.dwellSec);
    if (floor === null) return { floor: null, confidence: 0, state: 'unknown' };
    const gap = secondBestGap(s.evidence, floor);
    return { floor, confidence: clamp01(gap / 15), state: 'stable' };
  }
}

/** How far the chosen floor's score is above the runner-up. */
export function secondBestGap(score: ArrayLike<number>, chosen: number): number {
  let second = -Infinity;
  for (let f = 0; f < score.length; f++) if (f !== chosen && score[f]! > second) second = score[f]!;
  return score[chosen]! - second;
}
