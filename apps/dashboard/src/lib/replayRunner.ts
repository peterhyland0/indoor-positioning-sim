// Runs a recording through every estimator in the browser, then serves the dashboard state at any
// time cursor. Same packages as the server, so the numbers match the CLI exactly.
import { parseSession, createEstimator, SessionRunner, score, DEFAULT_SCORE, type EstimatorName, type Metrics, type Punch, type Tick } from '@sim/estimator';
import type { ScanMsg, EventMsg } from '@sim/protocol';
import { EMPTY_STATE, type DashboardState, type TaggedPunch, type TimelineSample, type WorkerNow } from './state';

interface Run { ticks: Tick[]; punches: Punch[] }

export class ReplayRunner {
  readonly duration: number;
  private readonly runs: Record<EstimatorName, Run>;
  private readonly scans: ScanMsg[];
  private readonly events: EventMsg[];
  private readonly base: DashboardState;
  private metricsCache = new Map<number, Partial<Record<EstimatorName, Metrics>>>();

  constructor(text: string, label: string, readonly estimators: EstimatorName[] = ['gps', 'nearest', 'smoothed', 'fused']) {
    const session = parseSession(text);
    this.scans = session.scans;
    this.events = session.events;
    this.duration = this.scans.length ? this.scans[this.scans.length - 1]!.t : 0;
    this.runs = {} as Record<EstimatorName, Run>;
    for (const name of estimators) {
      const { estimator, punchParams } = createEstimator(name);
      const runner = new SessionRunner(estimator, session.info, punchParams, DEFAULT_SCORE);
      for (const s of this.scans) runner.step(s);
      this.runs[name] = { ticks: runner.ticks, punches: runner.punches };
    }
    this.base = {
      ...EMPTY_STATE,
      session: {
        id: null, seed: session.info.seed, source: 'replay', label, startedAt: new Date().toISOString(),
        floorHeight: session.info.floorHeight, numFloors: session.info.numFloors,
        beacons: [...session.info.beacons.values()], workers: session.info.workers,
      },
      estimators, connected: true, replaying: true,
      beaconState: Object.fromEntries([...session.info.beacons.keys()].map((id) => [id, { alive: true, battery: 1 }])),
    };
  }

  /** Dashboard state as of sim time `t`. */
  at(t: number): DashboardState {
    const upTo = this.scans.findIndex((s) => s.t > t);
    const n = upTo < 0 ? this.scans.length : upTo;
    const workers = new Map<string, WorkerNow>();
    const timeline: Record<string, TimelineSample[]> = {};
    for (const w of this.base.session!.workers) {
      workers.set(w.id, { id: w.id, platform: w.platform, ...(w.route !== undefined ? { route: w.route } : {}), ...(w.targetFloor !== undefined ? { targetFloor: w.targetFloor } : {}), t: 0, truthFloor: 0, onHoist: false, estimates: {}, lastScan: [], pressure: 0 });
      timeline[w.id] = [];
    }
    for (let i = 0; i < n; i++) {
      const scan = this.scans[i]!;
      const w = workers.get(scan.worker)!;
      w.t = scan.t; w.truthFloor = scan.truth.floor; w.onHoist = scan.truth.onHoist; w.lastScan = scan.scans; w.pressure = scan.pressure;
      const est: TimelineSample['est'] = {};
      for (const name of this.estimators) {
        const tick = this.runs[name].ticks[i]!;
        w.estimates[name] = { ...tick.est, punchedFloor: tick.punchedFloor };
        est[name] = tick.est.floor;
      }
      const tl = timeline[scan.worker]!;
      const bucket = Math.floor(scan.t);
      const sample: TimelineSample = { t: bucket, truth: scan.truth.floor, onHoist: scan.truth.onHoist, est };
      if (tl.length && tl[tl.length - 1]!.t === bucket) tl[tl.length - 1] = sample; else tl.push(sample);
    }
    const punches: TaggedPunch[] = [];
    for (const name of this.estimators) {
      for (const p of this.runs[name].punches) {
        if (p.t > t) break;
        const truth = this.truthAt(p.worker, p.t);
        punches.push({ estimator: name, punch: p, truthFloor: truth });
      }
    }
    punches.sort((a, b) => a.punch.t - b.punch.t);
    const beaconState = { ...this.base.beaconState };
    const events = this.events.filter((e) => e.t <= t);
    for (const e of events) {
      const b = e.payload['b'];
      if (typeof b === 'string' && beaconState[b]) {
        if (e.name === 'beaconKilled') beaconState[b] = { ...beaconState[b]!, alive: false };
        if (e.name === 'beaconRevived') beaconState[b] = { ...beaconState[b]!, alive: true };
      }
    }
    return { ...this.base, t, workers: [...workers.values()], punches: punches.slice(-500), events: events.slice(-200), timeline, beaconState, metrics: this.metricsAt(n) };
  }

  private truthAt(worker: string, t: number): number {
    let f = 0;
    for (const s of this.scans) { if (s.t > t) break; if (s.worker === worker) f = s.truth.floor; }
    return f;
  }

  /** Metrics over the first n scans; cached per 20-scan bucket so scrubbing stays cheap. */
  private metricsAt(n: number): Partial<Record<EstimatorName, Metrics>> {
    const key = Math.floor(n / 20) * 20;
    const cached = this.metricsCache.get(key);
    if (cached) return cached;
    const out: Partial<Record<EstimatorName, Metrics>> = {};
    for (const name of this.estimators) {
      const run = this.runs[name];
      const tLimit = key < this.scans.length ? this.scans[key]!.t : Infinity;
      out[name] = score(name, run.ticks.slice(0, key), run.punches.filter((p) => p.t < tLimit), DEFAULT_SCORE);
    }
    this.metricsCache.set(key, out);
    return out;
  }
}
