// One active session at a time: holds every estimator's runner, turns bridge messages into
// dashboard frames, persists, and produces the estimate to send back to Unreal.
import { toSessionInfo, type EventMsg, type ScanMsg, type SessionMsg, type BeaconInfo, type WorkerInfo, type EstimateMsg } from '@sim/protocol';
import { createEstimator, ESTIMATOR_NAMES, SessionRunner, DEFAULT_SCORE, type EstimatorName, type Metrics, type Punch } from '@sim/estimator';
import type { Db } from './db';
import type { Snapshot, TaggedPunch, TimelineSample, UiFrame, WorkerNow } from './frames';

const MAX_PUNCHES = 500;
const MAX_EVENTS = 200;
const TIMELINE_BUCKET_SEC = 1;
const METRICS_EVERY_SEC = 2;

export class Pipeline {
  private runners = new Map<EstimatorName, SessionRunner>();
  private sessionMsg: SessionMsg | null = null;
  private dbSessionId: number | null = null;
  private source: 'live' | 'replay' = 'live';
  private label: string | null = null;
  private startedAt = new Date().toISOString();
  private t = 0;
  private workers = new Map<string, WorkerNow>();
  private punches: TaggedPunch[] = [];
  private events: EventMsg[] = [];
  private metrics: Partial<Record<EstimatorName, Metrics>> = {};
  private lastMetricsT = -Infinity;
  private timeline = new Map<string, TimelineSample[]>();
  private beaconState: Record<string, { alive: boolean; battery: number }> = {};

  constructor(
    private readonly db: Db,
    private readonly primary: EstimatorName,
    private readonly hourlyRate: number,
    private readonly emit: (frame: UiFrame) => void,
    private readonly toUnreal: (msg: EstimateMsg) => void,
  ) {}

  get active(): boolean {
    return this.sessionMsg !== null;
  }

  async startSession(msg: SessionMsg, source: 'live' | 'replay', label: string | null): Promise<void> {
    this.sessionMsg = msg;
    this.source = source;
    this.label = label;
    this.startedAt = new Date().toISOString();
    this.t = 0;
    this.workers.clear();
    this.punches = [];
    this.events = [];
    this.metrics = {};
    this.lastMetricsT = -Infinity;
    this.timeline.clear();
    this.beaconState = Object.fromEntries(msg.beacons.map((b) => [b.id, { alive: true, battery: 1 }]));
    const info = toSessionInfo(msg);
    this.runners.clear();
    for (const name of ESTIMATOR_NAMES) {
      const { estimator, punchParams } = createEstimator(name);
      this.runners.set(name, new SessionRunner(estimator, info, punchParams, { ...DEFAULT_SCORE, hourlyRate: this.hourlyRate }));
    }
    for (const w of msg.workers) {
      this.workers.set(w.id, this.emptyWorker(w));
      this.timeline.set(w.id, []);
    }
    this.dbSessionId = await this.db.createSession(msg, source, label);
    this.emit(this.snapshot(false));
  }

  private emptyWorker(w: WorkerInfo): WorkerNow {
    const now: WorkerNow = { id: w.id, platform: w.platform, t: 0, truthFloor: 0, onHoist: false, estimates: {}, lastScan: [], pressure: 0 };
    if (w.route !== undefined) now.route = w.route;
    if (w.targetFloor !== undefined) now.targetFloor = w.targetFloor;
    return now;
  }

