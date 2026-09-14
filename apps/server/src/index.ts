import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { loadConfig } from './config.js';
import { startServer } from './server.js';

// Minimal .env loader (no dependency): KEY=value lines in apps/server/.env
try {
  for (const line of readFileSync(resolve(import.meta.dirname, '../.env'), 'utf8').split('\n')) {
    const m = /^\s*([A-Z_]+)\s*=\s*(.*?)\s*$/.exec(line);
    if (m && !(m[1]! in process.env)) process.env[m[1]!] = m[2]!;
  }
} catch { /* no .env */ }

const server = await startServer(loadConfig());
const shutdown = () => { void server.close().then(() => process.exit(0)); };
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
