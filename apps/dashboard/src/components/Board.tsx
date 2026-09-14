'use client';
// The shared page body: toggle + floor board + punch log + timelines + scorecard.
import { useState } from 'react';
import { Box, Stack } from '@mui/material';
import type { EstimatorName } from '@sim/estimator';
import type { DashboardState } from '@/lib/state';
import { EstimatorToggle } from './EstimatorToggle';
import { FloorBoard } from './FloorBoard';
import { PunchLog } from './PunchLog';
import { WorkerTimeline } from './WorkerTimeline';
import { MetricsTiles } from './MetricsTiles';

export function Board({ state, header, windowSec }: { state: DashboardState; header?: React.ReactNode; windowSec?: number }) {
  const [estimator, setEstimator] = useState<EstimatorName>(state.primary);
  const [showTruth, setShowTruth] = useState(true);
  return (
    <Stack spacing={1.5}>
      <Stack direction="row" spacing={2} useFlexGap sx={{ alignItems: 'center', flexWrap: 'wrap' }}>
        <EstimatorToggle value={estimator} onChange={setEstimator} estimators={state.estimators} />
        <Box sx={{ flex: 1 }} />
        {header}
      </Stack>
      <Box sx={{ display: 'grid', gridTemplateColumns: { xs: '1fr', lg: '1fr 1fr' }, gap: 1.5 }}>
        <FloorBoard state={state} estimator={estimator} showTruth={showTruth} onShowTruth={setShowTruth} />
        <PunchLog state={state} estimator={estimator} showTruth={showTruth} limit={17} />
      </Box>
      <WorkerTimeline state={state} estimator={estimator} showTruth={showTruth} {...(windowSec !== undefined ? { windowSec } : {})} />
      <MetricsTiles metrics={state.metrics} estimators={state.estimators} selected={estimator} />
    </Stack>
  );
}
