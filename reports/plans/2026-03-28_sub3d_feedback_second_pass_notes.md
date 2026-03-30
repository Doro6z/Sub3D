# Sub3D Feedback Second Pass Notes

Date: 2026-03-28
Scope: placement pass after layout, runtime leak audio, and current Proto04C positioning
Status: active working note

## 1. Where We Are In The Plan

- `Proto04B`: closed
- `Proto04C`:
  - `C.1 / C.2`: done
  - `C.3 / C.4`: done as placeholder runtime
  - `C.5`: consolidated around `USubmarineFeedbackDirectorComponent`
  - `C.6`: not started

Current state:
- the gameplay truth chain exists,
- the feedback runtime exists,
- the placement strategy now needs to be separated cleanly between authored props, post-layout spawned anchors, and runtime-only sources.

## 2. Placement Decision

The feedback placement logic should not live in the envelope.

Envelope remains responsible for:
- hull shape
- compartment geometry
- compile-time layout metrics

Feedback placement should happen after layout resolution.

This means:
- no alarm props in `EnvelopeDef`
- no flood audio anchors in `EnvelopeDef`
- no leak audio anchors authored into the envelope model

## 3. Authoring Model By Feedback Type

### 3.1 Alarm

Keep as authored or placed props.

Runtime model:
- `ASubmarineAlarmBeacon`

Placement model:
- attached in the submarine BP for now
- later spawnable from a post-layout placement pass if desired

Reason:
- alarm is a real visible prop
- it owns:
  - mesh
  - red light / gyrophare style light
  - spatialized audio component

### 3.2 Flood Interior

Keep as compartment-scoped anchors.

Runtime model:
- `ASubmarineFloodAudioAnchor`

Placement model:
- attached in the submarine BP for now
- later spawned automatically from a post-layout pass:
  - usually one anchor per relevant compartment
  - keyed by `CompartmentId`

Reason:
- flood ambience is compartment-level state
- this should be stable and spatialized in the playable interior

### 3.3 Leak / Pressure

Do not author dedicated actors.

Runtime model:
- leak audio is emitted directly by runtime `UAudioComponent` instances pooled by `USubmarineFeedbackDirectorComponent`
- each active source is attached to the submarine and positioned at a current breach cluster

Reason:
- leak audio follows transient runtime state
- it should track actual breach locations
- authoring fixed leak actors would be unnecessary and brittle

## 4. Implemented Runtime Split

Current split is now:
- `AlarmBeacon`: authored / attached prop
- `FloodAudioAnchor`: authored / attached compartment anchor
- `LeakAudio`: runtime pooled source on live breach positions

This is the intended short-term architecture.

## 5. Next Placement Pass

The next placement pass should be a post-layout pass, not an envelope pass.

Goals:
- spawn flood audio anchors from compiled layout
- optionally place default alarm beacons from layout heuristics
- keep leak audio runtime-only

Recommended inputs:
- `USubmarineLayoutAsset`
- compartment ids
- station or passage positions if useful
- eventual designer overrides in BP

Recommended outputs:
- attached `ASubmarineFloodAudioAnchor` actors
- optional attached `ASubmarineAlarmBeacon` actors

## 6. Immediate Designer Workflow

Until the placement pass is implemented:

1. keep `bFreezeMovementForTesting = true` during feedback iteration
2. assign `USubmarineFeedbackProfile` on the feedback director
3. attach one or more `ASubmarineAlarmBeacon`
4. attach one `ASubmarineFloodAudioAnchor` per test compartment
5. let leak audio stay fully runtime-driven
6. validate with `CreateDebugBreachOnFirstExteriorSheet`

## 7. Deferred Work

Not for this pass:
- moving placement into envelope compilation
- final asset rules for all state tiers
- final sound mix pass
- final pressure/water layering design
- door pressure burst feedback
