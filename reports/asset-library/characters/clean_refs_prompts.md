# Clean refs — Prompt ChatGPT pour générer images sans équipement

## But

Pour chaque personnage, partir de la ref existante (`references/{role}_front.png`) et générer une **version propre sans équipement** (pas de harnais, pas d'outils, pas de lampe, pas de pochettes, pas de galons), à utiliser ensuite comme input image-to-3d Meshy.

## Workflow

Pour chacun des 4 personnages (Crew, Captain, Engineer, Concierge) :

1. Ouvrir ChatGPT (web, modèle GPT-4o avec image-gen)
2. Joindre l'image `references/{role}_front.png` comme référence
3. Coller le prompt ci-dessous
4. Itérer si nécessaire (peut prendre 2-3 essais pour avoir une T-pose stricte + zéro équipement)
5. Sauvegarder l'image générée → `references/{role}_clean.png`
6. Quand les 4 `_clean.png` sont prêts → me dire "go", je relance Meshy

## Prompt générique (à utiliser tel quel pour les 4)

```
Regenerate this character as a clean version with NO equipment, keeping the exact same face, hairstyle, age, body, and base outfit colors.

KEEP:
- Same face, same eyes, same skin tone
- Same hairstyle and hair color
- Same facial hair (mustache if present)
- Same age and body build
- Same coverall outfit color and collar style
- Same boots
- Same knee patches
- Same overall art style (stylized PBR low-poly between Deep Rock Galactic and Team Fortress 2)

REMOVE entirely:
- All harness, straps, suspenders, X-belts
- All tool pouches, ammo pouches, hip pouches, thigh pouches
- All flashlights, keys, ratchets, wrenches, tools
- All rags or cloths hanging from belt
- Hat, beanie, helmet, headgear
- Yellow or gold rank stripes on arms
- Belt buckle attachments
- Anything hanging from belt or thighs
- Backpack, bag, satchel
- Goggles, glasses (unless explicitly required for character identity)

KEEP the belt itself (plain, empty, with simple buckle).
KEEP the chest pockets (closed, empty).

OUTPUT:
- Single male character, front view, strict T-pose (arms horizontal at shoulder height, palms down, fingers relaxed)
- Plain neutral grey background #808080
- Soft frontal lighting, no harsh shadows, no rim light
- Full body visible from top of head to 10cm below feet
- Centered in frame, even margins left/right
- High quality, 1024x1024 or higher
- Photorealistic stylized rendering matching the reference
```

## Checklist par perso (à vérifier sur chaque image générée)

### Captain (`captain_clean.png`)

- [ ] Visage 55-60 ans, mâchoire forte, gris arrière coiffé, rasé
- [ ] Combinaison **navy** `#1A2238`
- [ ] Col droit avec passepoil doré
- [ ] **PAS** de galons sur bras D
- [ ] **PAS** de lampe à la ceinture
- [ ] **PAS** de pochette ni clés
- [ ] Ceinture cuir noir avec boucle laiton (vide)
- [ ] Genouillères octogonales
- [ ] Bottes cuir noir mi-mollet

### Engineer / Mécano (`engineer_clean.png`)

- [ ] Visage 45-50 ans, build compact musclé
- [ ] **Moustache noire épaisse** préservée
- [ ] Cheveux noirs courts (pas de bonnet)
- [ ] Combinaison **charcoal** `#3A3D40`
- [ ] Col passepoil jaune `#C9A646`
- [ ] **PAS** de bonnet
- [ ] **PAS** de harnais X
- [ ] **PAS** de bandes jaunes sur bras D
- [ ] **PAS** de kit outils poche poitrine
- [ ] **PAS** de porte-outils cuisse D
- [ ] **PAS** de chiffon
- [ ] **PAS** de pochette G
- [ ] Ceinture sangle olive (vide, simple boucle)
- [ ] Genouillères octogonales
- [ ] Bottes caoutchouc noires

### Concierge / Artisan (`concierge_clean.png`)

- [ ] Visage 50-55 ans, long, joues marquées
- [ ] Gris poivre-sel coiffé arrière
- [ ] Combinaison **charcoal** `#3A3D40`
- [ ] Col passepoil jaune `#C9A646`
- [ ] **PAS** de harnais Y
- [ ] **PAS** de bande jaune bras D
- [ ] **PAS** de pochette+clés G
- [ ] **PAS** de lampe ni chiffon D
- [ ] Ceinture sangle olive (vide)
- [ ] Genouillères octogonales
- [ ] Bottes caoutchouc noires

### Crew (`crew_clean.png`)

- [ ] Visage 25 ans, frais, build athlétique
- [ ] Cheveux courts (couleur libre, default brun)
- [ ] Rasé
- [ ] Combinaison **charcoal** `#3A3D40`
- [ ] Col passepoil jaune `#C9A646`
- [ ] **PAS** d'aucun équipement
- [ ] Ceinture sangle olive (vide)
- [ ] Genouillères octogonales
- [ ] Bottes caoutchouc noires

## Astuces ChatGPT

- Si la T-pose n'est pas stricte (bras affaissés) : ajouter `"strict T-pose, arms perfectly horizontal at shoulder height, NOT relaxed pose, NOT A-pose"`
- Si des outils reviennent : régénérer avec `"absolutely NO equipment of any kind, plain coverall only, completely empty hands and belt"`
- Si le style dérive cartoon : ajouter `"realistic stylized rendering, NOT cartoon, NOT anime, photorealistic textures with stylized proportions"`
- Si le fond n'est pas uni : `"flat solid neutral grey background only, no gradient, no scenery"`
