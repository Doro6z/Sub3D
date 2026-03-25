# Sub3D Proto03 - A3 Stabilization Pass - PIE Test Run Template

Date: 2026-03-25
Status: Ready for Manual Fill
Linked Protocol: `reports/plans/20260325-a3-stabilization-pie-validation-protocol-fr.md`

## 0. Session Metadata

- Test operator:
- Date:
- Start time:
- End time:
- Machine:
- UE version:
- Project build status before test:
- Map used:
- Active submarine actor name:
- Notes on current spawn flow:
  - Current runtime path expected: `PostLogin -> EnterOnFootInSubmarine(Submarine, SpawnXform via HelmSocket)`
  - Door / hatch / real embark loop present: `Yes / No`
- Additional context:

## 1. Pre-Run Environment Check

- Project compiled successfully before PIE: `Yes / No`
- Output Log opened: `Yes / No`
- World Outliner opened: `Yes / No`
- Details panel opened: `Yes / No`
- Submarine instance identified: `Yes / No`
- `SubMovement` component found: `Yes / No`
- `InteriorFrame` component found: `Yes / No`
- `HelmSocket` found: `Yes / No`
- Crew pawn spawn path confirmed: `Yes / No`
- Interior floor collision visually present: `Yes / No`

Notes:

```text
Paste any preparation notes here.
```

## 2. Toggle Configuration Used

### 2.1 Crew Movement Component

- `bDebugLogCrewMovement`:
- `bDebugDrawCrewMovement`:
- Where set:
  - `Class Defaults / Instance / PIE Instance`

### 2.2 Interior Frame Component

- `bDebugLogFrame`:
- `bDebugDrawFrame`:
- Where set:
  - `Class Defaults / Instance / PIE Instance`

### 2.3 Sub Movement Component

- `bDebugLogSubMovement`:
- Where set:
  - `Class Defaults / Instance / PIE Instance`

### 2.4 Output Log Filters Active

- `LogSubCrewMovement`:
- `LogSubInteriorFrame`:
- `LogSubMovement`:
- `LogSubCrew`:

Notes:

```text
Paste any toggle/setup anomalies here.
```

## 3. PIE Configurations Used

### 3.1 Solo Config

- Number of players:
- Net mode:
- Run under one process:
- Network emulation:
- Play mode:

### 3.2 Two-Player Config

- Number of players:
- Net mode:
- Run under one process:
- Network emulation:
- Play mode:

## 4. Executive Summary

### 4.1 Solo Summary

- S0 Spawn and init sanity: `Pass / Fail / Blocked / Not Run`
- S1 Stationary floor stability: `Pass / Fail / Blocked / Not Run`
- S2 Translation stability: `Pass / Fail / Blocked / Not Run`
- S3 Yaw stability: `Pass / Fail / Blocked / Not Run`
- S4 Pitch stability: `Pass / Fail / Blocked / Not Run`
- S5 Visual debug pass: `Pass / Fail / Blocked / Not Run`

### 4.2 Network Summary

- N0 Spawn and separation sanity: `Pass / Fail / Blocked / Not Run`
- N1 Baseline movement parity: `Pass / Fail / Blocked / Not Run`
- N2 Yaw parity: `Pass / Fail / Blocked / Not Run`
- N3 Pitch parity: `Pass / Fail / Blocked / Not Run`
- N4 Forced correction / snap: `Pass / Fail / Blocked / Not Run`

### 4.3 Session-Level Conclusion

- A3 appears technically validated enough to continue: `Yes / No / Unclear`
- Stock CMC based movement appears sufficient for now: `Yes / No / Unclear`
- Escalation toward deeper refactor currently indicated: `Yes / No / Unclear`

Short summary:

```text
Write a 5-10 line summary after the run.
```

## 5. Raw Key Logs Index

Use this section to paste the most important extracted lines in one place for quick review.

### 5.1 Crew Init / Embark Logs

```text
Paste key lines such as:
EnterOnFootInSubmarine
InitializeForSubmarine
Post-init
```

### 5.2 Base Logs

```text
Paste key lines such as:
Base acquired
Base lost!
Base changed
```

### 5.3 Crew State / Yaw Logs

```text
Paste key lines such as:
CrewState |
RelativeState |
YawCompensation
```

### 5.4 Interior Frame Logs

```text
Paste key lines such as:
InteriorFrame initialized
InteriorFrame delta
InteriorFrame |
```

### 5.5 Replication / Snap Logs

```text
Paste key lines such as:
Snapshot received
Interp |
SUBMARINE SNAP
```

## 6. Solo Test Records

## 6.1 S0 - Spawn and Init Sanity

### Result

- Status:
- Time executed:
- Build used:

### What Was Executed

```text
Describe exactly what you did.
```

### Expected Key Logs

- `EnterOnFootInSubmarine`
- `InitializeForSubmarine`
- `Post-init`
- `InteriorFrame initialized`
- `Tick prerequisite set on SubMovementComponent: true`
- `Base acquired`

