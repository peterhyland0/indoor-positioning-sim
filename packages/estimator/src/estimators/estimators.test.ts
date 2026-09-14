import { describe, expect, it } from 'vitest';
import type { Observation, SessionInfo } from '@sim/protocol';
import { NearestBeaconEstimator } from './nearest.js';
import { SmoothedBeaconEstimator } from './smoothed.js';
import { FusedEstimator } from './fused.js';
import { SiteGeofenceEstimator } from './gps.js';

const FLOOR_H = 3.8;
const PPM = 0.1205;

function info(): SessionInfo {
  const beacons = new Map();
  for (let f = 0; f <= 15; f++) {
    beacons.set(`F${String(f).padStart(2, '0')}-A`, { id: `F${String(f).padStart(2, '0')}-A`, floor: f, x: 12, y: 9.5, z: f * FLOOR_H + 3.2, txPowerDbm: -59 });
  }
  return { seed: 1, floorHeight: FLOOR_H, numFloors: 15, beacons, workers: [{ id: 'w', platform: 'android', targetFloor: 12 }], config: {} };
}

/** An observation of a phone standing on `floor`, hearing the given beacons. */
function obs(t: number, floor: number, scans: [string, number][], extra: Partial<Observation> = {}): Observation {
  const h = floor * FLOOR_H + 0.9;
  return {
    v: 1, type: 'scan', t, worker: 'w', platform: 'android', appState: 'fg', phoneState: 'inHand',
    scans: scans.map(([b, rssi]) => ({ b, rssi })), regionEvents: [],
    pressure: 1013.25 - h * PPM, refPressure: 1013.25, ...extra,
  };
}

describe('nearest', () => {
  it('reports the floor of the strongest beacon and nothing when silent', () => {
    const e = new NearestBeaconEstimator();
    e.reset(info());
    expect(e.update(obs(1, 12, [['F12-A', -70], ['F13-A', -80]])).floor).toBe(12);
    expect(e.update(obs(2, 12, [['F12-A', -90], ['F13-A', -80]])).floor).toBe(13); // one loud bleed flips it
    expect(e.update(obs(3, 12, [])).floor).toBeNull();
  });
  it('uses iOS region enter events when there are no scans', () => {
    const e = new NearestBeaconEstimator();
    e.reset(info());
    expect(e.update(obs(1, 5, [], { regionEvents: [{ b: 'F05-A', event: 'enter' }] })).floor).toBe(5);
  });
});

describe('smoothed', () => {
  it('ignores a single-scan bleed but follows a sustained change', () => {
    const e = new SmoothedBeaconEstimator();
    e.reset(info());
    for (let t = 0; t < 20; t++) e.update(obs(t, 12, [['F12-A', -70]]));
    expect(e.update(obs(20, 12, [['F12-A', -90], ['F13-A', -65]])).floor).toBe(12); // spike: no switch
    let f = 12;
    for (let t = 21; t < 60; t++) f = e.update(obs(t, 13, [['F13-A', -65]])).floor!;
    expect(f).toBe(13);
  });
});

describe('fused', () => {
  it('holds the floor from the barometer alone when beacons are dead', () => {
    const e = new FusedEstimator();
    e.reset(info());
    let r = e.update(obs(0, 12, []));
    for (let t = 1; t < 30; t++) r = e.update(obs(t, 12, []));
    expect(r.floor).toBe(12);
    expect(r.state).toBe('stable');
  });
  it('is not fooled by loud bleed from the floor above', () => {
    const e = new FusedEstimator();
    e.reset(info());
    let r = e.update(obs(0, 12, [['F12-A', -75]]));
    for (let t = 1; t < 40; t++) r = e.update(obs(t, 12, [['F12-A', -80], ['F13-A', -66]]));
    expect(r.floor).toBe(12);
  });
  it('enters transit on a hoist ride and settles on the destination', () => {
    const e = new FusedEstimator();
    e.reset(info());
    for (let t = 0; t < 10; t++) e.update(obs(t, 0, [['F00-A', -70]]));
    const states = new Set<string>();
    // ride: 1 m/s from floor 0 to floor 12 = 45.6 m
    for (let t = 10; t < 56; t++) {
      const h = Math.min((t - 10) * 1.0, 12 * FLOOR_H) + 0.9;
      const r = e.update({ ...obs(t, 0, []), pressure: 1013.25 - h * PPM });
      states.add(r.state);
    }
    expect(states.has('transit')).toBe(true);
    let r = e.update(obs(56, 12, [['F12-A', -70]]));
    for (let t = 57; t < 80; t++) r = e.update(obs(t, 12, [['F12-A', -70]]));
    expect(r.state).toBe('stable');
    expect(r.floor).toBe(12);
  });
});

describe('gps', () => {
  it('credits the assigned floor wherever the phone is', () => {
    const e = new SiteGeofenceEstimator();
    e.reset(info());
    expect(e.update(obs(1, 0, [['F00-A', -60]])).floor).toBe(12);
  });
});
