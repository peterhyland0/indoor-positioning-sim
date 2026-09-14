'use client';
// Runs entirely in the browser: loads a recording from /sessions, runs every estimator, scrubs.
import { useEffect, useMemo, useRef, useState } from 'react';
import { Alert, Box, Button, MenuItem, Select, Slider, Stack, ToggleButton, ToggleButtonGroup, Typography } from '@mui/material';
import { ReplayRunner } from '@/lib/replayRunner';
import { Board } from '@/components/Board';

const DESCRIPTIONS: Record<string, string> = {
  'honest.jsonl': 'Four honest workers: up the hoist, work, back down. Baseline.',
  'lobby-cheat.jsonl': 'Two workers clock in and loiter in the lobby for 10 minutes before going up. Watch "Lobby minutes billed" for Site GPS.',
  'beacon-failure.jsonl': 'F12-A runs low, dies at 5:00, F12-B is kicked 8 m at 7:00, F12-A is revived at 10:00. Fused holds; Nearest does not.',
  'phones-and-weather.jsonl': 'Pocket, app in background, a pressure storm, a door slam on F12, and two workers hopping floors by the stairs.',
};

const fmt = (s: number) => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, '0')}`;

export default function ReplayPage() {
  const [files, setFiles] = useState<string[]>([]);
  const [file, setFile] = useState<string>('lobby-cheat.jsonl');
  const [runner, setRunner] = useState<ReplayRunner | null>(null);
  const [loading, setLoading] = useState(false);
  const [t, setT] = useState(0);
  const [speed, setSpeed] = useState(10);
  const [playing, setPlaying] = useState(false);
  const raf = useRef<number | null>(null);

  useEffect(() => { void fetch('/sessions/index.json').then((r) => r.json()).then((f: string[]) => setFiles(f)).catch(() => {}); }, []);
  useEffect(() => {
    let cancelled = false;
    setLoading(true); setPlaying(false); setT(0);
    void fetch(`/sessions/${file}`).then((r) => r.text()).then((text) => {
      if (cancelled) return;
      setTimeout(() => { if (!cancelled) { setRunner(new ReplayRunner(text, file.replace('.jsonl', ''))); setLoading(false); } }, 0);
    });
    return () => { cancelled = true; };
  }, [file]);

  useEffect(() => {
    if (!playing || !runner) return;
    let last = performance.now();
    const step = (now: number) => {
      const dt = ((now - last) / 1000) * speed;
      last = now;
      setT((prev) => { const next = prev + dt; if (next >= runner.duration) { setPlaying(false); return runner.duration; } return next; });
      raf.current = requestAnimationFrame(step);
    };
    raf.current = requestAnimationFrame(step);
    return () => { if (raf.current !== null) cancelAnimationFrame(raf.current); };
  }, [playing, speed, runner]);

  const state = useMemo(() => (runner ? runner.at(t) : null), [runner, t]);

  return (
    <Stack spacing={1.5}>
      <Stack direction="row" spacing={2} useFlexGap sx={{ alignItems: 'center', flexWrap: 'wrap' }}>
        <Typography variant="subtitle2">Recording</Typography>
        <Select size="small" value={file} onChange={(e) => setFile(e.target.value)} sx={{ minWidth: 220 }}>
          {(files.length ? files : [file]).map((f) => <MenuItem key={f} value={f}>{f.replace('.jsonl', '')}</MenuItem>)}
        </Select>
        <Button size="small" variant="contained" disableElevation disabled={!runner} onClick={() => { if (runner && t >= runner.duration) setT(0); setPlaying((p) => !p); }}>{playing ? 'Pause' : 'Play'}</Button>
        <ToggleButtonGroup size="small" exclusive value={speed} onChange={(_, v: number | null) => v && setSpeed(v)}>
          {[1, 10, 50].map((s) => <ToggleButton key={s} value={s}>{s}×</ToggleButton>)}
        </ToggleButtonGroup>
        <Box sx={{ flex: 1, minWidth: 240, px: 1 }}>
          <Slider size="small" min={0} max={runner?.duration ?? 900} step={1} value={t} onChange={(_, v) => { setPlaying(false); setT(v as number); }} valueLabelDisplay="auto" valueLabelFormat={fmt} />
        </Box>
        <Typography variant="caption" sx={{ fontVariantNumeric: 'tabular-nums', minWidth: 90 }}>{fmt(t)} / {fmt(runner?.duration ?? 0)}</Typography>
      </Stack>
      <Alert severity="info" icon={false}>{DESCRIPTIONS[file] ?? 'Recorded session.'}{loading ? ' Loading…' : ''}</Alert>
      {state && <Board state={state} windowSec={900} />}
    </Stack>
  );
}