  handleScan(scan: ScanMsg): void {
    if (!this.sessionMsg) return;
    this.t = Math.max(this.t, scan.t);
    let now = this.workers.get(scan.worker);
    if (!now) {
      now = this.emptyWorker({ id: scan.worker, platform: scan.platform });
      this.workers.set(scan.worker, now);
      this.timeline.set(scan.worker, []);
    }
    now.t = scan.t;
    now.truthFloor = scan.truth.floor;
    now.onHoist = scan.truth.onHoist;
    now.lastScan = scan.scans;
    now.pressure = scan.pressure;

    if (this.dbSessionId !== null) this.db.addScan(this.dbSessionId, scan);

    const estFloors: Partial<Record<EstimatorName, number | null>> = {};
    for (const [name, runner] of this.runners) {
      const { tick, punches } = runner.step(scan);
      now.estimates[name] = { ...tick.est, punchedFloor: tick.punchedFloor };
      estFloors[name] = tick.est.floor;
      if (this.dbSessionId !== null) this.db.addEstimate(this.dbSessionId, scan.t, scan.worker, name, tick.est);
      for (const p of punches) this.recordPunch(name, p, scan.truth.floor);
      if (name === this.primary) {
        const punch = punches.find((p) => p.worker === scan.worker);
        this.toUnreal({ v: 1, type: 'estimate', worker: scan.worker, estFloor: tick.est.floor, punch: punch ? punch.kind : null, source: name });
      }
    }

    // Timeline: one sample per second per worker (last sample in the bucket wins).
    const tl = this.timeline.get(scan.worker)!;
    const bucket = Math.floor(scan.t / TIMELINE_BUCKET_SEC) * TIMELINE_BUCKET_SEC;
    const last = tl[tl.length - 1];
    const sample: TimelineSample = { t: bucket, truth: scan.truth.floor, onHoist: scan.truth.onHoist, est: estFloors };
    if (last && last.t === bucket) tl[tl.length - 1] = sample;
    else tl.push(sample);

    this.emit({ type: 'tick', worker: now });

    if (scan.t - this.lastMetricsT >= METRICS_EVERY_SEC) {
      this.lastMetricsT = scan.t;
      this.refreshMetrics();
    }
  }

  private recordPunch(estimator: EstimatorName, punch: Punch, truthFloor: number): void {
    const tagged: TaggedPunch = { estimator, punch, truthFloor };
    this.punches.push(tagged);
    if (this.punches.length > MAX_PUNCHES) this.punches.shift();
    if (this.dbSessionId !== null) this.db.addPunch(this.dbSessionId, estimator, punch);
    this.emit({ type: 'punch', punch: tagged });
  }

  handleEvent(event: EventMsg): void {
    if (!this.sessionMsg) return;
    this.t = Math.max(this.t, event.t);
    this.events.push(event);
    if (this.events.length > MAX_EVENTS) this.events.shift();
    const b = event.payload['b'];
    if (typeof b === 'string' && this.beaconState[b]) {
      if (event.name === 'beaconKilled') this.beaconState[b] = { ...this.beaconState[b]!, alive: false };
      if (event.name === 'beaconRevived') this.beaconState[b] = { ...this.beaconState[b]!, alive: true };
      if (event.name === 'beaconBattery' && typeof event.payload['battery'] === 'number') this.beaconState[b] = { ...this.beaconState[b]!, battery: event.payload['battery'] };
    }
    if (this.dbSessionId !== null) this.db.addEvent(this.dbSessionId, event);
    this.emit({ type: 'event', event });
  }

  refreshMetrics(): void {
    for (const [name, runner] of this.runners) this.metrics[name] = runner.metrics();
    this.emit({ type: 'metrics', metrics: this.metrics, t: this.t });
    if (this.dbSessionId !== null) void this.db.upsertMetrics(this.dbSessionId, this.metrics);
  }

  /** Called when the source ends (Unreal disconnects, replay finishes). */
  async finish(): Promise<void> {
    if (!this.sessionMsg) return;
    this.refreshMetrics();
    await this.db.flush();
  }

  snapshot(unrealConnected: boolean): Snapshot {
    const s = this.sessionMsg;
    return {
      type: 'snapshot',
      session: s
        ? { id: this.dbSessionId, seed: s.seed, source: this.source, label: this.label, startedAt: this.startedAt, floorHeight: s.floorHeight, numFloors: s.numFloors, beacons: s.beacons as BeaconInfo[], workers: s.workers }
        : null,
      estimators: [...ESTIMATOR_NAMES],
      primary: this.primary,
      unrealConnected,
      t: this.t,
      workers: [...this.workers.values()],
      punches: this.punches,
      events: this.events,
      metrics: this.metrics,
      timeline: Object.fromEntries(this.timeline),
      beaconState: this.beaconState,
    };
  }
}
