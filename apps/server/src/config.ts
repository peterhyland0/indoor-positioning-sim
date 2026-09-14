import { resolve } from 'node:path';
import { ESTIMATOR_NAMES, type EstimatorName } from '@sim/estimator';

export interface Config {
  port: number;
  databaseUrl: string | null;
  primary: EstimatorName;
  hourlyRate: number;
  sessionsDir: string;
}

export function loadConfig(env: NodeJS.ProcessEnv = process.env): Config {
  const primary = (env.PRIMARY_ESTIMATOR ?? 'fused') as EstimatorName;
  if (!ESTIMATOR_NAMES.includes(primary)) throw new Error(`PRIMARY_ESTIMATOR must be one of ${ESTIMATOR_NAMES.join(', ')}`);
  return {
    port: Number(env.PORT ?? 8080),
    databaseUrl: env.DATABASE_URL || null,
    primary,
    hourlyRate: Number(env.HOURLY_RATE ?? 45),
    sessionsDir: resolve(env.SESSIONS_DIR ?? resolve(import.meta.dirname, '../../../sessions')),
  };
}
