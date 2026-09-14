'use client';
import { Box, ToggleButton, ToggleButtonGroup, Tooltip, Typography } from '@mui/material';
import { useTheme } from '@mui/material/styles';
import type { EstimatorName } from '@sim/estimator';
import { ESTIMATOR_COLOR } from '@/lib/palette';

export function EstimatorToggle({ value, onChange, estimators }: { value: EstimatorName; onChange: (e: EstimatorName) => void; estimators: EstimatorName[] }) {
  const dark = useTheme().palette.mode === 'dark';
  return (
    <Box sx={{ display: 'flex', alignItems: 'center', gap: 1.5 }}>
      <Typography variant="subtitle2">Estimator driving the board</Typography>
      <ToggleButtonGroup size="small" exclusive value={value} onChange={(_, v: EstimatorName | null) => v && onChange(v)}>
        {estimators.map((e) => (
          <Tooltip key={e} title={ESTIMATOR_COLOR[e].blurb}>
            <ToggleButton value={e} sx={{ textTransform: 'none', gap: 0.75 }}>
              <Box component="span" sx={{ width: 10, height: 10, borderRadius: '50%', bgcolor: ESTIMATOR_COLOR[e][dark ? 'dark' : 'light'] }} />
              {ESTIMATOR_COLOR[e].label}
            </ToggleButton>
          </Tooltip>
        ))}
      </ToggleButtonGroup>
    </Box>
  );
}
