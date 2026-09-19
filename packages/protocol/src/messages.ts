// Wire format between the Unreal simulator and the estimator side. Mirrors docs/bridge-protocol.md.
// Anything the estimator may see is defined here; anything only the scorer may see is in `Truth`.
import { z } from 'zod';

export const Platform = z.enum(['ios', 'android']);
export const AppState = z.enum(['fg', 'bg']);
export const PhoneState = z.enum(['inHand', 'inPocket']);

export const Scan = z.object({
  /** beacon id, e.g. "F12-A" */
  b: z.string(),
  /** dBm */
  rssi: z.number(),
});

export const RegionEvent = z.object({
  b: z.string(),
  event: z.enum(['enter', 'exit']),
});

/** Ground truth. Scoring only - never passed to an estimator. */
export const Truth = z.object({
  floor: z.number().int(),
  onHoist: z.boolean(),
  z: z.number(),
});

export const BeaconInfo = z.object({
  id: z.string(),
  floor: z.number().int(),
  x: z.number(),
  y: z.number(),
  z: z.number(),
  txPowerDbm: z.number().default(-59),
});

export const WorkerInfo = z.object({
  id: z.string(),
  platform: Platform,
  route: z.string().optional(),
  /** the floor the worker is assigned to work on (their shift); what a site-level geofence would credit */
  targetFloor: z.number().int().optional(),
  /** e.g. Electrician, Ironworker, Labourer, Supervisor */
  trade: z.string().optional(),
});

export const SessionMsg = z.object({
  v: z.literal(1),
  type: z.literal('session'),
  seed: z.number().int(),
  floorHeight: z.number(),
  numFloors: z.number().int(),
  config: z.record(z.string(), z.unknown()).default({}),
  beacons: z.array(BeaconInfo),
  workers: z.array(WorkerInfo),
});

export const ScanMsg = z.object({
  v: z.literal(1),
  type: z.literal('scan'),
  /** sim seconds since Play */
  t: z.number(),
  worker: z.string(),
  platform: Platform,
  appState: AppState,
  phoneState: PhoneState.default('inHand'),
  scans: z.array(Scan),
  regionEvents: z.array(RegionEvent).default([]),
  /** hPa at the phone */
  pressure: z.number(),
  /** hPa at the lobby reference station, same instant */
  refPressure: z.number(),
  truth: Truth,
});

export const EventMsg = z.object({
  v: z.literal(1),
  type: z.literal('event'),
  t: z.number(),
  name: z.string(),
  payload: z.record(z.string(), z.unknown()).default({}),
});

/** Estimator -> Unreal: colours the worker label, flashes a punch. */
export const EstimateMsg = z.object({
  v: z.literal(1),
  type: z.literal('estimate'),
  worker: z.string(),
  estFloor: z.number().int().nullable(),
  punch: z.enum(['in', 'out']).nullable(),
  /** which estimator produced it, e.g. "fused" */
  source: z.string().optional(),
});

/** Dashboard -> Unreal (via the server): a sabotage/control command, see ASimBuilding::RunCommand. */
export const CommandMsg = z.object({
  v: z.literal(1),
  type: z.literal('command'),
  cmd: z.string(),
});

export const OutboundMsg = z.discriminatedUnion('type', [SessionMsg, ScanMsg, EventMsg]);
export const InboundMsg = z.discriminatedUnion('type', [EstimateMsg, CommandMsg]);
export const BridgeMsg = z.discriminatedUnion('type', [SessionMsg, ScanMsg, EventMsg, EstimateMsg, CommandMsg]);

export type Platform = z.infer<typeof Platform>;
export type AppState = z.infer<typeof AppState>;
export type PhoneState = z.infer<typeof PhoneState>;
export type Scan = z.infer<typeof Scan>;
export type RegionEvent = z.infer<typeof RegionEvent>;
export type Truth = z.infer<typeof Truth>;
export type BeaconInfo = z.infer<typeof BeaconInfo>;
export type WorkerInfo = z.infer<typeof WorkerInfo>;
export type SessionMsg = z.infer<typeof SessionMsg>;
export type ScanMsg = z.infer<typeof ScanMsg>;
export type EventMsg = z.infer<typeof EventMsg>;
export type EstimateMsg = z.infer<typeof EstimateMsg>;
export type CommandMsg = z.infer<typeof CommandMsg>;
export type OutboundMsg = z.infer<typeof OutboundMsg>;
export type InboundMsg = z.infer<typeof InboundMsg>;
export type BridgeMsg = z.infer<typeof BridgeMsg>;

/** Parse one JSONL line / one socket frame. Throws on malformed input. */
export function parseLine(line: string): BridgeMsg {
  return BridgeMsg.parse(JSON.parse(line));
}

/** Non-throwing variant for streams where a bad line should be skipped, not fatal. */
export function tryParseLine(line: string): BridgeMsg | null {
  try {
    return parseLine(line);
  } catch {
    return null;
  }
}
