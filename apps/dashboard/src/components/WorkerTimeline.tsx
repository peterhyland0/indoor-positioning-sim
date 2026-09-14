'use client';
// One strip per worker: floor over time. Truth = thin neutral line, the selected estimator = 2px
// series line, hoist rides shaded, punches as ticks at the baseline. Hover shows a crosshair readout.
import { useMemo, useState } from 'react';
import { Box, Paper, Stack, Typography } from '@mui/material';
import { useTheme } from '@mui/material/styles';
import type { EstimatorName } from '@sim/estimator';
import type { DashboardState, TimelineSample } from '@/lib/state';
import { ESTIMATOR_COLOR, HOIST_SHADE, STATUS, TRUTH } from '@/lib/palette';

const W = 720, H = 64, PAD_L = 34, PAD_R = 8, PAD_T = 6, PAD_B = 14;

function path(samples: TimelineSample[], pick: (s: TimelineSample) => number | null, x: (t: number) => number, y: (f: number) => number): string {
  let d = '';
  let pen = false;
  for (const s of samples) {
    const v = pick(s);
    if (v === null) { pen = false; continue; }
    d += `${pen ? 'H' : 'M'}${x(s.t).toFixed(1)} ${pen ? `V${y(v).toFixed(1)}` : y(v).toFixed(1)}`;
    pen = true;
  }
  return d;
}

