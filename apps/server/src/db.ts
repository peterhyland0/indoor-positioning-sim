// Optional Postgres persistence with batched writes. Every method is a no-op when there is no
// DATABASE_URL, so the rest of the server never has to check.
import postgres from 'postgres';
import type { EventMsg, ScanMsg, SessionMsg } from '@sim/protocol';
import type { EstimatorName, FloorEstimate, Metrics, Punch } from '@sim/estimator';

type Sql = ReturnType<typeof postgres>;

interface ScanRow { session_id: number; t: number; worker: string; platform: string; app_state: string; phone_state: string; scans: unknown; region_events: unknown; pressure: number; ref_pressure: number; truth_floor: number; truth_on_hoist: boolean }
interface EstimateRow { session_id: number; t: number; worker: string; estimator: string; floor: number | null; confidence: number; state: string }
interface PunchRow { session_id: number; t: number; worker: string; estimator: string; kind: string; floor: number; verified: boolean }
interface EventRow { session_id: number; t: number; name: string; payload: unknown }

export class Db {
  private sql: Sql | null = null;
  private scans: ScanRow[] = [];
  private estimates: EstimateRow[] = [];
  private punches: PunchRow[] = [];
  private events: EventRow[] = [];
  private timer: NodeJS.Timeout | null = null;
  private flushing: Promise<void> = Promise.resolve();

  constructor(url: string | null, private readonly log: (msg: string) => void = console.log) {
    if (url) {
      this.sql = postgres(url, { max: 4, prepare: false, onnotice: () => {} });
      this.timer = setInterval(() => void this.flush(), 1000);
      this.timer.unref();
    }
  }

  get enabled(): boolean {
    return this.sql !== null;
  }

  async ping(): Promise<boolean> {
    if (!this.sql) return false;
    try {
      await this.sql`select 1`;
      return true;
    } catch (e) {
      this.log(`db: ping failed: ${(e as Error).message}`);
      return false;
    }
  }

  async createSession(msg: SessionMsg, source: 'live' | 'replay', label: string | null): Promise<number | null> {
    if (!this.sql) return null;
    const sql = this.sql;
    try {
      const [row] = await sql<{ id: number }[]>`
        insert into sessions (seed, source, label, floor_height, num_floors, config)
        values (${msg.seed}, ${source}, ${label}, ${msg.floorHeight}, ${msg.numFloors}, ${sql.json(msg.config as never)})
        returning id`;
      const id = Number(row!.id);
      if (msg.workers.length) {
        await sql`insert into workers ${sql(msg.workers.map((w) => ({ session_id: id, id: w.id, platform: w.platform, route: w.route ?? null, target_floor: w.targetFloor ?? null })))}`;
      }
      if (msg.beacons.length) {
        await sql`insert into beacons ${sql(msg.beacons.map((b) => ({ session_id: id, id: b.id, floor: b.floor, x: b.x, y: b.y, z: b.z, tx_power_dbm: b.txPowerDbm })))}`;
      }
      return id;
    } catch (e) {
      this.log(`db: createSession failed: ${(e as Error).message}`);
      return null;
    }
  }

  addScan(sessionId: number, s: ScanMsg): void {
    if (!this.sql) return;
    this.scans.push({ session_id: sessionId, t: s.t, worker: s.worker, platform: s.platform, app_state: s.appState, phone_state: s.phoneState, scans: JSON.stringify(s.scans), region_events: JSON.stringify(s.regionEvents), pressure: s.pressure, ref_pressure: s.refPressure, truth_floor: s.truth.floor, truth_on_hoist: s.truth.onHoist });
    if (this.scans.length >= 500) void this.flush();
  }

  addEstimate(sessionId: number, t: number, worker: string, estimator: EstimatorName, est: FloorEstimate): void {
    if (!this.sql) return;
    this.estimates.push({ session_id: sessionId, t, worker, estimator, floor: est.floor, confidence: est.confidence, state: est.state });
  }

  addPunch(sessionId: number, estimator: EstimatorName, p: Punch): void {
    if (!this.sql) return;
    this.punches.push({ session_id: sessionId, t: p.t, worker: p.worker, estimator, kind: p.kind, floor: p.floor, verified: p.verified });
  }

