# Meeting notes & open questions

Running list for conversations about the project. Add to it as things come up.

## For the next meeting with Mick

### Ideas

- **Per-trade rulesets for the punch engine.** Dwell/stillness thresholds should depend on the job:
  a welder can legitimately stand in one spot for a long time, a labourer moves constantly. Today the
  punch engine uses one `enterDwellSec` / `exitDwellSec` for everyone (`packages/estimator/src/punch.ts`);
  a per-worker (or per-trade) `PunchParams` keyed off the shift would let "stationary for 20 min" mean
  "working" for one trade and "phone left on a bench" for another.

### Questions

- **Devices:** do workers get a device (phone/tag) handed to them as they enter the building, or is it
  their own phone with the app installed? Changes everything about the phone model — a site-issued tag
  could be always-on, no iOS background limits, no "in pocket" variability.
- **Stack check:** is the assumed tech stack right? Assumed, with evidence:
  - TypeScript, Node, PostgreSQL — from the full-stack job ad (high confidence)
  - React + Next.js App Router — `app.ralco.io` serves `/_next/` App Router chunks (high)
  - MUI + emotion — style tags on the login page, alongside legacy Bootstrap 4 (medium)
  - Mobile is React Native / hybrid — ad says "native or hybrid"; a guess (low)
  - BLE beacons per floor with nearest-beacon logic, no barometer — beacons confirmed by release notes
    and Android permissions; the logic is inferred (medium)
  - Floor estimation on the **server**, live data over WebSockets, hosted Postgres — my architectural
    choices, no evidence either way (unknown)

  Most consequential to get right: (1) phone-side vs server-side estimation — decides whether the
  estimator package needs to drop into React Native; (2) is mobile actually React Native; (3) do they
  already smooth / apply hysteresis, which would make my "nearest" baseline unfair to them.
- **RF model check:** the simulator's radio model is a straight line from beacon to phone with
  log-distance path loss plus a fixed loss per slab crossed (`UPhoneSensorsComponent::ComputeRssi`).
  Is that a fair stand-in for what they see on site, or is real behaviour dominated by something else
  (multipath, rebar, people)? Do they have any measured RSSI-vs-floor data to calibrate against?
