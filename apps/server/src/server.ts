// HTTP + WebSocket server. `/` is the Unreal bridge, `/ui` the dashboard feed, `/api/*` REST.
import { createServer, type IncomingMessage, type ServerResponse } from 'node:http';
import { readdir, stat } from 'node:fs/promises';
import { basename, join, resolve } from 'node:path';
import { WebSocketServer, WebSocket } from 'ws';
import { tryParseLine, type CommandMsg, type EstimateMsg } from '@sim/protocol';
import { Db } from './db';
import { Pipeline } from './pipeline';
import { Replayer } from './replay';
import type { Config } from './config';
import type { UiCommand, UiFrame } from './frames';

export interface RunningServer {
  port: number;
  close(): Promise<void>;
}

export async function startServer(cfg: Config, log: (msg: string) => void = console.log): Promise<RunningServer> {
  const db = new Db(cfg.databaseUrl, log);
  if (db.enabled) log(`db: ${(await db.ping()) ? 'connected' : 'UNREACHABLE (writes will fail)'}`);
  else log('db: disabled (no DATABASE_URL)');

  const uiClients = new Set<WebSocket>();
  let unreal: WebSocket | null = null;

  const broadcast = (frame: UiFrame) => {
    const text = JSON.stringify(frame);
    for (const c of uiClients) if (c.readyState === WebSocket.OPEN) c.send(text);
  };
  const toUnreal = (msg: EstimateMsg | CommandMsg) => {
    if (unreal && unreal.readyState === WebSocket.OPEN) unreal.send(JSON.stringify(msg));
  };

  const pipeline = new Pipeline(db, cfg.primary, cfg.hourlyRate, broadcast, toUnreal);
  const replayer = new Replayer(pipeline);
  const status = () => broadcast({ type: 'status', unrealConnected: unreal !== null, replaying: replayer.running });

  // ---- REST
  const json = (res: ServerResponse, code: number, body: unknown) => {
    res.writeHead(code, { 'content-type': 'application/json', 'access-control-allow-origin': '*', 'access-control-allow-headers': 'content-type' });
    res.end(JSON.stringify(body));
  };
  const readBody = (req: IncomingMessage) => new Promise<string>((ok) => { let b = ''; req.on('data', (c) => (b += c)); req.on('end', () => ok(b)); });
  const recordingPath = (file: string) => {
    const p = resolve(cfg.sessionsDir, basename(file)); // no path traversal
    return p.startsWith(cfg.sessionsDir) && p.endsWith('.jsonl') ? p : null;
  };

  const http = createServer(async (req, res) => {
    const url = new URL(pathOf(req.url) + (req.url?.includes('?') ? '?' + req.url.split('?')[1] : ''), 'http://localhost');
    try {
      if (req.method === 'OPTIONS') return json(res, 204, {});
      if (url.pathname === '/api/health') return json(res, 200, { ok: true, unrealConnected: unreal !== null, replaying: replayer.running, db: db.enabled, primary: cfg.primary });
      if (url.pathname === '/api/state') return json(res, 200, { ...pipeline.snapshot(unreal !== null), replaying: replayer.running });
      if (url.pathname === '/api/recordings') {
        const files = (await readdir(cfg.sessionsDir)).filter((f) => f.endsWith('.jsonl'));
        const out = [];
        for (const f of files) { const s = await stat(join(cfg.sessionsDir, f)); out.push({ file: f, bytes: s.size }); }
        return json(res, 200, out);
      }
      if (url.pathname === '/api/sessions' && req.method === 'GET') return json(res, 200, await db.listSessions());
      const m = /^\/api\/sessions\/(\d+)\/(timeline|punches)$/.exec(url.pathname);
      if (m) {
        const id = Number(m[1]);
        if (m[2] === 'timeline') return json(res, 200, await db.sessionTimeline(id, url.searchParams.get('worker')));
        return json(res, 200, await db.sessionPunches(id));
      }
      if (url.pathname === '/api/replay' && req.method === 'POST') {
        const body = JSON.parse((await readBody(req)) || '{}') as { file?: string; speed?: number };
        const p = body.file ? recordingPath(body.file) : null;
        if (!p) return json(res, 400, { error: 'file must name a .jsonl in the sessions dir' });
        if (replayer.running) replayer.stop();
        const speed = Number(body.speed ?? 1);
        void replayer.run(p, basename(p, '.jsonl'), speed).then((r) => { log(`replay ${basename(p)} done: ${r.scans} scans`); status(); });
        status();
        return json(res, 202, { started: basename(p), speed });
      }
      if (url.pathname === '/api/replay' && req.method === 'DELETE') { replayer.stop(); return json(res, 200, { stopped: true }); }
      if (url.pathname === '/api/command' && req.method === 'POST') {
        const body = JSON.parse((await readBody(req)) || '{}') as { cmd?: string };
        if (!body.cmd) return json(res, 400, { error: 'cmd required' });
        toUnreal({ v: 1, type: 'command', cmd: body.cmd });
        return json(res, 200, { sent: unreal !== null });
      }
      json(res, 404, { error: 'not found' });
    } catch (e) {
      json(res, 500, { error: (e as Error).message });
    }
  });

  // ---- WebSockets
  const wss = new WebSocketServer({ server: http });
  const pathOf = (target: string | undefined): string => {
    // Browsers/ws send "/ui"; Unreal's client may send an absolute-form target or nothing at all.
    if (!target) return '/';
    if (target.startsWith('/')) return target.split('?')[0]!;
    try { return new URL(target).pathname; } catch { return '/'; }
  };
  wss.on('connection', (ws, req) => {
    const path = pathOf(req.url);
    if (path === '/ui') {
      uiClients.add(ws);
      ws.send(JSON.stringify({ ...pipeline.snapshot(unreal !== null), replaying: replayer.running }));
      ws.on('message', (raw) => {
        let cmd: UiCommand;
        try { cmd = JSON.parse(raw.toString()) as UiCommand; } catch { return; }
        if (cmd.type === 'command') toUnreal({ v: 1, type: 'command', cmd: cmd.cmd });
        else if (cmd.type === 'replay') { const p = recordingPath(cmd.file); if (p) { replayer.stop(); void replayer.run(p, basename(p, '.jsonl'), cmd.speed).then(status); status(); } }
        else if (cmd.type === 'stop') replayer.stop();
      });
      ws.on('close', () => uiClients.delete(ws));
      return;
    }
    // Unreal bridge
    if (unreal) { log('bridge: replacing previous Unreal connection'); unreal.close(); }
    unreal = ws;
    log(`bridge: Unreal connected from ${req.socket.remoteAddress}`);
    status();
    // Messages are handled strictly in order: a session must be fully started (DB row created,
    // snapshot sent) before its first scan is processed.
    let chain: Promise<void> = Promise.resolve();
    ws.on('message', (raw) => {
      const msg = tryParseLine(raw.toString());
      if (!msg) { log(`bridge: unparseable frame: ${raw.toString().slice(0, 100)}`); return; }
      chain = chain.then(async () => {
        if (msg.type === 'session') { if (replayer.running) replayer.stop(); await pipeline.startSession(msg, 'live', null); }
        else if (msg.type === 'scan') pipeline.handleScan(msg);
        else if (msg.type === 'event') pipeline.handleEvent(msg);
      }).catch((e) => log(`bridge: ${(e as Error).message}`));
    });
    ws.on('error', (e) => log(`bridge: socket error: ${e.message}`));
    ws.on('close', (code, reason) => {
      if (unreal === ws) unreal = null;
      log(`bridge: Unreal disconnected (code ${code}${reason.length ? ` ${reason.toString()}` : ''})`);
      void pipeline.finish();
      status();
    });
  });

  await new Promise<void>((ok) => http.listen(cfg.port, ok));
  const addr = http.address();
  const port = typeof addr === 'object' && addr ? addr.port : cfg.port;
  log(`server: ws://localhost:${port}/ (Unreal)  ws://localhost:${port}/ui (dashboard)  http://localhost:${port}/api/health`);

  return {
    port,
    async close() {
      replayer.stop();
      for (const c of uiClients) c.close();
      unreal?.close();
      wss.close();
      await new Promise<void>((ok) => http.close(() => ok()));
      await db.close();
    },
  };
}
