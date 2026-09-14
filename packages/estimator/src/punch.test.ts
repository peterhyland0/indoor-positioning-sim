import { describe, expect, it } from 'vitest';
import { DEFAULT_PUNCH, NAIVE_PUNCH, PunchEngine } from './punch.js';
import type { FloorEstimate } from './types.js';

const on = (floor: number, state: FloorEstimate['state'] = 'stable'): FloorEstimate => ({ floor, confidence: 0.9, state });

describe('punch engine', () => {
  it('clocks in after the enter dwell and out after the exit dwell', () => {
    const pe = new PunchEngine(DEFAULT_PUNCH);
    const all = [];
    for (let t = 0; t <= 25; t++) all.push(...pe.update(t, 'w', on(12)));
    expect(all).toEqual([{ t: 20, worker: 'w', kind: 'in', floor: 12, verified: true }]);
    for (let t = 26; t <= 60; t++) all.push(...pe.update(t, 'w', on(13)));
    expect(all[1]).toMatchObject({ kind: 'out', floor: 12, t: 56 });
    expect(all[2]).toMatchObject({ kind: 'in', floor: 13 });
  });
  it('never punches during transit and clocks out on a long one', () => {
    const pe = new PunchEngine({ ...DEFAULT_PUNCH, transitTimeoutSec: 30 });
    for (let t = 0; t <= 20; t++) pe.update(t, 'w', on(12));
    const during = [];
    for (let t = 21; t <= 60; t++) during.push(...pe.update(t, 'w', on(12, 'transit')));
    expect(during).toEqual([{ t: 51, worker: 'w', kind: 'out', floor: 12, verified: false }]);
  });
  it('naive parameters reproduce zone entry/exit on every change', () => {
    const pe = new PunchEngine(NAIVE_PUNCH);
    const all = [...pe.update(0, 'w', on(1)), ...pe.update(1, 'w', on(2)), ...pe.update(2, 'w', on(3))];
    expect(all.map((p) => `${p.kind}${p.floor}`)).toEqual(['in1', 'out1', 'in2', 'out2', 'in3']);
  });
});
