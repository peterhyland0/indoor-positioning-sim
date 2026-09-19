'use client';
// History from Postgres via the server: every stored session with its per-estimator metrics.
import { useEffect, useState } from 'react';
import { Alert, Paper, Table, TableBody, TableCell, TableHead, TableRow, Typography } from '@mui/material';
import type { Metrics } from '@sim/estimator';
import { SERVER_URL } from '@/lib/feed';
import { ESTIMATOR_COLOR } from '@/lib/palette';

interface Row { id: number; seed: number; source: string; label: string | null; started_at: string; num_floors: number; scans: number; metrics: Record<string, Metrics> }
const EST = ['gps', 'nearest', 'smoothed', 'fused'] as const;

export default function SessionsPage() {
  const [rows, setRows] = useState<Row[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  useEffect(() => {
    void fetch(`${SERVER_URL}/api/sessions`)
      .then(async (r) => { if (!r.ok) throw new Error(`server returned ${r.status}`); return (await r.json()) as Row[]; })
      .then(setRows)
      .catch((e: Error) => setError(e.message));
  }, []);
  if (error) return <Alert severity="warning">Could not reach the server at <code>{SERVER_URL}</code> ({error}). History needs <code>npm run server</code> with <code>DATABASE_URL</code> set.</Alert>;
  if (!rows) return <Typography color="text.secondary">Loading…</Typography>;
  if (rows.length === 0) return <Alert severity="info">No stored sessions yet. Run the server with <code>DATABASE_URL</code> set and play the sim or replay a recording.</Alert>;
  return (
    <Paper sx={{ p: 1.5, overflowX: 'auto' }}>
      <Typography variant="h6" sx={{ mb: 1 }}>Stored sessions</Typography>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>#</TableCell><TableCell>Started</TableCell><TableCell>Source</TableCell><TableCell>Label</TableCell><TableCell align="right">Scans</TableCell>
            {EST.map((e) => <TableCell key={e} align="right">{ESTIMATOR_COLOR[e].label}<br /><Typography variant="caption" color="text.secondary">acc · spurious/ride · $ misattr</Typography></TableCell>)}
          </TableRow>
        </TableHead>
        <TableBody>
          {rows.map((r) => (
            <TableRow key={r.id}>
              <TableCell>{r.id}</TableCell>
              <TableCell>{new Date(r.started_at).toLocaleString()}</TableCell>
              <TableCell>{r.source}</TableCell>
              <TableCell>{r.label ?? `seed ${r.seed}`}</TableCell>
              <TableCell align="right">{r.scans}</TableCell>
              {EST.map((e) => {
                const m = r.metrics[e];
                return <TableCell key={e} align="right" sx={{ fontVariantNumeric: 'tabular-nums' }}>{m ? `${(m.floorAccuracyOffHoist * 100).toFixed(0)}% · ${m.spuriousPerHoistRide.toFixed(2)} · $${m.misattributedDollars.toFixed(2)}` : '–'}</TableCell>;
              })}
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </Paper>
  );
}
