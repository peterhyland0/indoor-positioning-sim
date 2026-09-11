// Minimal stand-in for the estimator side. Prints every message Unreal sends
// and replies with a fake estimate (the ground-truth floor) so the two-way
// bridge and the in-viewport labels can be tested before the real TS side exists.
//
// Accepts both transports the Unreal bridge may use:
//   - WebSocket  ws://localhost:8080          (primary; estimate is sent back on the socket)
//   - HTTP POST  http://localhost:8080/ingest (fallback; estimate is the JSON response body)
//
//   npm run echo
//   PORT=9000 npm run echo
import { createServer } from 'node:http';
import { WebSocketServer } from 'ws';

const port = Number(process.env.PORT ?? 8080);
let count = 0;

function handle(msg) {
  count++;
  switch (msg.type) {
    case 'session':
      console.log(`session seed=${msg.seed} floors=${msg.numFloors} beacons=${msg.beacons?.length} workers=${msg.workers?.length}`);
      return null;
    case 'scan': {
      const heard = (msg.scans ?? []).map((s) => `${s.b}:${s.rssi}`).join(' ');
      const region = (msg.regionEvents ?? []).map((e) => `${e.b}:${e.event}`).join(' ');
      console.log(`t=${Number(msg.t).toFixed(1)} ${msg.worker} truth=${msg.truth?.floor}${msg.truth?.onHoist ? '(hoist)' : ''} p=${msg.pressure} [${heard}${region ? ' region: ' + region : ''}]`);
      // Fake estimate: echo the truth so labels go green. Replace with the real estimator.
      return { v: 1, type: 'estimate', worker: msg.worker, estFloor: msg.truth?.floor ?? null, punch: null };
    }
    case 'event':
      console.log(`event ${msg.name} ${JSON.stringify(msg.payload ?? {})}`);
      return null;
    default:
      console.log('unknown type', msg.type);
      return null;
  }
}

function parse(raw) {
  try {
    return JSON.parse(raw);
  } catch {
    console.warn('non-JSON message:', String(raw).slice(0, 120));
    return null;
  }
}

const http = createServer((req, res) => {
  if (req.method === 'POST' && req.url === '/ingest') {
    let body = '';
    req.on('data', (c) => (body += c));
    req.on('end', () => {
      const msg = parse(body);
      const reply = msg ? handle(msg) : null;
      res.writeHead(msg ? 200 : 400, { 'content-type': 'application/json' });
      res.end(JSON.stringify(reply ?? { ok: Boolean(msg) }));
    });
    return;
  }
  res.writeHead(404).end();
});

const wss = new WebSocketServer({ server: http });
wss.on('connection', (ws, req) => {
  console.log(`ws client connected from ${req.socket.remoteAddress}`);
  ws.on('message', (raw) => {
    const msg = parse(raw.toString());
    if (!msg) return;
    const reply = handle(msg);
    if (reply) ws.send(JSON.stringify(reply));
  });
  ws.on('close', () => console.log(`ws client disconnected (${count} messages so far)`));
});

http.listen(port, () => console.log(`echo-server: ws://localhost:${port}  and  POST http://localhost:${port}/ingest`));
