# Sub3D Proto 03 - First Submarine Blockout Sheet

Date: 2026-03-20  
Project: `C:/Dev/Sub3D`

## 1. Scope

This sheet is the concrete modeling guide for the first playable submarine.

It is meant for:

- Blender blockout
- Unreal import
- Gemini or artist handoff

This is a **prototype production sheet**, not final art bible.

## 2. Locked Design Assumptions

- small crew
- coop or AI crew navmesh compatible
- small exploration vessel
- independent civilian / industrial
- rare, reinforced windows only
- repair-heavy internal loop
- third-person movement first

## 3. Asset Deliverables

Required for first playable pass:

- `SM_Sub_Proto03_Hull_A`
- `SM_Sub_Proto03_InteriorShell_A`
- `SM_Sub_Proto03_BulkheadDoor_A`
- `SM_Sub_Proto03_Hatch_A`
- `SM_HelmConsole_A`
- `SM_EngineBlock_A`
- `SM_PumpConsole_A`
- `SM_RepairPanel_A`

Optional but recommended:

- `SM_BunkFrame_A`
- `SM_LockerBank_A`
- `SM_PipeKit_A`
- `SM_CableTray_A`
- `SM_Lamp_Industrial_A`

## 4. Coordinate And Scale Rules

### Unreal target

- Forward: `+X`
- Right: `+Y`
- Up: `+Z`
- Dimensions authored in real-world scale

### Blender working rule

Use metric scene scale and think in meters while modeling.

Recommended scene setup:

- Unit System: `Metric`
- Unit Scale: `1.0`
- Length: `Meters`

Then export with FBX settings that preserve scale for Unreal.

### Pivot rule

For the submarine root meshes:

- pivot at submarine center on floor datum, roughly mid-length
- Z=0 should correspond to usable floor reference, not geometric center

For props:

- floor props pivot at base center
- wall props pivot at back center
- doors pivot on hinge side if animated later

## 5. Collections In Blender

Use these collections:

- `COL_Sub_Proto03_Blockout`
- `COL_Sub_Proto03_Hull`
- `COL_Sub_Proto03_Interior`
- `COL_Sub_Proto03_Stations`
- `COL_Sub_Proto03_Dressing`
- `COL_Sub_Proto03_Reference`

## 6. Overall Dimensions

- Total exterior length: `14.8m`
- Total exterior width: `5.2m`
- Total exterior height: `4.8m`
- Interior usable length: about `12.2m`
- Interior usable max width: about `3.6m`
- Interior clear height: `2.35m`

These dimensions are chosen for gameplay readability, not naval realism.

## 7. Layout Plan

Use this sequence from front to back:

1. Helm room
2. Main corridor
3. Side bunk-storage nook
4. Engine / pump room
5. Rear service end with repair frontage

### Top-level plan dimensions

- Helm room depth: `2.8m`
- Corridor length: `5.8m`
- Side nook footprint: `2.0m x 2.2m`
- Engine room depth: `3.2m`
- Rear service strip: `1.2m`

### Corridor widths

- Main clear walk path: `2.3m`
- Minimum emergency pass width near dressed walls: `1.45m`

## 8. Floor Plan ASCII

```text
Front

  [ HELM ]
  rounded room, wide front console arc
  dia ~3.4m

  [ BULKHEAD A ]

  [ MAIN CORRIDOR ]-------------------------[ ENGINE / PUMP ]
  clear path 2.3m                            denser machinery
  left/right wall service runs               rear repair wall

                [ SIDE NOOK ]
                bunk + lockers
                shallow inset

  [ BULKHEAD B / REAR SERVICE TRANSITION ]

Rear
```

## 9. Piece-By-Piece Modeling Specs

### A. `SM_Sub_Proto03_InteriorShell_A`

This is the gameplay truth mesh for the interior.

Build it first.

Target dimensions:

- length: `12.2m`
- max inner width: `3.6m`
- clear height: `2.35m`
- wall thickness visual target: `0.18m` to `0.25m`

Shape rules:

- keep floor mostly flat
- walls can be gently curved or faceted
- ceiling can be curved, but avoid low side pinching
- keep long wall stretches readable for future damage visuals

Openings:

- Bulkhead A opening width: `1.2m`
- Bulkhead B opening width: `1.2m`
- opening height: `2.1m`

### B. `SM_Sub_Proto03_Hull_A`

Build this second, wrapping the interior shell.

Outer shell rules:

- must feel pressure-rated
- should not become a fish-shaped traditional military sub
- can be rounded capsule + faceted side planes
- use industrial seams, rails, hatches, access plates

Exterior suggestions:

