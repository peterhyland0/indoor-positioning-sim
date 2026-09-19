// Golden tests on the four canonical recordings: the story each session tells must hold.
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { parseSession, runSession } from './session';
import { createEstimator, ESTIMATOR_NAMES, type EstimatorName } from './registry';
import type { Metrics } from './score';

const dir = join(import.meta.dirname, '../../../sessions');
const load = (name: string) => parseSession(readFileSync(join(dir, `${name}.jsonl`), 'utf8'));
const run = (session: ReturnType<typeof load>, name: EstimatorName): Metrics => {
  const { estimator, punchParams } = createEstimator(name);
  return runSession(session, estimator, punchParams).metrics;
};

describe('canonical sessions', () => {
  const sessions = { honest: load('honest'), cheat: load('lobby-cheat'), failure: load('beacon-failure'), phones: load('phones-and-weather') };

  it('fused produces far fewer spurious punches per hoist ride than nearest, everywhere', () => {
    for (const s of Object.values(sessions)) {
      const n = run(s, 'nearest'), f = run(s, 'fused');
      expect(n.spuriousPerHoistRide).toBeGreaterThan(10);
      expect(f.spuriousPerHoistRide).toBeLessThan(0.5);
      expect(f.punches).toBeLessThan(n.punches / 10);
    }
  });

  it('fused is the most accurate estimator off the hoist, everywhere', () => {
    for (const s of Object.values(sessions)) {
      const f = run(s, 'fused');
      for (const other of ['gps', 'nearest', 'smoothed'] as const) {
        expect(f.floorAccuracyOffHoist).toBeGreaterThanOrEqual(run(s, other).floorAccuracyOffHoist - 0.01);
      }
    }
  });

  it('the lobby cheat: a site geofence credits the lobby loiterers, vertical geofencing does not', () => {
    const gps = run(sessions.cheat, 'gps'), fused = run(sessions.cheat, 'fused');
    expect(gps.lobbyMinutesCredited).toBeGreaterThan(15);
    expect(fused.lobbyMinutesCredited).toBeLessThan(gps.lobbyMinutesCredited / 10);
    expect(fused.misattributedDollars).toBeLessThan(gps.misattributedDollars / 5);
  });

  it('beacon failure: fused stays above 90% off-hoist and well ahead of nearest', () => {
    const fused = run(sessions.failure, 'fused'), nearest = run(sessions.failure, 'nearest');
    expect(fused.floorAccuracyOffHoist).toBeGreaterThan(0.9);
    expect(fused.floorAccuracyOffHoist - nearest.floorAccuracyOffHoist).toBeGreaterThan(0.1);
  });

  it('is deterministic', () => {
    for (const name of ESTIMATOR_NAMES) {
      expect(run(sessions.honest, name)).toEqual(run(sessions.honest, name));
    }
  });
});
