// Read a recorded JSONL session and run an estimator + punch engine over it, producing per-tick results
// and metrics. Also used incrementally by the live server (see `SessionRunner`).
import { parseLine, toObservation, toSessionInfo, type EventMsg, type ScanMsg, type SessionInfo } from '@sim/protocol';
import { DEFAULT_PUNCH, PunchEngine, type Punch, type PunchParams } from './punch.js';
import { DEFAULT_SCORE, score, type Metrics, type ScoreOptions, type Tick } from './score.js';
import type { FloorEstimator } from './types.js';

export interface Session {
  info: SessionInfo;
  scans: ScanMsg[];
  events: EventMsg[];
}

export function parseSession(text: string): Session {
  let info: SessionInfo | null = null;
  const scans: ScanMsg[] = [];
  const events: EventMsg[] = [];
  for (const line of text.split('\n')) {
    if (!line.trim()) continue;
    const msg = parseLine(line);
    if (msg.type === 'session') info = toSessionInfo(msg);
    else if (msg.type === 'scan') scans.push(msg);
    else if (msg.type === 'event') events.push(msg);
  }
  if (!info) throw new Error('no session message in recording');
  scans.sort((a, b) => a.t - b.t);
  return { info, scans, events };
}

export interface RunResult {
  estimator: string;
  ticks: Tick[];
  punches: Punch[];
  metrics: Metrics;
}

/** Incremental runner: feed scans one at a time (live) or all at once (replay). */
export class SessionRunner {
  readonly ticks: Tick[] = [];
  readonly punches: Punch[] = [];
  private readonly punchEngine: PunchEngine;

  constructor(
    readonly estimator: FloorEstimator,
    info: SessionInfo,
    punchParams: PunchParams = DEFAULT_PUNCH,
    private readonly scoreOpts: ScoreOptions = DEFAULT_SCORE,
  ) {
    estimator.reset(info);
    this.punchEngine = new PunchEngine(punchParams);
  }

  /** Returns this tick and any punches it produced. */
  step(scan: ScanMsg): { tick: Tick; punches: Punch[] } {
    const est = this.estimator.update(toObservation(scan));
    const punches = this.punchEngine.update(scan.t, scan.worker, est);
    const tick: Tick = { scan, est, punchedFloor: this.punchEngine.punchedFloor(scan.worker) };
    this.ticks.push(tick);
    this.punches.push(...punches);
    return { tick, punches };
  }

  metrics(): Metrics {
    return score(this.estimator.name, this.ticks, this.punches, this.scoreOpts);
  }
}

export function runSession(
  session: Session,
  estimator: FloorEstimator,
  punchParams: PunchParams = DEFAULT_PUNCH,
  scoreOpts: ScoreOptions = DEFAULT_SCORE,
): RunResult {
  const runner = new SessionRunner(estimator, session.info, punchParams, scoreOpts);
  for (const scan of session.scans) runner.step(scan);
  return { estimator: estimator.name, ticks: runner.ticks, punches: runner.punches, metrics: runner.metrics() };
}
