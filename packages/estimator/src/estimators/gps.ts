// What a site-perimeter (GPS) geofence does: if the phone is on site, credit the floor the worker is
// assigned to. It cannot tell the lobby from the 12th floor - this is the before-state that vertical
// geofencing replaces, and the one the "clocked in from the lobby" story is about.
import type { Observation, SessionInfo } from '@sim/protocol';
import { UNKNOWN, type FloorEstimate, type FloorEstimator } from '../types';

export class SiteGeofenceEstimator implements FloorEstimator {
  readonly name = 'gps';
  private assigned = new Map<string, number>();

  reset(session: SessionInfo): void {
    this.assigned.clear();
    for (const w of session.workers) if (w.targetFloor !== undefined) this.assigned.set(w.id, w.targetFloor);
  }

  update(obs: Observation): FloorEstimate {
    // Any report at all means the phone is inside the site perimeter.
    const floor = this.assigned.get(obs.worker);
    if (floor === undefined) return UNKNOWN;
    return { floor, confidence: 1, state: 'stable' };
  }
}
