import { describe, expect, it } from 'vitest';
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { parseLine, ScanMsg } from './messages.js';
import { toObservation, toSessionInfo, beaconFloor } from './observation.js';

const sessionsDir = join(import.meta.dirname, '../../../sessions');

describe('bridge protocol', () => {
  const files = readdirSync(sessionsDir).filter((f) => f.endsWith('.jsonl') && !f.startsWith('sample'));

  it('parses every line of every canonical session', () => {
    expect(files.length).toBeGreaterThanOrEqual(4);
    for (const f of files) {
      const lines = readFileSync(join(sessionsDir, f), 'utf8').split('\n').filter(Boolean);
      const first = parseLine(lines[0]!);
      expect(first.type).toBe('session');
      let scans = 0;
      for (const line of lines) {
        const msg = parseLine(line);
        if (msg.type === 'scan') scans++;
      }
      expect(scans).toBeGreaterThan(100);
    }
  });

  it('strips ground truth from observations', () => {
    const scan = ScanMsg.parse({
      v: 1, type: 'scan', t: 1, worker: 'w01', platform: 'android', appState: 'fg', phoneState: 'inHand',
      scans: [{ b: 'F12-A', rssi: -70 }], regionEvents: [], pressure: 1008, refPressure: 1013,
      truth: { floor: 12, onHoist: false, z: 46 },
    });
    const obs = toObservation(scan);
    expect('truth' in obs).toBe(false);
    expect(obs.scans[0]?.b).toBe('F12-A');
  });

  it('resolves beacon floors from the session layout, with an id fallback', () => {
    const lines = readFileSync(join(sessionsDir, 'honest.jsonl'), 'utf8').split('\n');
    const session = parseLine(lines[0]!);
    if (session.type !== 'session') throw new Error('first line must be session');
    const info = toSessionInfo(session);
    expect(info.beacons.size).toBe(32);
    expect(beaconFloor(info, 'F12-A')).toBe(12);
    expect(beaconFloor(info, 'F99-Z')).toBe(99);
    expect(beaconFloor(info, 'nope')).toBeNull();
  });

  it('rejects malformed lines', () => {
    expect(() => parseLine('{"type":"scan"}')).toThrow();
  });
});
