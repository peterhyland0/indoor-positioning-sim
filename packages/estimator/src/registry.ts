import { SiteGeofenceEstimator } from './estimators/gps.js';
import { NearestBeaconEstimator } from './estimators/nearest.js';
import { SmoothedBeaconEstimator } from './estimators/smoothed.js';
import { FusedEstimator } from './estimators/fused.js';
import { DEFAULT_PUNCH, NAIVE_PUNCH, type PunchParams } from './punch.js';
import type { FloorEstimator } from './types.js';

export const ESTIMATOR_NAMES = ['gps', 'nearest', 'smoothed', 'fused'] as const;
export type EstimatorName = (typeof ESTIMATOR_NAMES)[number];

/** Fresh estimator instance by name, with the punch parameters it is meant to run with. */
export function createEstimator(name: EstimatorName): { estimator: FloorEstimator; punchParams: PunchParams } {
  switch (name) {
    case 'gps':      return { estimator: new SiteGeofenceEstimator(), punchParams: DEFAULT_PUNCH };
    case 'nearest':  return { estimator: new NearestBeaconEstimator(), punchParams: NAIVE_PUNCH };
    case 'smoothed': return { estimator: new SmoothedBeaconEstimator(), punchParams: DEFAULT_PUNCH };
    case 'fused':    return { estimator: new FusedEstimator(), punchParams: DEFAULT_PUNCH };
  }
}
