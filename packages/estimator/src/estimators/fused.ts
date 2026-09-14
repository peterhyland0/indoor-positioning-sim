// The proposal: beacon evidence (as in `smoothed`) fused with a differential barometer.
//   height above the lobby reference = (refPressure - pressure) / pressurePerMetre
// The barometer gives an absolute floor prior that beacons can't fake (bleed through the shaft can't
// move it), and its rate of change tells us when the worker is in the hoist or on the stairs - in
// which case the floor is frozen and the punch engine is told to hold off.
import type { Observation, SessionInfo } from '@sim/protocol';
import { DEFAULT_SMOOTHED, SmoothedBeaconEstimator, secondBestGap, type SmoothedParams } from './smoothed';
import { clamp01 } from './nearest';
import type { FloorEstimate } from '../types';

export interface FusedParams extends SmoothedParams {
  /** hPa per metre (~1/8.3) */
  pressurePerMetre: number;
  /** EWMA time constant for the height estimate, seconds */
  heightTauSec: number;
  /** barometer floor prior std-dev, in floors */
  baroSigmaFloors: number;
  /** weight of beacon evidence (per 10 dB) vs baro log-prior */
  beaconWeight: number;
  baroWeight: number;
  /** |dh/dt| above this (m/s) over the window = vertical transit */
  transitRateMps: number;
  transitWindowSec: number;
  /** stay in transit this long after the rate drops, so arrival overshoot settles */
  transitHoldSec: number;
  /** combined-score hysteresis */
  switchMarginUnits: number;
  fusedDwellSec: number;
}

export const DEFAULT_FUSED: FusedParams = {
  ...DEFAULT_SMOOTHED,
  pressurePerMetre: 0.1205,
  heightTauSec: 2,
  baroSigmaFloors: 0.5,
  beaconWeight: 1.0,
  baroWeight: 1.0,
  transitRateMps: 0.3,
  transitWindowSec: 3,
  transitHoldSec: 3,
  switchMarginUnits: 1.0,
  fusedDwellSec: 4,
};

interface BaroState {
  h: number | null;
  history: { t: number; h: number }[];
  transitUntil: number;
  score: Float64Array;
}

export class FusedEstimator extends SmoothedBeaconEstimator {
  private baro = new Map<string, BaroState>();

  constructor(private readonly fp: FusedParams = DEFAULT_FUSED) {
    super(fp, 'fused');
  }

  override reset(session: SessionInfo): void {
    super.reset(session);
    this.baro.clear();
  }

  override update(obs: Observation): FloorEstimate {
    const s = this.state(obs.worker, obs.t);
    const prevT = s.lastT;
    this.updateEvidence(s, obs);

    let b = this.baro.get(obs.worker);
    if (!b) {
      b = { h: null, history: [], transitUntil: -Infinity, score: new Float64Array(s.evidence.length) };
      this.baro.set(obs.worker, b);
    }

    // Height above the reference station, smoothed.
    const hRaw = (obs.refPressure - obs.pressure) / this.fp.pressurePerMetre;
    const dt = Math.max(0, obs.t - prevT);
    if (b.h === null) b.h = hRaw;
    else b.h += (1 - Math.exp(-dt / this.fp.heightTauSec)) * (hRaw - b.h);

    // Vertical motion over the window.
    b.history.push({ t: obs.t, h: b.h });
    while (b.history.length > 1 && obs.t - b.history[0]!.t > this.fp.transitWindowSec) b.history.shift();
    const oldest = b.history[0]!;
    const span = obs.t - oldest.t;
    const rate = span > 0.5 ? (b.h - oldest.h) / span : 0;
    if (Math.abs(rate) > this.fp.transitRateMps) b.transitUntil = obs.t + this.fp.transitHoldSec;
    const inTransit = obs.t < b.transitUntil;

    // Combined per-floor score: beacon evidence (10 dB = 1 unit) + baro log-prior.
    const baroFloor = b.h / this.info.floorHeight;
    for (let f = 0; f < b.score.length; f++) {
      const beacon = (s.evidence[f]! - this.fp.floorDbm) / 10;
      const d = (f - baroFloor) / this.fp.baroSigmaFloors;
      b.score[f] = this.fp.beaconWeight * beacon + this.fp.baroWeight * (-0.5 * d * d);
    }

    if (inTransit) {
      // Freeze the reported floor; don't let the hysteresis machinery advance during the ride.
      s.challenger = null;
      return { floor: s.incumbent, confidence: 0.2, state: 'transit' };
    }

    const floor = this.decide(s, b.score, obs.t, this.fp.switchMarginUnits, this.fp.fusedDwellSec);
    if (floor === null) return { floor: null, confidence: 0, state: 'unknown' };
    const gap = secondBestGap(b.score, floor);
    return { floor, confidence: clamp01(gap / 3), state: 'stable' };
  }
}