- one forward reinforced window cluster
- one small side porthole max per side, optional
- one top sensor mast
- one rear thruster assembly placeholder zone

### C. `SM_Sub_Proto03_BulkheadDoor_A`

Dimensions:

- frame width: `1.45m`
- frame height: `2.3m`
- clear opening: `1.2m x 2.1m`
- thickness: `0.18m`

Shape:

- heavy pressure door
- simple and readable
- circular or rounded-rect inner leaf both acceptable

### D. `SM_Sub_Proto03_Hatch_A`

Use only if you want one top or floor maintenance access.

Dimensions:

- clear opening: `0.8m`
- outer frame: `1.0m`

Do not add more than one in the first pass.

### E. `SM_HelmConsole_A`

This is the strongest gameplay landmark.

Dimensions:

- overall width: `2.4m`
- depth: `0.9m`
- height: `1.15m`
- player operating clearance in front: `1.5m`

Placement:

- front arc of helm room
- center-aligned

### F. `SM_EngineBlock_A`

Dimensions:

- footprint: `1.8m x 1.2m`
- height: `1.6m`

Placement:

- rear room, one side or centered rear-biased
- leave at least `1.2m` repair access on one exposed face

### G. `SM_PumpConsole_A`

Dimensions:

- footprint: `1.2m x 0.7m`
- height: `1.2m`

Placement:

- engine room, but distinct from engine mass
- should read immediately as secondary station

### H. `SM_RepairPanel_A`

Dimensions:

- width: `0.8m`
- height: `1.2m`
- wall depth: `0.15m`

Placement:

- rear room wall
- or corridor wall near service line

This should be on a clean readable wall, not buried in clutter.

## 10. Side Nook Program

The side nook is the only non-linear pocket in the first ship.

Purpose:

- prove habitation
- create a secondary landmark
- create asymmetry without killing navigation

Contents:

- one bunk
- one locker bank
- one small shelf or crate zone

Hard rule:

- keep the nook shallow enough that the main corridor still reads as the dominant path

## 11. Window Design Guidance

Allowed:

- one forward reinforced helm glazing cluster
- one optional small side porthole near bunk or corridor

Not allowed:

- panoramic front wall glass
- long strip windows
- multiple decorative side windows

Construction read:

- thick frame
- external bolts or clamp ring
- inset glass
- armored shutter option if desired

## 12. Repair Surface Reservation

Reserve these wall zones visually.

Do not fill them with deep geometry.

Required repair-friendly surfaces:

- Helm room side wall: `1.3m` wide clean section
- Mid corridor side wall: `1.5m` wide clean section
- Engine room side wall: `1.5m` wide clean section
- Rear room wall: `1.4m` wide clean section

These surfaces are valuable for:

- breach VFX
- hole masks
- repair readability
- suction event readability

## 13. Navmesh And Circulation Rules

Target clearances:

- ideal walk path width: `2.3m`
- minimum local squeeze width: `1.2m`
- no decorative dead ends smaller than `1m`
- no low ceiling beams below `2.05m`

Avoid:

- zig-zag corridor
- machine islands in the center path
- overhanging props at head level in traversal path

## 14. Modeling Order

Follow this order exactly for speed:

1. floor slab
2. interior side walls
3. helm front curve
4. rear engine room envelope
5. side nook cut-in
6. ceiling closure
7. door openings
8. outer hull wrap
9. helm console
10. engine block
11. pump console
12. repair panel
13. basic bunk + lockers
14. export to Unreal for scale validation
15. only after validation, add dressing kit details

## 15. Unreal Validation Pass

Before detailing, import and validate:

- third-person turn radius in helm room
- sprint panic traversal through both bulkheads
- clear visibility from corridor to engine room
- wall readability for future breach effects
- no prop blocking on likely suction vectors

If any of these fail, return to blockout before adding detail.

## 16. Export Notes

Recommended export grouping:

- export structural meshes separately
- export stations separately
- keep dressing kit modular

Naming:

- `SM_` for meshes
- `M_` for master materials
- `MI_` for material instances
- `T_` for textures

## 17. What To Hand To AI If Needed

If you want Gemini or another model to help generate variants, give it:

- this document
- top view blockout screenshot
- side orthographic screenshot
- front orthographic screenshot
- one clean clay 3/4 view

Do not give it only cinematic concept art.

## 18. Minimum Success Condition

The blockout is successful when:

- the player can understand the whole ship in one pass
- helm, corridor, engine room, and side nook all read differently
- the interior feels like a small working habitat
- the exterior feels derived from the interior, not randomly wrapped
- the ship looks precious, heavy, and repairable
