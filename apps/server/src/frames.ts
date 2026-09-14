// Frames sent to dashboard clients on /ui. Kept as plain types (no zod) - the dashboard imports them.
import type { BeaconInfo, EventMsg, WorkerInfo } from '@sim/protocol';
import type { EstimatorName, FloorEstimate, Metrics, Punch } from '@sim/estimator';

export interface WorkerEstimate extends FloorEstimate {
  punchedFloor: number | null;
}

export interface WorkerNow {
  id: string;
  platform: string;
  route?: string;
  targetFloor?: number;
  t: number;
  truthFloor: number;
  onHoist: boolean;
  estimates: Partial<Record<EstimatorName, WorkerEstimate>>;
  lastScan: { b: string; rssi: number }[];
  pressure: number;
}

/** One second of one worker's history, for the timeline strips. */
export interface TimelineSample {
  t: number;
  truth: number;
  onHoist: boolean;
  est: Partial<Record<EstimatorName, number | null>>;
}

export interface SessionSummary {
  id: number | null;
  seed: number;
  source: 'live' | 'replay';
  label: string | null;
  startedAt: string;
  floorHeight: number;
  numFloors: number;
  beacons: BeaconInfo[];
  workers: WorkerInfo[];
}

export interface Snapshot {
  type: 'snapshot';
  session: SessionSummary | null;
  estimators: EstimatorName[];
  primary: EstimatorName;
  unrealConnected: boolean;
  replaying?: boolean;
  t: number;
  workers: WorkerNow[];
  punches: TaggedPunch[];
  events: EventMsg[];
  metrics: Partial<Record<EstimatorName, Metrics>>;
  timeline: Record<string, TimelineSample[]>;
  beaconState: Record<string, { alive: boolean; battery: number }>;
}

export interface TaggedPunch {
  estimator: EstimatorName;
  punch: Punch;
  /** true floor at punch time */
  truthFloor: number;
}

export type UiFrame =
  | Snapshot
  | { type: 'tick'; worker: WorkerNow }
  | { type: 'punch'; punch: TaggedPunch }
  | { type: 'event'; event: EventMsg }
  | { type: 'metrics'; metrics: Partial<Record<EstimatorName, Metrics>>; t: number }
  | { type: 'status'; unrealConnected: boolean; replaying: boolean };

/** Dashboard -> server. */
export type UiCommand = { type: 'command'; cmd: string } | { type: 'replay'; file: string; speed: number } | { type: 'stop' };
