'use client';
// Buttons that drive the Unreal sim through the server. Same verbs as the in-game panel.
import { useState } from 'react';
import { Button, MenuItem, Paper, Select, Stack, Typography } from '@mui/material';
import type { DashboardState } from '@/lib/state';

export function SabotageBar({ state, send }: { state: DashboardState; send: (cmd: string) => void }) {
  const beacons = (state.session?.beacons ?? []).map((b) => b.id);
  const workers = state.workers.map((w) => w.id);
  const [beacon, setBeacon] = useState('F12-A');
  const [worker, setWorker] = useState('w01');
  const disabled = !state.unrealConnected;
  return (
    <Paper sx={{ p: 1.5 }}>
      <Typography variant="h6" sx={{ mb: 1 }}>Sabotage the site {disabled && <Typography component="span" variant="caption" color="text.secondary">(needs the Unreal sim connected)</Typography>}</Typography>
      <Stack direction="row" spacing={1} useFlexGap sx={{ alignItems: 'center', flexWrap: 'wrap' }}>
        <Select size="small" value={beacons.includes(beacon) ? beacon : (beacons[0] ?? '')} onChange={(e) => setBeacon(e.target.value)} sx={{ minWidth: 100 }}>
          {beacons.map((b) => <MenuItem key={b} value={b}>{b}</MenuItem>)}
        </Select>
        <Button size="small" variant="outlined" disabled={disabled} onClick={() => send(`killbeacon ${beacon}`)}>Kill beacon</Button>
        <Button size="small" variant="outlined" disabled={disabled} onClick={() => send(`revivebeacon ${beacon}`)}>Revive</Button>
        <Button size="small" variant="outlined" disabled={disabled} onClick={() => send(`nudge ${beacon} 5`)}>Move 5 m</Button>
        <Select size="small" value={workers.includes(worker) ? worker : (workers[0] ?? '')} onChange={(e) => setWorker(e.target.value)} sx={{ minWidth: 80, ml: 2 }}>
          {workers.map((w) => <MenuItem key={w} value={w}>{w}</MenuItem>)}
        </Select>
        <Button size="small" variant="outlined" disabled={disabled} onClick={() => send(`pocket ${worker} on`)}>Phone in pocket</Button>
        <Button size="small" variant="outlined" disabled={disabled} onClick={() => send(`background ${worker} on`)}>App to background</Button>
        <Button size="small" variant="outlined" disabled={disabled} sx={{ ml: 2 }} onClick={() => send('storm on')}>Storm</Button>
        <Button size="small" variant="outlined" disabled={disabled} onClick={() => send('slam 12')}>Door slam F12</Button>
      </Stack>
    </Paper>
  );
}