export function WorkerTimeline({ state, estimator, showTruth, windowSec = 900 }: { state: DashboardState; estimator: EstimatorName; showTruth: boolean; windowSec?: number }) {
  const theme = useTheme();
  const dark = theme.palette.mode === 'dark';
  const numFloors = state.session?.numFloors ?? 15;
  const tEnd = Math.max(state.t, 60);
  const tStart = Math.max(0, tEnd - windowSec);
  const x = (t: number) => PAD_L + ((t - tStart) / (tEnd - tStart)) * (W - PAD_L - PAD_R);
  const y = (f: number) => PAD_T + (1 - f / numFloors) * (H - PAD_T - PAD_B);
  const [hover, setHover] = useState<number | null>(null);
  const series = ESTIMATOR_COLOR[estimator][dark ? 'dark' : 'light'];
  const truthColor = dark ? TRUTH.dark : TRUTH.light;

  const workers = useMemo(() => state.workers.map((w) => w.id).sort(), [state.workers]);

  return (
    <Paper sx={{ p: 1.5 }}>
      <Stack direction="row" sx={{mb: 0.5, alignItems: 'baseline', justifyContent: 'space-between'}}>
        <Typography variant="h6">Floor over time</Typography>
        <Stack direction="row" spacing={2} sx={{ fontSize: 12, color: 'text.secondary' }}>
          <span><Box component="span" sx={{ display: 'inline-block', width: 14, height: 2, bgcolor: series, verticalAlign: 'middle', mr: 0.5 }} />{ESTIMATOR_COLOR[estimator].label} estimate</span>
          {showTruth && <span><Box component="span" sx={{ display: 'inline-block', width: 14, height: 1, bgcolor: truthColor, verticalAlign: 'middle', mr: 0.5 }} />truth</span>}
          {showTruth && <span><Box component="span" sx={{ display: 'inline-block', width: 14, height: 8, bgcolor: dark ? HOIST_SHADE.dark : HOIST_SHADE.light, verticalAlign: 'middle', mr: 0.5 }} />hoist ride</span>}
          <span><Box component="span" sx={{ display: 'inline-block', width: 2, height: 8, bgcolor: STATUS.good, verticalAlign: 'middle', mr: 0.5 }} />clock in <Box component="span" sx={{ display: 'inline-block', width: 2, height: 8, bgcolor: STATUS.critical, verticalAlign: 'middle', mx: 0.5 }} />out</span>
        </Stack>
      </Stack>
      {workers.map((id) => {
        const samples = (state.timeline[id] ?? []).filter((s) => s.t >= tStart);
        const punches = state.punches.filter((p) => p.estimator === estimator && p.punch.worker === id && p.punch.t >= tStart);
        const hoverSample = hover !== null ? samples.reduce<TimelineSample | null>((best, s) => (s.t <= hover && (!best || s.t > best.t) ? s : best), null) : null;
        // hoist ride shading from truth
        const rides: { a: number; b: number }[] = [];
        let start: number | null = null;
        for (const s of samples) {
          if (s.onHoist && start === null) start = s.t;
          if (!s.onHoist && start !== null) { rides.push({ a: start, b: s.t }); start = null; }
        }
        if (start !== null) rides.push({ a: start, b: tEnd });
        return (
          <Box key={id} sx={{ display: 'flex', alignItems: 'center', gap: 1, overflowX: 'auto' }}>
            <Typography variant="caption" sx={{ width: 32, fontWeight: 700 }}>{id}</Typography>
            <svg viewBox={`0 0 ${W} ${H}`} width="100%" preserveAspectRatio="none" style={{ display: 'block', height: H }} role="img" aria-label={`${id} floor over time`}
              onMouseMove={(e) => { const r = e.currentTarget.getBoundingClientRect(); const px = ((e.clientX - r.left) / r.width) * W; setHover(tStart + ((px - PAD_L) / (W - PAD_L - PAD_R)) * (tEnd - tStart)); }}
              onMouseLeave={() => setHover(null)}>
              {[0, Math.round(numFloors / 2), numFloors].map((f) => (
                <g key={f}>
                  <line x1={PAD_L} x2={W - PAD_R} y1={y(f)} y2={y(f)} stroke={theme.palette.divider} strokeWidth={1} />
                  <text x={PAD_L - 4} y={y(f) + 3} fontSize={9} textAnchor="end" fill={theme.palette.text.secondary}>{f === 0 ? 'L' : `F${f}`}</text>
                </g>
              ))}
              {showTruth && rides.map((r, i) => <rect key={i} x={x(r.a)} y={PAD_T} width={Math.max(1, x(r.b) - x(r.a))} height={H - PAD_T - PAD_B} fill={dark ? HOIST_SHADE.dark : HOIST_SHADE.light} />)}
              {showTruth && <path d={path(samples, (s) => s.truth, x, y)} fill="none" stroke={truthColor} strokeWidth={1} />}
              <path d={path(samples, (s) => s.est[estimator] ?? null, x, y)} fill="none" stroke={series} strokeWidth={2} strokeLinejoin="round" strokeLinecap="round" />
              {punches.map((p, i) => <line key={i} x1={x(p.punch.t)} x2={x(p.punch.t)} y1={H - PAD_B + 2} y2={H - 2} stroke={p.punch.kind === 'in' ? STATUS.good : STATUS.critical} strokeWidth={2} />)}
              {hover !== null && hover >= tStart && hover <= tEnd && (
                <g>
                  <line x1={x(hover)} x2={x(hover)} y1={PAD_T} y2={H - PAD_B} stroke={theme.palette.text.secondary} strokeWidth={1} strokeDasharray="2 2" />
                  {hoverSample && (
                    <text x={Math.min(x(hover) + 4, W - 120)} y={PAD_T + 9} fontSize={9} fill={theme.palette.text.primary}>
                      {`${Math.floor(hover / 60)}:${String(Math.floor(hover % 60)).padStart(2, '0')}  est ${hoverSample.est[estimator] === null || hoverSample.est[estimator] === undefined ? '–' : `F${hoverSample.est[estimator]}`}${showTruth ? `  truth F${hoverSample.truth}${hoverSample.onHoist ? ' (hoist)' : ''}` : ''}`}
                    </text>
                  )}
                </g>
              )}
            </svg>
          </Box>
        );
      })}
      {workers.length === 0 && <Typography variant="caption" color="text.secondary">Waiting for workers…</Typography>}
    </Paper>
  );
}
