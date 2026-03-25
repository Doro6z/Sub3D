# Sub3D Proto 03 - First Submarine Art Brief

Date: 2026-03-20  
Project: `C:/Dev/Sub3D`

Companion blockout sheet:
`C:/Dev/Sub3D/reports/plans/20260320-proto03-first-sub-blockout-sheet.md`

## 1. Locked Intent

This first playable submarine is:

- small crew
- compatible with coop or AI crew navmesh
- small exploration vessel
- independent civilian / industrial
- repair-heavy and habitation-capable

It is **not**:

- a military-first attack submarine
- a sleek luxury craft
- a giant cargo barge
- a cramped one-man pod

## 2. One-Line DA

**A precious independent exploration work-sub: compact, pressure-rated, industrial, lived-in, and clearly maintained by a small crew.**

## 3. Spatial Program

The first ship should support these roles at the same time:

- piloting
- internal traversal
- emergency repair
- pump / engine interaction
- storage / rest read
- clear AI and player circulation

Recommended room program:

- Helm room
- Main corridor / central work spine
- Engine and pump room
- One side nook for bunk + storage

Do not add more than one optional side nook in the first ship.

## 4. Recommended Dimensions

- Exterior length: `14m` to `15m`
- Exterior width: `5m` to `5.5m`
- Exterior height: `4.5m` to `5m`
- Helm usable diameter: `3.4m`
- Main corridor clear width: `2.3m`
- Main corridor clear length: `5.5m` to `6m`
- Engine/pump zone depth: `3m` to `3.5m`
- Side nook clear footprint: about `2m x 2.2m`
- Door clear width: `1.2m`
- Door clear height: `2.1m`

## 5. Exterior Design Rules

- Exterior should read as one compact pressure vessel built around a real interior.
- The hull can be rounded, faceted, or flattened on the sides, but must remain thick and pressure-believable.
- Use reinforced rails, brackets, access plates, and maintenance seams.
- Add visible utility hardware: lights, antennae, sensor mast, external brackets, service ports.
- Keep weapons optional or absent on the first ship.

Window policy:

- rare
- small
- thick
- reinforced
- expensive-looking

Good window uses:

- one forward helm glazing cluster
- one or two small side hublots

Bad window uses:

- panoramic tourist glazing
- large civilian aquarium windows
- long strips of exposed glass

## 6. Interior Design Rules

- One obvious circulation spine at all times.
- Machinery belongs mostly on walls, corners, or rear room edges.
- Keep a readable repair frontage on major walls.
- Do not choke the corridor with crates or deep machinery.
- The ship must feel inhabited, but not domestic.

Interior balance:

- `70%` machine / structure / work
- `20%` storage / utility / survival
- `10%` sleep / personal trace

## 7. Mood Keywords

- industrial
- compact
- pressurized
- reinforced
- practical
- expensive to lose
- maintained by hand
- patched but reliable
- low-visibility explorer

## 8. Anti-Keywords

- naval destroyer
- luxury yacht
- pristine lab
- dieselpunk caricature
- steampunk ornament overload
- cramped capsule
- decorative greeble spam

## 9. Asset Priority Order

Build in this order:

1. `SM_Sub_Proto03_Hull_A`
2. `SM_Sub_Proto03_InteriorShell_A`
3. `SM_HelmConsole_A`
4. `SM_EngineBlock_A`
5. `SM_PumpConsole_A`
6. `SM_BulkheadDoor_A`
7. `SM_RepairPanel_A`
8. `SM_BunkFrame_A`
9. `SM_LockerBank_A`
10. `SM_PipeKit_A`
11. `SM_CableTray_A`
12. `SM_Lamp_Industrial_A`

If time is short, stop after item `7` and use primitive placeholders for the rest.

## 10. Recommended Modeling Workflow

1. Block out the interior in Blender first.
2. Validate dimensions against third-person movement.
3. Wrap the outer hull around that blockout.
4. Export a simple clay pass for side, top, front, and 3/4 views.
5. Use those views for paintover or AI ideation only if needed.
6. Build the actual final proto mesh from the validated blockout.

## 11. Unreal Dressing Guidance

Prefer doing these in Unreal:

- spline cables
- spline hoses
- repeat pipes
- warning lamps
- loose crates and clutter placement

Prefer modeling these as authored meshes:

- hull
- interior shell
- helm console
- engine mass
- pump station
- repair panel
- bunk and locker shapes

## 12. What Must Read On Screen

At a glance, the player should understand:

- where to pilot
- where to run
- where to repair
- where the mechanical heart of the ship is
- where the crew actually lives

If one of these reads is weak, the asset pass is not ready yet.
