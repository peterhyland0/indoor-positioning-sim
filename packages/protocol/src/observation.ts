// What an estimator is allowed to see: a scan message with the ground truth removed.
// The type makes the separation explicit; `toObservation` is the only way to build one.
import type { ScanMsg, SessionMsg, BeaconInfo, WorkerInfo } from './messages.js';

export type Observation = Omit<ScanMsg, 'truth'>;

export function toObservation(scan: ScanMsg): Observation {
  const { truth: _truth, ...rest } = scan;
  return rest;
}

/** Static facts about a session that estimators may use (beacon placement is installed knowledge). */
export interface SessionInfo {
  seed: number;
  floorHeight: number;
  numFloors: number;
  beacons: Map<string, BeaconInfo>;
  workers: WorkerInfo[];
  config: Record<string, unknown>;
}

export function toSessionInfo(msg: SessionMsg): SessionInfo {
  return {
    seed: msg.seed,
    floorHeight: msg.floorHeight,
    numFloors: msg.numFloors,
    beacons: new Map(msg.beacons.map((b) => [b.id, b])),
    workers: msg.workers,
    config: msg.config,
  };
}

/** Floor of a beacon by id; falls back to parsing "F12-A" if the layout is unknown. */
export function beaconFloor(info: SessionInfo, id: string): number | null {
  const b = info.beacons.get(id);
  if (b) return b.floor;
  const m = /^F(\d+)/.exec(id);
  return m ? Number(m[1]) : null;
}
