import type { Observation, SessionInfo } from '@sim/protocol';

export type EstimateState = 'stable' | 'transit' | 'unknown';

export interface FloorEstimate {
  /** null = no opinion (yet) */
  floor: number | null;
  /** 0..1 */
  confidence: number;
  state: EstimateState;
}

/**
 * A floor estimator. Stateful per worker; must only ever see Observations (no ground truth).
 * `update` is called once per scan message in time order.
 */
export interface FloorEstimator {
  readonly name: string;
  reset(session: SessionInfo): void;
  update(obs: Observation): FloorEstimate;
}

export const UNKNOWN: FloorEstimate = { floor: null, confidence: 0, state: 'unknown' };
