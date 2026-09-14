'use client';
import { Alert, Chip, Stack, Typography } from '@mui/material';
import { useFeed, postCommand, SERVER_URL } from '@/lib/feed';
import { Board } from '@/components/Board';
import { SabotageBar } from '@/components/SabotageBar';

export default function LivePage() {
  const { state } = useFeed();
  const fmt = (t: number) => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, '0')}`;
  return (
    <Stack spacing={1.5}>
      {!state.connected && (
        <Alert severity="warning">Not connected to the estimator server at <code>{SERVER_URL}</code>. Run <code>npm run server</code> in the repo, then start the Unreal sim (or use the Replay page, which needs no server).</Alert>
      )}
      {state.connected && !state.unrealConnected && !state.replaying && (
        <Alert severity="info">Server connected, waiting for the Unreal simulator. Press Play in the editor or launch <code>build/Mac/VerticalGeofenceSim.app</code>; or replay a recording from the Replay page.</Alert>
      )}
      <Board
        state={state}
        header={
          <Stack direction="row" spacing={1} sx={{ alignItems: 'center' }}>
            <Chip size="small" label={state.unrealConnected ? 'Unreal: live' : state.replaying ? 'Replaying on server' : 'No source'} color={state.unrealConnected ? 'success' : state.replaying ? 'info' : 'default'} />
            {state.session && <Typography variant="caption" color="text.secondary">seed {state.session.seed} · t = {fmt(state.t)}</Typography>}
          </Stack>
        }
      />
      <SabotageBar state={state} send={(cmd) => void postCommand(cmd)} />
    </Stack>
  );
}
