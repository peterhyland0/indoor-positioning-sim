'use client';
import { Chip, Paper, Table, TableBody, TableCell, TableHead, TableRow, Typography } from '@mui/material';
import type { EstimatorName } from '@sim/estimator';
import type { DashboardState } from '@/lib/state';
import { STATUS } from '@/lib/palette';

const fmt = (t: number) => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, '0')}`;

export function PunchLog({ state, estimator, showTruth, limit = 12 }: { state: DashboardState; estimator: EstimatorName; showTruth: boolean; limit?: number }) {
  const rows = state.punches.filter((p) => p.estimator === estimator).slice(-limit).reverse();
  const total = state.punches.filter((p) => p.estimator === estimator).length;
  return (
    <Paper sx={{ p: 1.5 }}>
      <Typography variant="h6" sx={{ mb: 0.5 }}>Punch log <Typography component="span" variant="caption" color="text.secondary">({total} this session)</Typography></Typography>
      <Table size="small">
        <TableHead>
          <TableRow><TableCell>Time</TableCell><TableCell>Worker</TableCell><TableCell>Punch</TableCell><TableCell>Floor</TableCell><TableCell>Status</TableCell></TableRow>
        </TableHead>
        <TableBody>
          {rows.map((r, i) => {
            const wrong = showTruth && r.punch.kind === 'in' && r.punch.floor !== r.truthFloor;
            return (
              <TableRow key={`${r.punch.t}-${r.punch.worker}-${i}`}>
                <TableCell sx={{ fontVariantNumeric: 'tabular-nums' }}>{fmt(r.punch.t)}</TableCell>
                <TableCell>{r.punch.worker}</TableCell>
                <TableCell>{r.punch.kind === 'in' ? 'Clock in' : 'Clock out'}</TableCell>
                <TableCell>{r.punch.floor === 0 ? 'Lobby' : `F${r.punch.floor}`}</TableCell>
                <TableCell>
                  {wrong ? <Chip size="small" label={`✗ was on F${r.truthFloor}`} sx={{ bgcolor: STATUS.critical, color: '#fff' }} />
                    : r.punch.floor === 0 && r.punch.kind === 'in' ? <Chip size="small" label="⚠ lobby" sx={{ bgcolor: STATUS.warning }} />
                    : r.punch.verified ? <Chip size="small" label="✓ verified" sx={{ bgcolor: STATUS.good, color: '#fff' }} />
                    : <Chip size="small" label="unverified" variant="outlined" />}
                </TableCell>
              </TableRow>
            );
          })}
          {rows.length === 0 && <TableRow><TableCell colSpan={5}><Typography variant="caption" color="text.secondary">No punches yet</Typography></TableCell></TableRow>}
        </TableBody>
      </Table>
    </Paper>
  );
}
