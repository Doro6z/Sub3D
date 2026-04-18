# Freelance Brief — Submarine Interior Blockout

**Project:** Sub3D — Submarine Simulation Game (Unreal Engine 5.7)
**Asset type:** Low-poly blockout mesh for gameplay prototyping
**Budget:** 50–150 EUR (negotiable)
**Timeline:** 3–5 days, 2 revision rounds
**Language:** English or French

---

## 1. Project Overview

Sub3D is an indie submarine simulation game. The player controls a crew inside a submarine: navigating compartments, managing flooding, repairing hull breaches, and operating stations (helm, engine, ballast).

We need a **low-poly blockout mesh** of the submarine for our First Playable milestone. This is NOT final art — it's a structural blockout for gameplay validation. Clean geometry, no detail props.

---

## 2. Deliverables

1. **Outer hull** — single closed mesh, smooth exterior, 15cm wall thickness (solidify inward). Organic cigar shape with tapered bow/stern. The hull shape adapts to the compartments inside (wider where there are more decks).
2. **3 deck plates** — horizontal floors (upper, main, lower). Continuous, no holes. Variable width per deck level.
3. **Bulkheads** — vertical walls between compartments, with rectangular door openings (90×185cm). One template, duplicated per boundary.
4. **10 compartments** — interior box-rooms forming the submarine layout. Each has walls, floor, ceiling, and door openings.
5. **1 SAS/Airlock** — small room on top of upper deck (bridge area). Vertical access only, open face for exterior hatch.

### File format

- FBX export, **centimeters**, X-forward Y-lateral Z-up
- Each major element as a **separate object** (hull, each deck, each compartment, each bulkhead)
- Blender source file (.blend) included
- Clean topology, quads preferred, normals consistent

---

## 3. Dimensions

| Parameter | Value |
|---|---|
| **Total length** | ~44 meters (4400 cm) |
| **Max hull diameter** | ~8 meters (800 cm) |
| **Hull wall thickness** | 15 cm (solidify inward) |
| **Deck plate thickness** | 18 cm |
| **Bulkhead thickness** | 14 cm |
| **Standard door opening** | 90 cm wide × 185 cm tall |
| **Number of decks** | 3 (upper, main, lower) |
| **Number of compartments** | 10 + 1 SAS |

### Layout (side view)

```
                      ┌─────────┐
                      │   SAS   │               <- Top
                ┌─────┴─────────┴─────┐
                │  NAV  │   BRIDGE    │         <- Upper deck
  ┌─────┬───────┼───────┼─────────────┼─────┬──────┐
  │TORP │ SONAR │ CREW  │   MEDBAY   │ ENG │ PROP │  <- Main deck
  └─────┴───────┼───────┼─────────────┼─────┴──────┘
                │ MACH  │  REACTOR   │         <- Lower deck
                └───────┴─────────────┘
```

### Compartment dimensions

| Name | Deck | Length (cm) | Width (cm) | Height (cm) | Doors |
|---|---|---|---|---|---|
| Torpedo | Main | 400 | 280 | 200 | Aft only |
| Sonar | Main | 350 | 360 | 200 | Fore + Aft |
| CrewQuarters | Main | 500 | 480 | 200 | Fore + Aft |
| MedBay | Main | 400 | 480 | 200 | Fore + Aft |
| Engine | Main | 400 | 400 | 200 | Fore + Aft |
| Propulsion | Main | 350 | 300 | 200 | Fore only |
| Navigation | Upper | 500 | 380 | 195 | Fore + Aft |
| Bridge | Upper | 400 | 400 | 195 | Fore + Aft |
| Machinery | Lower | 500 | 420 | 180 | Fore + Aft |
| Reactor | Lower | 400 | 440 | 180 | Fore + Aft |
| SAS | Top | 250 | 220 | 210 | None (vertical access) |

---

## 4. Visual Style & References

This is a blockout. We need:

- Clean, readable geometry — player must understand the space instantly
- Flat shading / simple materials (one color per compartment is fine)
- No UV mapping needed (materials applied in UE)
- No interior props or furniture
- Hull shape inspired by WW2 French submarines (Surcouf class)

### Reference submarines

- **Surcouf (1929)** — elongated hull, prominent sail, multi-deck interior
- **Barotrauma (game)** — modular compartments, clear deck separation, gameplay-first layout
- **Generic diesel-electric** — cigar body, tapered bow, stubby stern

---

## 5. Technical Constraints (non-negotiable)

These dimensions are driven by gameplay code:

- **Door openings:** exactly 90cm wide × 185cm tall, centered on wall, sill at floor level
- **Standing height:** minimum 186cm floor-to-ceiling in every compartment
- **Deck plates:** must be SOLID — no holes for hatches (those are separate game actors)
- **Hull:** one continuous mesh, solidified inward. Player sees the interior surface.
- **Each compartment:** separate Blender object for individual export
- **Coordinate system:** X = forward (bow), Y = lateral, Z = up. Origin at submarine center.
- **Scale:** 1 Blender unit = 1 centimeter

---

## 6. What is NOT needed

- No textures or UV maps
- No interior props (chairs, consoles, pipes, etc.)
- No rigging or animation
- No LODs
- No exterior details (periscope, rudder, propeller)
- No collision meshes (generated in-engine)

---

## 7. What We Provide

- **Blender script** that generates the current blockout (starting point / reference)
- **Reference images** (Surcouf, Barotrauma, technical drawings)
- **This brief** with all dimensions
- Direct communication for feedback during modeling

---

## 8. Budget & Timeline

| | |
|---|---|
| **Budget** | 50–150 EUR (negotiable based on quality) |
| **Timeline** | 3–5 days |
| **Revisions** | 2 rounds included |
| **Communication** | English or French |
| **Payment** | Via Fiverr/Upwork escrow |

---

*Contact via Fiverr/Upwork messaging. Blender scripts and reference images provided upon acceptance.*
