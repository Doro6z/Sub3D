// Sub3D Asset Library — source of truth
// Loaded via <script> tag in index.html. Format = JSON-compatible JS object literal.
// Export future : peut être parsé en JSON pour génération de UE data tables.
//
// Convention prompt sectioned : PROPORTIONS / SKIN/MATERIALS / DETAIL / HANDS / TECHNICAL
// Adopted from user template 2026-05-09 — char_body_male sets the canonical style for all character work.

window.SUB3D_ASSETS = {
  schema_version: "1.0",
  last_updated: "2026-05-09",
  items: [
    {
      id: "char_body_male",
      category: "character",
      name: "Base Body (Male, worker)",
      name_fr: "Corps de base (homme, ouvrier)",
      scale: {
        height_cm: 178,
        proportion_ratio: "1:6.5",
        fits_through_door: true
      },
      tags: ["character", "base_body", "male", "worker", "T-pose", "MASTER"],
      materials: ["thermal_undershirt_dark_grey_1A2028", "skin_weathered_dark"],
      concept_prompt: `human base body mesh, T-pose, male, game ready,
NO clothes, NO suit, NO harness, NO boots, NO gloves,
minimal thermal base layer only — dark grey #1A2028
skintight compression undershirt and shorts,
visible at neck wrists and ankles only where suit would end,

PROPORTIONS
slightly heroic build, head-to-body ratio 1:6.5,
broad shoulders, thick neck, solid chest,
hands 10% oversized for FPS readability,
compact muscular natural worker body, not gym-muscular,
aged worker physique, 40-50 years, natural body fat distribution,

SKIN
weathered dark skin with visible fatigue,
subtle skin texture, pores visible at face and hands,
roughness 0.72-0.80 on skin,
metallic 0.0 on all skin surfaces,

FACE
aged submarine worker, 45-55 years,
strong jaw, deep-set tired eyes,
short grey stubble beard, weathered creases,
character and life experience visible,
NOT generic, NOT young, NOT idealized,

HANDS
slightly oversized for FPS readability,
knuckle detail visible, veins subtle,
worker hands — calloused, not clean,

TECHNICAL
stylized low poly geometry, sharp defined edge loops,
3500 triangles maximum total,
clean topology for clothing overlay — no geo interpenetration risk,
UE5 Epic skeleton bone naming convention,
socket points at neck wrists ankles for clothing attachment,
PBR textures albedo roughness metallic normal,
single 2048x2048 texture atlas,
neutral grey gradient background,
FBX export format, Unreal Engine 5 compatible`,
      meshy_3d_prompt: `Aged male submarine worker base body, T-pose, 1.78m, head:body ratio 1:6.5 slightly stylized, broad shoulders thick neck solid chest compact muscular worker NOT gym-muscular, 40-55 years natural body fat, hands +10% FPS-oversized calloused, weathered dark skin fatigue visible, minimal thermal undershirt dark grey #1A2028 visible at neck wrists ankles, stylized low-poly sharp edge loops, 3500 tris max, UE5 Epic skeleton bone naming, sockets neck wrists ankles, single 2048x2048 PBR atlas (albedo+roughness+metallic+normal), FBX export`,
      ui_request: null,
      status: "draft",
      references: ["crew_lineup", "user_template_2026_05_09"],
      notes: "MASTER BASE — toutes les têtes (char_head_*) et clothings (wear_*) attachent via les sockets neck/wrists/ankles. Reusable pour tous les archétypes male. Prompt format = source de vérité pour le style overall (sectioned format adopted)."
    },
    {
      id: "char_head_pilot_male",
      category: "character",
      name: "Pilot Head (Male, weathered worker)",
      name_fr: "Tête pilote (homme, ouvrier marqué)",
      scale: {
        height_cm: 24,
        two_handed: false,
        fits_through_door: true
      },
      tags: ["character", "head", "male", "worker", "aged_45_55"],
      materials: ["skin_weathered_dark", "grey_stubble"],
      concept_prompt: `male submarine pilot head, isolated head only with neck stub for socket attachment,
3-view sheet (front + 3/4 side + back), neutral grey gradient background,

PROPORTIONS
head:body ratio 1:6.5 compatible with char_body_male base,
strong jaw, thick neck attachment matching base body socket,
deep-set tired eyes,
character and life experience visible,
NOT generic, NOT young, NOT idealized,

SKIN
weathered dark skin with visible fatigue,
subtle skin texture, pores visible,
roughness 0.72-0.80, metallic 0.0,
slight tan-line where headset would sit on forehead and ears,

FACE DETAIL
aged submarine pilot, 45-55 years,
short grey stubble beard, salt-and-pepper short hair,
weathered creases at eyes and forehead,
slightly tired but determined expression,
worn imprint on bridge of nose from goggles/headset,

TECHNICAL
stylized low poly geometry, sharp defined edge loops,
~2500 triangles head only,
clean topology compatible with morph targets (jaw_width, brow, cheekbones, weight, age),
neck socket attachment matching char_body_male,
single 2048x2048 atlas, PBR albedo+roughness+metallic+normal,
neutral grey gradient background,
UE5 game-ready, FBX export`,
      meshy_3d_prompt: `Weathered male submarine pilot head, 45-55 years aged worker, strong jaw thick neck socket, deep-set tired eyes, short grey stubble salt-and-pepper hair, weathered creases, headset wear imprint on forehead/nose, low-poly stylized sharp edge loops, ~2500 tris head only, UE5 Epic skeleton compatible neck socket, single 2048x2048 PBR atlas`,
      ui_request: null,
      status: "draft",
      references: ["pilot_archetype", "crew_lineup", "char_body_male"],
      notes: "Attaches to char_body_male via neck socket. Base for morph targets — Blender step post-Meshy. 4-5 distinct heads total target FP, all 40-55 weathered workers default."
    },
    {
      id: "tool_pipe_wrench",
      category: "held_tool",
      name: "Pipe Wrench",
      name_fr: "Clé à pipe",
      scale: {
        length_cm: 35,
        weight_g: 1200,
        two_handed: false,
        fits_through_door: true
      },
      tags: ["repair", "metal", "rust_patina", "industrial"],
      materials: ["forged_steel", "rubber_grip", "rouge_minium_paint"],
      concept_prompt: `industrial pipe wrench, isolated still life on neutral grey #4A4E52 background,
single warm tungsten 2700K key light from upper-left, soft shadows,

PROPORTIONS
35cm length proportional to a human hand grip,
adjustable jaw at one end, rubber-wrapped handle at the other,
NOT oversized, NOT cartoon-large,

MATERIALS
forged steel head with weathered patina,
rouge minium #80363F paint patches partially worn off,
black rubber-wrapped handle with grip texture and wear,
roughness 0.55-0.75, metallic 0.85 on steel,

DETAIL
visible jaw teeth, threaded adjustment screw,
small dings and chips at jaw corners,
hand-painted PBR texture style with painterly weathering,

TECHNICAL
stylized low poly geometry, sharp defined edge loops,
~3000 triangles max,
single 1024x1024 PBR atlas (albedo+roughness+metallic+normal),
hand-painted PBR style — painterly brush feel, NOT photorealistic,
NOT cartoon, palette aligned with Sub3D canonical color sheet,
UE5 game-ready, FBX export`,
      meshy_3d_prompt: `Industrial pipe wrench with adjustable jaw and threaded screw, weathered forged steel patina, rouge minium paint patches partially worn, black rubber grip with wear, 35cm, low-poly stylized sharp edge loops ~3000 tris, hand-painted PBR style, single 1024 atlas`,
      ui_request: {
        panel_id: "wrench_repair_overlay",
        priority: 60,
        fade_in_ms: 200,
        fade_out_ms: 300
      },
      status: "draft",
      references: ["color_sheet", "engineer_archetype"],
      notes: "Held by Engineer archetype (cf. crew lineup canonique). Used for valve repair interactions."
    },
    {
      id: "wear_helmet_engineer",
      category: "wearable",
      name: "Engineer Hard Hat",
      name_fr: "Casque ingénieur",
      scale: {
        length_cm: 28,
        weight_g: 600,
        two_handed: false,
        fits_through_door: true
      },
      tags: ["wearable", "head", "industrial", "yellow_safety"],
      materials: ["yellow_polymer", "leather_strap"],
      concept_prompt: `industrial submarine engineer hard hat, isolated on neutral dark background,
3/4 front view, soft tungsten 2700K key light + cool fill,

PROPORTIONS
fits over char_body_male head, neck socket compatible,
classic hard hat shape with brim,
front-mounted headlamp socket (light off),

MATERIALS
jaune sécurité #C8AA45 polymer with weathering and small dings,
brown leather chinstrap with brass buckle,
roughness 0.45-0.65, metallic 0.0,

DETAIL
edge wear and paint chips revealing darker substrate underneath,
small stenciled name plate on side (worn rouge minium),
visible tightening dial at back,
hand-painted PBR texture style with painterly weathering,

TECHNICAL
stylized low poly geometry, sharp defined edge loops,
~2000 triangles max,
single 1024x1024 PBR atlas,
hand-painted PBR style, NOT photoreal NOT cartoon,
palette aligned with Sub3D canonical color sheet,
UE5 game-ready, FBX export`,
      meshy_3d_prompt: `Industrial yellow #C8AA45 hard hat with brown leather chinstrap and front-mounted headlamp socket, weathered safety yellow polymer with paint chips, low-poly stylized sharp edge loops ~2000 tris, hand-painted PBR style, single 1024 atlas`,
      ui_request: null,
      status: "draft",
      references: ["color_sheet", "engineer_archetype"],
      notes: "Worn by Engineer archetype. Headlamp socket prepared, light-emitting flagged but post-FP."
    },
    {
      id: "dress_crate_small",
      category: "set_dressing",
      name: "Small Storage Crate",
      name_fr: "Petite caisse de stockage",
      scale: {
        length_cm: 50,
        width_cm: 35,
        height_cm: 30,
        weight_g: 4000,
        two_handed: true,
        fits_through_door: true
      },
      tags: ["set_dressing", "storage", "wood", "metal_corners"],
      materials: ["weathered_pine", "iron_corners", "stenciled_paint"],
      concept_prompt: `naval submarine storage crate, small size 50x35x30cm,
3/4 view isolated on grey #4A4E52 background,
soft top-down tungsten 2700K lighting,

MATERIALS
weathered pine wood planks #6B4F32 with visible grain,
iron-reinforced corners with rivets and rust patina,
rouge minium #80363F stenciled markings (partial, faded),
roughness 0.65-0.85, metallic 0.0 wood / 0.7 iron corners,

DETAIL
slightly battered with wear at edges,
nail heads visible on planks,
hand-painted PBR texture style with painterly weathering,
small handles or rope grip on sides,

TECHNICAL
stylized low poly geometry, sharp defined edge loops,
~1500 triangles max,
single 1024x1024 PBR atlas,
hand-painted PBR style, NOT photoreal NOT cartoon,
palette aligned with Sub3D canonical color sheet,
UE5 game-ready, FBX export`,
      meshy_3d_prompt: `Naval submarine storage crate 50x35x30cm, weathered pine planks with iron-reinforced corners and rust patina, rouge minium stenciled markings partial faded, low-poly stylized sharp edge loops ~1500 tris, hand-painted PBR style, single 1024 atlas`,
      ui_request: null,
      status: "draft",
      references: ["color_sheet"],
      notes: "Stackable. Three sizes total (small/medium/large), this is small."
    },
    {
      id: "fix_valve_handwheel_M",
      category: "sub_fixture",
      name: "Brass Valve Handwheel (M)",
      name_fr: "Volant de valve laiton (M)",
      scale: {
        diameter_cm: 25,
        depth_cm: 8,
        weight_g: 800,
        two_handed: false,
        fits_through_door: true
      },
      tags: ["fixture", "interactive", "valve", "brass"],
      materials: ["brass_patina", "iron_stem"],
      concept_prompt: `submarine valve handwheel, medium size 25cm diameter,
front view + 3/4 side view, isolated on neutral background,
warm tungsten 2700K lighting from above,

PROPORTIONS
six-spoke design, 25cm diameter, 8cm depth,
mounted on iron stem 4cm diameter,
hand grip area on rim ~2cm thick,

MATERIALS
brass #A57212 with green patina in recesses,
iron stem #4A4E52 with rust patches at base,
roughness 0.35-0.55 on brass / 0.65-0.85 on iron,
metallic 0.95 on brass / 0.85 on iron,

DETAIL
worn polish on rim from regular use,
threaded stem visible at base,
hand-painted PBR texture style with painterly weathering,

TECHNICAL
stylized low poly geometry, sharp defined edge loops,
~1500 triangles max,
single 1024x1024 PBR atlas,
hand-painted PBR style, NOT photoreal NOT cartoon,
palette aligned with Sub3D canonical color sheet,
UE5 game-ready, FBX export`,
      meshy_3d_prompt: `Brass submarine valve handwheel 25cm six-spoke with green patina recesses, mounted on iron stem with rust patches, low-poly stylized sharp edge loops ~1500 tris, hand-painted PBR style, single 1024 atlas`,
      ui_request: {
        panel_id: "valve_interaction_prompt",
        priority: 30,
        fade_in_ms: 150,
        fade_out_ms: 200
      },
      status: "draft",
      references: ["color_sheet"],
      notes: "Three sizes (S/M/L) for different pipe diameters. Rotation = closes/opens valve."
    },
    {
      id: "creature_stalker_juvenile",
      category: "abyss_exterior",
      name: "Stalker (juvenile)",
      name_fr: "Stalker (juvénile)",
      scale: {
        length_cm: 220,
        fits_through_door: false,
        exterior_only: true
      },
      tags: ["creature", "hostile", "bioluminescent", "chitin"],
      materials: ["chitin_dark", "bioluminescent_cyan"],
      concept_prompt: `bio-mechanical abyssal predator creature, juvenile size 2.2m,
side profile + top view (2-view sheet), isolated on dark background,
key light from above with bioluminescent self-emission visible,

PROPORTIONS
juvenile size 220cm length,
segmented body 4 main sections + tail,
articulated scythe-limbs tucked under body,
flat stingray-like tail,
mega-mouth currently closed,

MATERIALS
segmented dark chitin armor plates over organic flesh underlayer,
cyan #69E5F0 bioluminescent veins running through body and limbs,
roughness 0.55-0.75 on chitin / 0.30-0.45 on flesh,
metallic 0.0,
emissive cyan map driving the veins (intensity 2-3),

DETAIL
chitin plates with subtle ridges and predatory texture,
flesh underlayer visible at joints and mouth area,
needle teeth visible if mouth opens (closed in this concept),
hand-painted PBR texture style with painterly chitin and emissive veins,

TECHNICAL
stylized low poly geometry, sharp defined edge loops,
~5000 triangles max,
single 2048x2048 PBR atlas with emissive map,
hand-painted PBR style, NOT photoreal NOT cartoon,
matches Sub3D Stalker design concept reference (Mobs/Stalker (1).png),
UE5 game-ready, FBX export`,
      meshy_3d_prompt: `Bio-mechanical abyssal creature juvenile 2.2m, segmented dark chitin armor plates with cyan bioluminescent veins, articulated scythe-limbs tucked, flat stingray tail, low-poly stylized sharp edge loops ~5000 tris, hand-painted PBR with emissive cyan map, single 2048 atlas`,
      ui_request: null,
      status: "draft",
      references: ["stalker_concept"],
      notes: "Juvenile = scope FP. Adult version post-FP. Reference: Mobs/Stalker (1).png annotated design sheet."
    }
  ]
};