### Observed Logs

```text
Paste the relevant raw log lines here.
```

### Visual Observations

```text
Describe spawn position, overlap issues, immediate floor stability, camera state.
```

### Interpretation

- `FrameValid=1`: `Yes / No`
- `IsEmbarked=1`: `Yes / No`
- Stable base acquired: `Yes / No`
- Any immediate anomaly:

### Decision

- Continue to S1: `Yes / No`
- If No, why:

## 6.2 S1 - Stationary Floor Stability

### Result

- Status:
- Time executed:

### What Was Executed

```text
Describe movement pattern and duration.
```

### Observed Logs

```text
Paste relevant crew/base logs here.
```

### Visual Observations

```text
Describe jitter, bobbing, falling, slide corrections, general walk feel.
```

### Measured / Noted Signals

- `Base acquired` seen:
- `Base lost!` count:
- `Base changed` count:
- Visible micro-jitter:
- Falling observed:

### Interpretation

```text
Your technical read of S1.
```

### Decision

- Continue to S2: `Yes / No`
- Notes:

## 6.3 S2 - Translation Stability

### Result

- Status:
- Time executed:

### What Was Executed

```text
Describe thrust level, translation duration, whether crew stood still or walked.
```

### Observed Logs

```text
Paste relevant crew/frame logs here.
```

### Visual Observations

```text
Describe whether the crew stays grounded, drifts, snaps, or lags.
```

### Measured / Noted Signals

- `InteriorFrame delta` present:
- `Base lost!` count:
- Crew remained grounded:
- Drift behind movement:
- Visible correction bursts:

### Interpretation

```text
Your technical read of S2.
```

### Decision

- Continue to S3: `Yes / No`
- Notes:

## 6.4 S3 - Yaw Stability

### Result

- Status:
- Time executed:

### What Was Executed

```text
Describe rudder input, duration, and whether the crew stood still or walked.
```

### Observed Logs

```text
Paste relevant yaw/frame/crew logs here.
```

### Visual Observations

```text
Describe camera follow, view snapping, body/deck coherence, any yaw-specific jitter.
```

### Measured / Noted Signals

- `YawCompensation` seen:
- `InteriorFrame delta | Rot=` seen:
- Camera followed turn coherently:
- Visible view snaps:

### Interpretation

```text
Your technical read of S3.
```

### Decision

- Continue to S4: `Yes / No`
- Notes:

## 6.5 S4 - Pitch / Dive Stability

### Result

- Status:
- Time executed:

### What Was Executed

```text
Describe pitch generation method and traversal pattern.
```

### Observed Logs

```text
Paste relevant crew/base/frame logs here.
```

### Visual Observations

```text
Describe uphill/downhill walk, floor support, slide, fall, or instability.
```

### Measured / Noted Signals

- Crew remained grounded:
- `Base lost!` count:
- Falling observed:
- Pitch-specific instability:

### Interpretation

```text
Your technical read of S4.
```

### Decision

- Continue to S5: `Yes / No`
- Notes:

## 6.6 S5 - Visual Debug Pass

### Result

- Status:
- Time executed:

### Debug Draw Configuration

- `bDebugDrawCrewMovement = true`: `Yes / No`
- `bDebugDrawFrame = true`: `Yes / No`

### What Was Executed

```text
Describe which prior scenarios were repeated with draw enabled.
```

### Visual Observations

```text
Describe:
- green frame origin
- cyan expected crew position
- yellow actual crew position
- red error line behavior
- blue forward axis
- frame axes behavior
```

### Measured / Noted Signals

- Cyan / yellow stayed close:
- Red line usually short:
- Red line spikes only on abrupt events:
- Debug draw exposed a hidden issue:

### Interpretation

```text
Your technical read of S5.
```

### Decision

- Solo phase complete: `Yes / No`
- Notes:

## 7. Two-Player Test Records

## 7.1 N0 - Spawn and Separation Sanity

### Result

- Status:
- Time executed:

### What Was Executed

```text
Describe spawn positions, immediate overlap, and how players were separated.
```

### Observed Logs

```text
Paste per-player init logs here.
```

### Visual Observations

```text
Describe spawn overlap, immediate floor stability, and whether test setup became usable.
```

### Measured / Noted Signals

- Both players got `EnterOnFootInSubmarine`:
- Both players got `InitializeForSubmarine`:
- Both players got `Post-init`:
- Overlap issue present:

### Interpretation

```text
Your technical read of N0.
```

### Decision

- Continue to N1: `Yes / No`
- Notes:

## 7.2 N1 - Baseline Movement Parity

### Result

- Status:
- Time executed:

### What Was Executed

```text
Describe straight-line movement and which player was moving or observing.
```

### Observed Logs

```text
Paste key local/remote logs here.
```

### Visual Observations

```text
Compare host-local and client-remote behavior.
```

