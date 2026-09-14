#!/usr/bin/env tsx
// estimator score <session.jsonl...> [--estimator nearest,smoothed,fused] [--rate 45] [--json]
import { readFileSync } from 'node:fs';
import { basename } from 'node:path';
import { parseSession, runSession } from './session';
import { createEstimator, ESTIMATOR_NAMES, type EstimatorName } from './registry';
import { DEFAULT_SCORE, type Metrics } from './score';

const args = process.argv.slice(2);
const cmd = args.shift();
if (cmd !== 'score') {
  console.error('usage: estimator score <session.jsonl...> [--estimator a,b] [--rate 45] [--json]');
  process.exit(2);
}
let names: EstimatorName[] = [...ESTIMATOR_NAMES];
let rate = DEFAULT_SCORE.hourlyRate;
let json = false;
const files: string[] = [];
for (let i = 0; i < args.length; i++) {
  const a = args[i]!;
  if (a === '--estimator') names = args[++i]!.split(',') as EstimatorName[];
  else if (a === '--rate') rate = Number(args[++i]);
  else if (a === '--json') json = true;
  else files.push(a);
}
if (files.length === 0) {
  console.error('no session files given');
  process.exit(2);
}

const rows: (Metrics & { session: string })[] = [];
for (const file of files) {
  const session = parseSession(readFileSync(file, 'utf8'));
  for (const name of names) {
    const { estimator, punchParams } = createEstimator(name);
    const r = runSession(session, estimator, punchParams, { ...DEFAULT_SCORE, hourlyRate: rate });
    rows.push({ session: basename(file, '.jsonl'), ...r.metrics });
  }
}

if (json) {
  console.log(JSON.stringify(rows, null, 2));
} else {
  const pct = (x: number) => `${(x * 100).toFixed(1)}%`;
  const num = (x: number | null, d = 1) => (x === null ? '-' : x.toFixed(d));
  const cols = ['session', 'estimator', 'floor acc', 'off-hoist', 'on-hoist', 'punches', 'punch acc', 'rides', 'spurious/ride', 'to-correct s', 'misattr min', 'uncredited min', 'lobby min', '$ misattr'];
  const table = rows.map((r) => [
    r.session, r.estimator, pct(r.floorAccuracy), pct(r.floorAccuracyOffHoist), pct(r.floorAccuracyOnHoist), String(r.punches),
    pct(r.punchAccuracy), String(r.hoistRides), num(r.spuriousPerHoistRide, 2), num(r.timeToCorrectSec), num(r.misattributedMinutes),
    num(r.uncreditedMinutes), num(r.lobbyMinutesCredited), `$${r.misattributedDollars.toFixed(2)}`,
  ]);
  const widths = cols.map((c, i) => Math.max(c.length, ...table.map((row) => row[i]!.length)));
  const line = (row: string[]) => row.map((c, i) => c.padEnd(widths[i]!)).join('  ');
  console.log(line(cols));
  console.log(widths.map((w) => '-'.repeat(w)).join('  '));
  for (const row of table) console.log(line(row));
}
