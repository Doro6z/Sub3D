# Sub3D — UI Design Pillars

Référence persistante pour toute session UI. Ces piliers ne changent pas sans décision explicite.

---

## Méthode de collaboration UI

1. **Vérifier `reports/ui-references/`** avant de commencer. Si un fichier de référence visuelle existe pour le widget cible, le lire.
2. **Phase A avant Phase B** — proposer un mockup ASCII ou description structurée, attendre validation, PUIS écrire le C++/Slate/UMG.
3. **Respecter ces piliers** — ils overrident les préférences par défaut d'UMG ou les patterns génériques.

---

## Piliers visuels

### Lisibilité instrumentale
Chaque widget d'instrument (telegraph, yoke, dive, kill) est lisible en un coup d'œil depuis la position cockpit. Pas de texte, indicateurs visuels positionnels.

### Pas d'auto-spawn UI
Widgets editor-assigned. Noms stables. Pas de `CreateWidget` dans des chemins de tick ou de `BeginPlay` conditionnels.

### Debug = toggleable, pas permanent
Overlays debug via `USub3DDebugSettings`. Jamais hardcodés dans les widgets de production.

---

## Références visuelles

Placer dans `reports/ui-references/` :
- Screenshots de références (jeux, interfaces nautiques réelles)
- Mockups ASCII validés
- Notes de session sur les choix UI retenus

Nommer les fichiers : `[date]_[widget-name]_[type].md` ou `.png`

---

## Widgets existants (2026-04-25)

| Widget | Fichier | Statut |
|---|---|---|
| UHelmCockpitWidget | `Source/Sub3D/Submarine/Helm/SubHelmCockpitWidget.*` | Actif, PIE-validé |
| UTelegraphWidget | `Source/Sub3D/Submarine/Helm/` | Actif |
| UYokeWidget | `Source/Sub3D/Submarine/Helm/` | Actif |
| UDiveControlWidget | `Source/Sub3D/Submarine/Helm/` | Actif |
| UKillSwitchWidget | `Source/Sub3D/Submarine/Helm/` | Actif |
