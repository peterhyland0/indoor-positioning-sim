import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import WebSocket from 'ws';
import { startServer, type RunningServer } from './server.js';
import type { UiFrame } from './frames.js';

const sessionsDir = resolve(import.meta.dirname, '../../../sessions');
let server: RunningServer;

/** A UI/Unreal client that buffers every frame from the moment it connects (frames can arrive in the same tick as 'open'). */
class Client {
  readonly frames: UiFrame[] = [];
  readonly raw: unknown[] = [];
  private waiters: { type: string; ok: (f: UiFrame) => void }[] = [];
  constructor(readonly ws: WebSocket) {
    ws.on('message', (data) => {
      const f = JSON.parse(data.toString()) as UiFrame;
      this.raw.push(f);
      this.frames.push(f);
      const i = this.waiters.findIndex((w) => w.type === f.type);
      if (i >= 0) this.waiters.splice(i, 1)[0]!.ok(f);
    });
  }
  static open(url: string): Promise<Client> {
    return new Promise((ok, err) => { const ws = new WebSocket(url); const c = new Client(ws); ws.once('open', () => ok(c)); ws.once('error', err); });
  }
  /** Resolve with the next frame of `type` not yet consumed. */
  next<T extends UiFrame['type']>(type: T, timeoutMs = 10000): Promise<Extract<UiFrame, { type: T }>> {
    const i = this.frames.findIndex((f) => f.type === type);
    if (i >= 0) return Promise.resolve(this.frames.splice(i, 1)[0] as Extract<UiFrame, { type: T }>);
    return new Promise((ok, err) => {
      const timer = setTimeout(() => err(new Error(`no ${type} frame within ${timeoutMs} ms`)), timeoutMs);
      this.waiters.push({ type, ok: (f) => { clearTimeout(timer); this.frames.splice(this.frames.indexOf(f), 1); ok(f as Extract<UiFrame, { type: T }>); } });
    });
  }
  send(text: string) { this.ws.send(text); }
  close() { this.ws.close(); }
}

beforeAll(async () => {
  server = await startServer({ port: 0, databaseUrl: null, primary: 'fused', hourlyRate: 45, sessionsDir }, () => {});
});
afterAll(async () => { await server.close(); });

describe('server', () => {
  it('answers health', async () => {
    const r = await fetch(`http://localhost:${server.port}/api/health`).then((r) => r.json());
    expect(r).toMatchObject({ ok: true, db: false, primary: 'fused' });
  });

  it('runs a fake Unreal session: estimates come back, the dashboard sees ticks and punches', async () => {
    const ui = await Client.open(`ws://localhost:${server.port}/ui`);
    await ui.next('snapshot');
    const unreal = await Client.open(`ws://localhost:${server.port}/`);
    const estimates = unreal.raw;

    const lines = readFileSync(resolve(sessionsDir, 'honest.jsonl'), 'utf8').split('\n').filter(Boolean).slice(0, 400);
    for (const line of lines) unreal.send(line);

    const snap = await ui.next('snapshot');
    const punch = await ui.next('punch');
    expect(snap.session?.seed).toBe(42);
    expect(snap.estimators).toEqual(['gps', 'nearest', 'smoothed', 'fused']);
    expect(['gps', 'nearest', 'smoothed', 'fused']).toContain(punch.punch.estimator);
    // the snapshot for the new session must precede its first tick
    expect(snap.workers.every((w) => w.t === 0)).toBe(true);
    await new Promise((r) => setTimeout(r, 200));
    expect(estimates.length).toBeGreaterThan(100);
    expect(estimates[0]).toMatchObject({ type: 'estimate', source: 'fused' });

    const state = await fetch(`http://localhost:${server.port}/api/state`).then((r) => r.json()) as { workers: { estimates: Record<string, unknown> }[] };
    expect(state.workers.length).toBe(4);
    expect(Object.keys(state.workers[0]!.estimates)).toHaveLength(4);
    unreal.close();
    ui.close();
  });

  it('replays a recording through the same pipeline and produces metrics', async () => {
    const ui = await Client.open(`ws://localhost:${server.port}/ui`);
    await ui.next('snapshot');
    const r = await fetch(`http://localhost:${server.port}/api/replay`, { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ file: 'lobby-cheat.jsonl', speed: 0 }) });
    expect(r.status).toBe(202);
    const m = await ui.next('metrics', 20000);
    expect(m.metrics.fused).toBeDefined();
    // wait for the replay to finish, then the final metrics must show the lobby cheat caught
    for (let i = 0; i < 100; i++) {
      const h = await fetch(`http://localhost:${server.port}/api/health`).then((r) => r.json()) as { replaying: boolean };
      if (!h.replaying) break;
      await new Promise((r) => setTimeout(r, 100));
    }
    const state = await fetch(`http://localhost:${server.port}/api/state`).then((r) => r.json()) as { metrics: Record<string, { lobbyMinutesCredited: number }>; session: { source: string } };
    expect(state.session.source).toBe('replay');
    expect(state.metrics.gps!.lobbyMinutesCredited).toBeGreaterThan(15);
    expect(state.metrics.fused!.lobbyMinutesCredited).toBeLessThan(3);
    ui.close();
  });

  it('rejects path traversal in replay', async () => {
    const r = await fetch(`http://localhost:${server.port}/api/replay`, { method: 'POST', body: JSON.stringify({ file: '../package.json' }) });
    expect(r.status).toBe(400);
  });
});
