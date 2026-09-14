// Copy the canonical recordings into public/ so the replay page works with no server (and on Vercel).
import { copyFileSync, mkdirSync, readdirSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
const src = resolve(import.meta.dirname, '../../../sessions');
const dst = resolve(import.meta.dirname, '../public/sessions');
mkdirSync(dst, { recursive: true });
const files = readdirSync(src).filter((f) => f.endsWith('.jsonl') && !f.startsWith('sample'));
for (const f of files) copyFileSync(join(src, f), join(dst, f));
writeFileSync(join(dst, 'index.json'), JSON.stringify(files, null, 2));
console.log(`copied ${files.length} sessions to public/sessions`);