  addEvent(sessionId: number, e: EventMsg): void {
    if (!this.sql) return;
    this.events.push({ session_id: sessionId, t: e.t, name: e.name, payload: JSON.stringify(e.payload) });
  }

  async upsertMetrics(sessionId: number, metrics: Partial<Record<EstimatorName, Metrics>>): Promise<void> {
    if (!this.sql) return;
    const sql = this.sql;
    try {
      for (const [estimator, m] of Object.entries(metrics)) {
        await sql`insert into metrics (session_id, estimator, computed_at, values) values (${sessionId}, ${estimator}, now(), ${sql.json(m as never)})
                  on conflict (session_id, estimator) do update set computed_at = now(), values = excluded.values`;
      }
    } catch (e) {
      this.log(`db: upsertMetrics failed: ${(e as Error).message}`);
    }
  }

  /** Write everything queued. Serialised so batches never interleave. */
  flush(): Promise<void> {
    if (!this.sql) return Promise.resolve();
    const sql = this.sql;
    const scans = this.scans; this.scans = [];
    const estimates = this.estimates; this.estimates = [];
    const punches = this.punches; this.punches = [];
    const events = this.events; this.events = [];
    if (!scans.length && !estimates.length && !punches.length && !events.length) return this.flushing;
    this.flushing = this.flushing.then(async () => {
      try {
        for (let i = 0; i < scans.length; i += 500) await sql`insert into scans ${sql(scans.slice(i, i + 500).map((r) => ({ ...r, scans: sql.json(JSON.parse(r.scans as string)), region_events: sql.json(JSON.parse(r.region_events as string)) })))}`;
        for (let i = 0; i < estimates.length; i += 1000) await sql`insert into estimates ${sql(estimates.slice(i, i + 1000))}`;
        if (punches.length) await sql`insert into punches ${sql(punches)}`;
        for (const e of events) await sql`insert into events ${sql({ ...e, payload: sql.json(JSON.parse(e.payload as string)) })}`;
      } catch (e) {
        this.log(`db: flush failed (${scans.length} scans, ${estimates.length} estimates): ${(e as Error).message}`);
      }
    });
    return this.flushing;
  }

  // ---- reads for the dashboard's history page
  async listSessions(): Promise<{ id: number; seed: number; source: string; label: string | null; started_at: string; num_floors: number; scans: number; metrics: Record<string, Metrics> }[]> {
    if (!this.sql) return [];
    const sql = this.sql;
    const rows = await sql<{ id: number; seed: number; source: string; label: string | null; started_at: string; num_floors: number; scans: number; metrics: Record<string, Metrics> | null }[]>`
      select s.id, s.seed, s.source, s.label, s.started_at, s.num_floors,
             (select count(*) from scans sc where sc.session_id = s.id)::int as scans,
             (select jsonb_object_agg(m.estimator, m.values) from metrics m where m.session_id = s.id) as metrics
      from sessions s order by s.started_at desc limit 100`;
    return rows.map((r) => ({ ...r, id: Number(r.id), metrics: r.metrics ?? {} }));
  }

  async sessionTimeline(sessionId: number, worker: string | null): Promise<{ t: number; worker: string; truth: number; on_hoist: boolean; estimator: string; floor: number | null }[]> {
    if (!this.sql) return [];
    const sql = this.sql;
    return sql<{ t: number; worker: string; truth: number; on_hoist: boolean; estimator: string; floor: number | null }[]>`
      select e.t, e.worker, sc.truth_floor as truth, sc.truth_on_hoist as on_hoist, e.estimator, e.floor
      from estimates e
      join scans sc on sc.session_id = e.session_id and sc.worker = e.worker and sc.t = e.t
      where e.session_id = ${sessionId} ${worker ? sql`and e.worker = ${worker}` : sql``}
      order by e.t limit 50000`;
  }

  async sessionPunches(sessionId: number): Promise<{ t: number; worker: string; estimator: string; kind: string; floor: number; verified: boolean }[]> {
    if (!this.sql) return [];
    return this.sql`select t, worker, estimator, kind, floor, verified from punches where session_id = ${sessionId} order by t`;
  }

  async close(): Promise<void> {
    if (this.timer) clearInterval(this.timer);
    await this.flush();
    await this.sql?.end({ timeout: 2 });
  }
}
