# indoor-positioning-sim

Browser-based simulator for an indoor positioning stack (BLE RSSI + dead reckoning + barometer), fused by a map-constrained particle filter.

Planned:
- Floor plan with wall materials and placed beacons
- Synthetic walker generating RSSI, dead reckoning and barometric readings with realistic noise
- Particle filter fusing all three, constrained by the map (no walking through walls), floor level from the barometer
- Output: ground truth vs estimate, error CDF, floor classification accuracy — degradable under beacon dropout, barometer drift and crowds
- Replay of real phone + beacon traces through the same engine
- Runs from a link, nothing to install
