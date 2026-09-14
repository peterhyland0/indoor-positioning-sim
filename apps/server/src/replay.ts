// Feed a recorded JSONL session through the pipeline, paced by its `t` values (speed 0 = as fast as possible).
import { readFile } from 'node:fs/promises';
import { parseLine } from '@sim/protocol';
import type { Pipeline } from './pipeline.js';

export class Replayer {
  private stopped = false;
  running = false;

  constructor(private readonly pipeline: Pipeline) {}

  stop(): void {
    this.stopped = true;
  }

  async run(path: string, label: string, speed: number): Promise<{ scans: number }> {
    this.stopped = false;
    this.running = true;
    let scans = 0;
    try {
      const text = await readFile(path, 'utf8');
      const lines = text.split('\n').filter((l) => l.trim());
      let lastT: number | null = null;
      for (const line of lines) {
        if (this.stopped) break;
        const msg = parseLine(line);
        if (msg.type === 'session') {
          await this.pipeline.startSession(msg, 'replay', label);
          continue;
        }
        if (msg.type === 'estimate' || msg.type === 'command') continue;
        if (speed > 0 && lastT !== null && msg.t > lastT) {
          const waitMs = ((msg.t - lastT) * 1000) / speed;
          if (waitMs >= 1) await new Promise((r) => setTimeout(r, waitMs));
        }
        lastT = msg.t;
        if (msg.type === 'scan') { this.pipeline.handleScan(msg); scans++; }
        else this.pipeline.handleEvent(msg);
        // yield so the event loop can serve sockets during a fast replay
        if (speed === 0 && scans % 200 === 0) await new Promise((r) => setImmediate(r));
      }
      await this.pipeline.finish();
    } finally {
      this.running = false;
    }
    return { scans };
  }
}
