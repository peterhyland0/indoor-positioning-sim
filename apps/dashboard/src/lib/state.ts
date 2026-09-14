// The one state shape both pages render. Live builds it from server frames; Replay builds it in the
// browser from a recording. Field names match the server's Snapshot so the reducer is trivial.
import type { EventMsg } from '@sim/protocol';
import type { EstimatorName, Metrics } from '@sim/estimator';

export interface WorkerEstimate { floor: number | null; confidence: number; state: 'stable' | 'transit' | 'unknown'; punchedFloor: number | null }
export interface WorkerNow {
  id: string; platform: string; route?: string; targetFloor?: number;
  t: number; truthFloor: number; onHoist: boolean;
  estimates: Partial<Record<EstimatorName, WorkerEstimate>>;
  lastScan: { b: string; rssi: number }[];
  pressure: number;
}
export interface TimelineSample { t: number; truth: number; onHoist: boolean; est: Partial<Record<EstimatorName, number | null>> }
export interface TaggedPunch { estimator: EstimatorName; punch: { t: number; worker: string; kind: 'in' | 'out'; floor: number; verified: boolean }; truthFloor: number }
export interface SessionSummary { id: number | null; seed: number; source: 'live' | 'replay'; label: string | null; startedAt: string; floorHeight: number; numFloors: number; beacons: { id: string; floor: number }[]; workers: { id: string; platform: string; route?: string; targetFloor?: number }[] }

export interface DashboardState {
  session: SessionSummary | null;
  estimators: EstimatorName[];
  primary: EstimatorName;
  unrealConnected: boolean;
  replaying: boolean;
  connected: boolean;
  t: number;
  workers: WorkerNow[];
  punches: TaggedPunch[];
  events: EventMsg[];
  metrics: Partial<Record<EstimatorName, Metrics>>;
  timeline: Record<string, TimelineSample[]>;
  beaconState: Record<string, { alive: boolean; battery: number }>;
}

export const EMPTY_STATE: DashboardState = {
  session: null, estimators: ['gps', 'nearest', 'smoothed', 'fused'], primary: 'fused', unrealConnected: false, replaying: false, connected: false,
  t: 0, workers: [], punches: [], events: [], metrics: {}, timeline: {}, beaconState: {},
};

export type UiFrame =
  | ({ type: 'snapshot' } & Omit<DashboardState, 'connected' | 'replaying'>)
  | { type: 'tick'; worker: WorkerNow }
  | { type: 'punch'; punch: TaggedPunch }
  | { type: 'event'; event: EventMsg }
  | { type: 'metrics'; metrics: Partial<Record<EstimatorName, Metrics>>; t: number }
  | { type: 'status'; unrealConnected: boolean; replaying: boolean };

const MAX_PUNCHES = 500;
const MAX_EVENTS = 200;

export function reduce(s: DashboardState, f: UiFrame): DashboardState {
  switch (f.type) {
    case 'snapshot': {
      const { type: _t, ...rest } = f;
      return { ...s, ...rest, connected: true };
    }
    case 'tick': {
      const w = f.worker;
      const workers = s.workers.some((x) => x.id === w.id) ? s.workers.map((x) => (x.id === w.id ? w : x)) : [...s.workers, w];
      const tl = s.timeline[w.id] ?? [];
      const bucket = Math.floor(w.t);
      const est: TimelineSample['est'] = {};
      for (const [name, e] of Object.entries(w.estimates)) est[name as EstimatorName] = e?.floor ?? null;
      const sample: TimelineSample = { t: bucket, truth: w.truthFloor, onHoist: w.onHoist, est };
      const last = tl[tl.length - 1];
      const nextTl = last && last.t === bucket ? [...tl.slice(0, -1), sample] : [...tl, sample];
      return { ...s, t: Math.max(s.t, w.t), workers, timeline: { ...s.timeline, [w.id]: nextTl } };
    }
    case 'punch':
      return { ...s, punches: [...s.punches, f.punch].slice(-MAX_PUNCHES) };
    case 'event': {
      const beaconState = { ...s.beaconState };
      const b = f.event.payload['b'];
      if (typeof b === 'string' && beaconState[b]) {
        if (f.event.name === 'beaconKilled') beaconState[b] = { ...beaconState[b]!, alive: false };
        if (f.event.name === 'beaconRevived') beaconState[b] = { ...beaconState[b]!, alive: true };
        if (f.event.name === 'beaconBattery' && typeof f.event.payload['battery'] === 'number') beaconState[b] = { ...beaconState[b]!, battery: f.event.payload['battery'] };
      }
      return { ...s, events: [...s.events, f.event].slice(-MAX_EVENTS), beaconState };
    }
    case 'metrics':
      return { ...s, metrics: f.metrics, t: Math.max(s.t, f.t) };
    case 'status':
      return { ...s, unrealConnected: f.unrealConnected, replaying: f.replaying };
  }
}
