// Replays a recorded JSONL session into the estimator socket, pacing by `t`.
// Lets the TS side be developed and demoed without Unreal running.
//
//   node tools/replay-client.mjs sessions/honest.jsonl [ws://localhost:8080] [speed=1]
import { createReadStream } from 'node:fs';
import { createInterface } from 'node:readline';

const [file, url = 'ws://localhost:8080', speedArg = '1'] = process.argv.slice(2);
if (!file) {
  console.error('usage: node tools/replay-client.mjs <session.jsonl> [ws-url] [speed]');
  process.exit(1);
}
const speed = Number(speedArg);
const ws = new WebSocket(url); // Node >= 22 has a global WebSocket client
ws.addEventListener('message', (e) => console.log('<-', e.data));
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });

const rl = createInterface({ input: createReadStream(file) });
let lastT = null;
for await (const line of rl) {
  if (!line.trim()) continue;
  const msg = JSON.parse(line);
  if (typeof msg.t === 'number' && lastT !== null && speed > 0) {
    await new Promise((r) => setTimeout(r, Math.max(0, (msg.t - lastT) * 1000 / speed)));
  }
  if (typeof msg.t === 'number') lastT = msg.t;
  ws.send(line);
}
console.log('replay finished');
setTimeout(() => ws.close(), 500);
