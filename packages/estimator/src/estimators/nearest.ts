// Baseline: the floor of the strongest beacon in the latest scan. No memory, no smoothing.
// This is what a beacon-per-floor product does out of the box.
import { beaconFloor, type Observation, type SessionInfo } from '@sim/protocol';
import { UNKNOWN, type FloorEstimate, type FloorEstimator } from '../types.js';

export class NearestBeaconEstimator implements FloorEstimator {
  readonly name = 'nearest';
  private info!: SessionInfo;

  reset(session: SessionInfo): void {
    this.info = session;
  }

  update(obs: Observation): FloorEstimate {
    let best: { floor: number; rssi: number } | null = null;
    for (const s of obs.scans) {
      const floor = beaconFloor(this.info, s.b);
      if (floor === null) continue;
      if (!best || s.rssi > best.rssi) best = { floor, rssi: s.rssi };
    }
    if (best) {
      // Louder = more confident; -60 dBm is "right under it", -95 is the noise floor.
      const confidence = clamp01((best.rssi + 95) / 35);
      return { floor: best.floor, confidence, state: 'stable' };
    }
    // iOS background: only enter/exit events. Take the most recent "enter".
    for (let i = obs.regionEvents.length - 1; i >= 0; i--) {
      const ev = obs.regionEvents[i]!;
      if (ev.event === 'enter') {
        const floor = beaconFloor(this.info, ev.b);
        if (floor !== null) return { floor, confidence: 0.3, state: 'stable' };
      }
    }
    return UNKNOWN;
  }
}

export function clamp01(x: number): number {
  return x < 0 ? 0 : x > 1 ? 1 : x;
}
