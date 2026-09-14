import { SiteGeofenceEstimator } from './estimators/gps';
import { NearestBeaconEstimator } from './estimators/nearest';
import { SmoothedBeaconEstimator } from './estimators/smoothed';
import { FusedEstimator } from './estimators/fused';
import { DEFAULT_PUNCH, NAIVE_PUNCH, type PunchParams } from './punch';
import type { FloorEstimator } from './types';

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