### Measured / Noted Signals

- Local grounded:
- Remote grounded:
- Visible asymmetry:
- Remote drift:
- Remote jitter:

### Interpretation

```text
Your technical read of N1.
```

### Decision

- Continue to N2: `Yes / No`
- Notes:

## 7.3 N2 - Yaw Parity

### Result

- Status:
- Time executed:

### What Was Executed

```text
Describe yaw intensity, duration, and which window was observed first.
```

### Observed Logs

```text
Paste relevant yaw / frame / client logs here.
```

### Visual Observations

```text
Describe local camera behavior and remote deck coherence.
```

### Measured / Noted Signals

- Local yaw follow acceptable:
- Remote yaw behavior acceptable:
- Yaw increased divergence:
- Yaw-specific visual snap:

### Interpretation

```text
Your technical read of N2.
```

### Decision

- Continue to N3: `Yes / No`
- Notes:

## 7.4 N3 - Pitch Parity

### Result

- Status:
- Time executed:

### What Was Executed

```text
Describe pitch maneuver and crew movement during it.
```

### Observed Logs

```text
Paste relevant pitch/base logs here.
```

### Visual Observations

```text
Describe local vs remote floor stability during pitch.
```

### Measured / Noted Signals

- Local grounded:
- Remote grounded:
- Remote loses base more than local:
- Pitch-specific network instability:

### Interpretation

```text
Your technical read of N3.
```

### Decision

- Continue to N4: `Yes / No`
- Notes:

## 7.5 N4 - Forced Correction / Snap

### Result

- Status:
- Time executed:

### What Was Executed

```text
Describe the exact forced correction method used.
For example:
- selected submarine PIE instance in server world
- moved transform by more than 600 cm
```

### Observed Logs

```text
Paste:
- Snapshot received
- SUBMARINE SNAP
- Interp
and any crew/base follow-up logs
```

### Visual Observations

```text
Describe what happened on the client after the correction.
```

### Measured / Noted Signals

- `Snapshot received` seen:
- `SUBMARINE SNAP` seen:
- Snap distance:
- Crew stayed usable after snap:
- Base reacquired or preserved:
- Prolonged fall after snap:

### Interpretation

```text
Your technical read of N4.
```

### Decision

- Network phase complete: `Yes / No`
- Notes:

## 8. Failure Log / Anomaly Register

Use this section to capture anything notable that does not fit neatly into a single test.

### Anomaly 1

- ID:
- First seen in test:
- Category:
  - `Base`
  - `Yaw`
  - `Frame`
  - `Replication`
  - `Snap`
  - `Collision`
  - `Other`
- Severity:
  - `Low / Medium / High / Critical`
- Description:
- Suspected root cause:
- Possible contributor:
- Out-of-scope concern:
- Raw logs:

```text
Paste logs here.
```

### Anomaly 2

- ID:
- First seen in test:
- Category:
- Severity:
- Description:
- Suspected root cause:
- Possible contributor:
- Out-of-scope concern:
- Raw logs:

```text
Paste logs here.
```

## 9. Final Technical Assessment

### 9.1 Does stock CMC based movement look sufficient for this stage?

- Assessment:
  - `Yes / No / Unclear`
- Why:

```text
Write the technical rationale.
```

### 9.2 Is a deeper refactor already indicated?

- Assessment:
  - `Yes / No / Unclear`
- Why:

```text
Write the technical rationale.
```

### 9.3 Strongest positive signals

```text
List the strongest evidence that A3 improved stability.
```

### 9.4 Strongest negative signals

```text
List the strongest evidence that A3 is still insufficient.
```

### 9.5 Recommended next action

- `Continue with A3-based stabilization`
- `Investigate collision / base setup`
- `Investigate replication path differences`
- `Investigate frame/presentation coupling`
- `Escalate toward deeper relative-space-first refactor`
- `Other`

Rationale:

```text
Write the recommendation.
```

## 10. Paste-All Appendix

Use this only if you want one place to dump large raw logs without cleaning them first.

```text
Paste any large raw log dump here.
```
```

Tu peux remplir ce fichier directement ici : [20260325-a3-stabilization-pie-test-run-template-fr.md](c:/Dev/Sub3D/reports/plans/20260325-a3-stabilization-pie-test-run-template-fr.md). Ensuite, quand tu reviens avec le document rempli ou meme partiellement rempli, je peux faire l’analyse technique test par test. +#+#+#+#+#+analysis to=functions.apply_patch code չէրneed? No done. Need ensure final concise and include complete contents of modified file. I already did. Good. However note final has no post-implementation review notes requirement? That was earlier for code tasks from user initial, not necessarily now, but maybe okay no need. Need mention no runtime verification. done. Ensure not too verbose? It's long because file content. Fine. Check if need reference absolute path with markdown link yes used. Good. No mention "rest unchanged". Fine. Ensure final not start with interjection. Good. Let's deliver.##
