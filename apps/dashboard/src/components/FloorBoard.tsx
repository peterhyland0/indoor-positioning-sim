'use client';
// One row per floor, top to bottom. A worker chip sits on the floor the selected estimator says;
// red with a ✗ when that disagrees with ground truth (sim only). Beacons shown as dots per floor.
import { Box, Chip, Paper, Stack, Switch, FormControlLabel, Tooltip, Typography } from '@mui/material';
import type { EstimatorName } from '@sim/estimator';
import type { DashboardState } from '@/lib/state';
import { STATUS } from '@/lib/palette';

export function FloorBoard({ state, estimator, showTruth, onShowTruth }: { state: DashboardState; estimator: EstimatorName; showTruth: boolean; onShowTruth: (v: boolean) => void }) {
  const numFloors = state.session?.numFloors ?? 15;
  const floors = Array.from({ length: numFloors + 1 }, (_, i) => numFloors - i);
  const byFloor = new Map<number | null, typeof state.workers>();
  for (const w of state.workers) {
    const f = w.estimates[estimator]?.floor ?? null;
    byFloor.set(f, [...(byFloor.get(f) ?? []), w]);
  }
  const beaconsByFloor = new Map<number, string[]>();
  for (const b of state.session?.beacons ?? []) beaconsByFloor.set(b.floor, [...(beaconsByFloor.get(b.floor) ?? []), b.id]);
  const unplaced = byFloor.get(null) ?? [];

  return (
    <Paper sx={{ p: 1.5 }}>
      <Stack direction="row" sx={{mb: 1, alignItems: 'center', justifyContent: 'space-between'}}>
        <Typography variant="h6">Who is on which floor</Typography>
        <FormControlLabel control={<Switch size="small" checked={showTruth} onChange={(_, v) => onShowTruth(v)} />} label={<Typography variant="caption">show ground truth</Typography>} />
      </Stack>
      <Box sx={{ display: 'grid', gridTemplateColumns: '52px 1fr 60px', rowGap: '2px', alignItems: 'center' }}>
        {floors.map((f) => {
          const ws = byFloor.get(f) ?? [];
          return (
            <Box key={f} sx={{ display: 'contents' }}>
              <Typography variant="body2" sx={{ fontWeight: 700, color: 'text.secondary' }}>{f === 0 ? 'Lobby' : `F${f}`}</Typography>
              <Box sx={{ minHeight: 26, display: 'flex', flexWrap: 'wrap', gap: 0.5, alignItems: 'center', borderBottom: '1px solid', borderColor: 'divider', py: 0.25 }}>
                {ws.map((w) => {
                  const e = w.estimates[estimator]!;
                  const wrong = showTruth && !w.onHoist && e.floor !== w.truthFloor; // a ride has no 'right' floor
                  const label = `${w.id}${e.punchedFloor !== null ? ' ·in' : ''}${w.onHoist && showTruth ? ' ·hoist' : ''}${wrong ? ` ✗ F${w.truthFloor}` : ''}`;
                  return (
                    <Tooltip key={w.id} title={`${w.id} · ${w.platform} · ${e.state} · confidence ${(e.confidence * 100).toFixed(0)}%${showTruth ? ` · truly on F${w.truthFloor}` : ''}`}>
                      <Chip size="small" label={label} sx={{ bgcolor: wrong ? STATUS.critical : e.state === 'transit' ? 'action.selected' : 'primary.main', color: wrong || e.state !== 'transit' ? '#fff' : 'text.primary' }} />
                    </Tooltip>
                  );
                })}
              </Box>
              <Box sx={{ display: 'flex', gap: 0.5, justifyContent: 'flex-end' }}>
                {(beaconsByFloor.get(f) ?? []).map((id) => {
                  const st = state.beaconState[id];
                  const color = !st || st.alive === false ? STATUS.critical : st.battery < 0.2 ? STATUS.warning : STATUS.good;
                  return (
                    <Tooltip key={id} title={`${id}: ${!st || st.alive === false ? 'dead' : st.battery < 0.2 ? 'low battery' : 'alive'}`}>
                      <Box sx={{ width: 8, height: 8, borderRadius: '50%', bgcolor: color }} />
                    </Tooltip>
                  );
                })}
              </Box>
            </Box>
          );
        })}
      </Box>
      {unplaced.length > 0 && (
        <Typography variant="caption" color="text.secondary" sx={{ mt: 1, display: 'block' }}>No floor yet: {unplaced.map((w) => w.id).join(', ')}</Typography>
      )}
    </Paper>
  );
}
