'use client';
// KPI tiles per estimator, in payroll language. The selected estimator's column is highlighted.
import { Box, Paper, Table, TableBody, TableCell, TableHead, TableRow, Tooltip, Typography } from '@mui/material';
import { useTheme } from '@mui/material/styles';
import type { EstimatorName, Metrics } from '@sim/estimator';
import { ESTIMATOR_COLOR } from '@/lib/palette';

const ROWS: { key: keyof Metrics; label: string; help: string; fmt: (v: number | null) => string; better: 'high' | 'low' }[] = [
  { key: 'floorAccuracyOffHoist', label: 'Floor accuracy (stationary)', help: 'Share of phone reports where the estimated floor equals the true floor, excluding hoist rides.', fmt: (v) => (v === null ? '–' : `${(v * 100).toFixed(1)}%`), better: 'high' },
  { key: 'punches', label: 'Punches', help: 'Clock-in / clock-out events emitted this session. Thousands means the log is unusable.', fmt: (v) => (v === null ? '–' : String(Math.round(v))), better: 'low' },
  { key: 'spuriousPerHoistRide', label: 'Spurious punches per hoist ride', help: 'Punches fired during or just after a hoist ride to a floor other than the destination, per ride.', fmt: (v) => (v === null ? '–' : v.toFixed(2)), better: 'low' },
  { key: 'punchAccuracy', label: 'Clock-in on the right floor', help: 'Share of clock-ins whose floor was the true floor at that moment.', fmt: (v) => (v === null ? '–' : `${(v * 100).toFixed(0)}%`), better: 'high' },
  { key: 'timeToCorrectSec', label: 'Settle time after a ride', help: 'Median seconds from stepping off the hoist to a correct floor estimate.', fmt: (v) => (v === null ? '–' : `${v.toFixed(0)} s`), better: 'low' },
  { key: 'lobbyMinutesCredited', label: 'Lobby minutes billed to a work floor', help: 'Minutes credited to a work floor while the worker was actually in the lobby. The "clocked in from the lobby" number.', fmt: (v) => (v === null ? '–' : `${v.toFixed(1)} min`), better: 'low' },
  { key: 'misattributedDollars', label: 'Payroll misattributed', help: 'Minutes billed to the wrong floor × hourly rate ($45/h, hoist rides excluded).', fmt: (v) => (v === null ? '–' : `$${v.toFixed(2)}`), better: 'low' },
];

export function MetricsTiles({ metrics, estimators, selected }: { metrics: Partial<Record<EstimatorName, Metrics>>; estimators: EstimatorName[]; selected: EstimatorName }) {
  const dark = useTheme().palette.mode === 'dark';
  return (
    <Paper sx={{ p: 1.5, overflowX: 'auto' }}>
      <Typography variant="h6" sx={{ mb: 0.5 }}>Scorecard</Typography>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell />
            {estimators.map((e) => (
              <TableCell key={e} align="right" sx={{ fontWeight: 700, bgcolor: e === selected ? 'action.selected' : undefined }}>
                <Box component="span" sx={{ display: 'inline-block', width: 8, height: 8, borderRadius: '50%', bgcolor: ESTIMATOR_COLOR[e][dark ? 'dark' : 'light'], mr: 0.75 }} />
                {ESTIMATOR_COLOR[e].label}
              </TableCell>
            ))}
          </TableRow>
        </TableHead>
        <TableBody>
          {ROWS.map((row) => {
            const vals = estimators.map((e) => (metrics[e]?.[row.key] as number | null | undefined) ?? null);
            const defined = vals.filter((v): v is number => v !== null);
            const best = defined.length ? (row.better === 'high' ? Math.max(...defined) : Math.min(...defined)) : null;
            return (
              <TableRow key={row.key}>
                <TableCell><Tooltip title={row.help}><span>{row.label}</span></Tooltip></TableCell>
                {estimators.map((e, i) => (
                  <TableCell key={e} align="right" sx={{ fontVariantNumeric: 'tabular-nums', fontWeight: vals[i] !== null && vals[i] === best ? 700 : 400, bgcolor: e === selected ? 'action.selected' : undefined }}>
                    {row.fmt(vals[i]!)}
                  </TableCell>
                ))}
              </TableRow>
            );
          })}
        </TableBody>
      </Table>
    </Paper>
  );
}
