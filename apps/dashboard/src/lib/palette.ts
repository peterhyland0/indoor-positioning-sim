// Chart colours. Estimator identity = categorical (fixed order, never cycled); status colours are
// reserved for verified / wrong and always ship with an icon or label. Values from the validated
// reference palette (dataviz skill), light + dark.
import type { EstimatorName } from '@sim/estimator';

export const ESTIMATOR_COLOR: Record<EstimatorName, { light: string; dark: string; label: string; blurb: string }> = {
  fused:    { light: '#2a78d6', dark: '#3987e5', label: 'Fused',     blurb: 'beacons + barometer, dwell & hysteresis (proposed)' },
  nearest:  { light: '#eb6834', dark: '#d95926', label: 'Nearest',   blurb: 'strongest beacon wins, no memory (beacon-per-floor as shipped)' },
  smoothed: { light: '#1baf7a', dark: '#199e70', label: 'Smoothed',  blurb: 'beacons with averaging + hysteresis' },
  gps:      { light: '#4a3aa7', dark: '#9085e9', label: 'Site GPS',  blurb: 'site perimeter only: credits the assigned floor' },
};

export const STATUS = { good: '#0ca30c', warning: '#fab219', critical: '#d03b3b' };
export const TRUTH = { light: '#52514e', dark: '#c3c2b7' };
export const HOIST_SHADE = { light: 'rgba(82,81,78,0.10)', dark: 'rgba(195,194,183,0.12)' };
