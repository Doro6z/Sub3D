from __future__ import annotations

from pathlib import Path
from xml.sax.saxutils import escape


OUT_DIR = Path(__file__).resolve().parent / "20260325-proto03-pack-illustre-assets"

W = 1800
H = 1280

PALETTE = {
    "bg": "#0f1419",
    "panel": "#172028",
    "panel_alt": "#131b22",
    "stroke": "#43525d",
    "soft": "#34414a",
    "ink": "#f4efe6",
    "muted": "#d1c9b8",
    "nav": "#d4a93a",
    "nav_dark": "#4a3b17",
    "ball": "#66b7c8",
    "ball_dark": "#274854",
    "prop": "#5a95dc",
    "prop_dark": "#233c61",
    "conf": "#c66e3a",
    "conf_dark": "#4f3022",
    "sense": "#bb8638",
    "sense_dark": "#4a341a",
    "danger": "#de5a49",
    "ok": "#82c97b",
    "warn": "#e3b65b",
}

GRID = {
    "outer_l": 90,
    "outer_r": 1154,
    "inner_l": 190,
    "b1": 400,
    "b2": 610,
    "b3": 848,
    "inner_r": 1058,
}


def esc(value: str) -> str:
    return escape(value, {"'": "&apos;", '"': "&quot;"})


def text_block(x: int, y: int, lines: list[str], cls: str = "text", line_h: int = 28, anchor: str = "start") -> str:
    out = [f'<text class="{cls}" x="{x}" y="{y}" text-anchor="{anchor}">']
    for index, line in enumerate(lines):
        dy = 0 if index == 0 else line_h
        out.append(f'<tspan x="{x}" dy="{dy}">{esc(line)}</tspan>')
    out.append("</text>")
    return "".join(out)


def rect(x: int, y: int, w: int, h: int, cls: str, rx: int = 0, extra: str = "") -> str:
    return f'<rect class="{cls}" x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" {extra}/>'


def line(x1: int, y1: int, x2: int, y2: int, cls: str, extra: str = "") -> str:
    return f'<line class="{cls}" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" {extra}/>'


def circle(cx: int, cy: int, r: int, cls: str, extra: str = "") -> str:
    return f'<circle class="{cls}" cx="{cx}" cy="{cy}" r="{r}" {extra}/>'


def path(d: str, cls: str, extra: str = "") -> str:
    return f'<path class="{cls}" d="{d}" {extra}/>'


def polygon(points: str, cls: str, extra: str = "") -> str:
    return f'<polygon class="{cls}" points="{points}" {extra}/>'


def base_style() -> str:
    return f"""
  <defs>
    <marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
      <path d="M 0 0 L 10 5 L 0 10 z" fill="{PALETTE["ink"]}"/>
    </marker>
    <style><![CDATA[
      .bg {{ fill: {PALETTE["bg"]}; }}
      .panel {{ fill: {PALETTE["panel"]}; stroke: {PALETTE["stroke"]}; stroke-width: 2; }}
      .panelAlt {{ fill: {PALETTE["panel_alt"]}; stroke: {PALETTE["soft"]}; stroke-width: 2; }}
      .frame {{ fill: none; stroke: #9ca8af; stroke-width: 4; }}
      .grid {{ fill: none; stroke: {PALETTE["soft"]}; stroke-width: 1; stroke-dasharray: 6 8; opacity: 0.45; }}
      .bulkhead {{ stroke: #98a4ad; stroke-width: 8; }}
      .doorGap {{ stroke: {PALETTE["bg"]}; stroke-width: 14; }}
      .repair {{ fill: none; stroke: #ead59b; stroke-width: 4; stroke-dasharray: 12 10; }}
      .pathBand {{ fill: #f0eadb; opacity: 0.12; stroke: {PALETTE["ink"]}; stroke-width: 2; }}
      .dim {{ fill: none; stroke: {PALETTE["ink"]}; stroke-width: 2.2; }}
      .danger {{ fill: {PALETTE["danger"]}; }}
      .ok {{ fill: {PALETTE["ok"]}; }}
      .warn {{ fill: {PALETTE["warn"]}; }}
      .title {{ fill: {PALETTE["ink"]}; font: 700 42px Arial, 'Segoe UI', sans-serif; }}
      .subtitle {{ fill: {PALETTE["muted"]}; font: 19px Arial, 'Segoe UI', sans-serif; }}
      .h1 {{ fill: {PALETTE["ink"]}; font: 700 28px Arial, 'Segoe UI', sans-serif; }}
      .h2 {{ fill: {PALETTE["ink"]}; font: 700 22px Arial, 'Segoe UI', sans-serif; }}
      .text {{ fill: {PALETTE["ink"]}; font: 19px Arial, 'Segoe UI', sans-serif; }}
      .small {{ fill: {PALETTE["muted"]}; font: 17px Arial, 'Segoe UI', sans-serif; }}
      .mono {{ fill: {PALETTE["ink"]}; font: 17px Consolas, 'Courier New', monospace; }}
      .navFill {{ fill: {PALETTE["nav_dark"]}; stroke: {PALETTE["nav"]}; stroke-width: 2.5; }}
      .ballFill {{ fill: {PALETTE["ball_dark"]}; stroke: {PALETTE["ball"]}; stroke-width: 2.5; }}
      .propFill {{ fill: {PALETTE["prop_dark"]}; stroke: {PALETTE["prop"]}; stroke-width: 2.5; }}
      .confFill {{ fill: {PALETTE["conf_dark"]}; stroke: {PALETTE["conf"]}; stroke-width: 2.5; }}
      .senseFill {{ fill: {PALETTE["sense_dark"]}; stroke: {PALETTE["sense"]}; stroke-width: 2.5; }}
      .navSolid {{ fill: {PALETTE["nav"]}; }}
      .ballSolid {{ fill: {PALETTE["ball"]}; }}
      .propSolid {{ fill: {PALETTE["prop"]}; }}
      .confSolid {{ fill: {PALETTE["conf"]}; }}
      .senseSolid {{ fill: {PALETTE["sense"]}; }}
      .whiteStroke {{ fill: none; stroke: {PALETTE["ink"]}; stroke-width: 3; }}
      .tank {{ fill: {PALETTE["ball"]}; stroke: #d8f3fb; stroke-width: 2; }}
      .tag {{ fill: #e7d8a2; }}
      .tag2 {{ fill: #6a7c86; }}
      .tagText {{ fill: {PALETTE["bg"]}; font: 700 16px Arial, 'Segoe UI', sans-serif; }}
      .tableLine {{ stroke: {PALETTE["soft"]}; stroke-width: 2; }}
    ]]></style>
  </defs>
"""


def svg_doc(title: str, desc: str, body: str, width: int = W, height: int = H) -> str:
    return f"""<svg viewBox="0 0 {width} {height}" xmlns="http://www.w3.org/2000/svg" role="img" aria-labelledby="title desc">
  <title id="title">{esc(title)}</title>
  <desc id="desc">{esc(desc)}</desc>
{base_style()}
  <rect class="bg" x="0" y="0" width="{width}" height="{height}"/>
{body}
</svg>
"""


def header(title: str, subtitle: str) -> str:
    return (
        f'<text class="title" x="60" y="62">{esc(title)}</text>'
        f'<text class="subtitle" x="60" y="98">{esc(subtitle)}</text>'
    )


def hull_top(y_top: int, y_bottom: int) -> str:
    return path(
        f"M{GRID['outer_l'] + 30} {y_top} "
        f"Q{GRID['outer_l']} {y_top} {GRID['outer_l'] - 16} {y_top + 38} "
        f"L{GRID['outer_l'] - 30} {y_top + 88} "
        f"L{GRID['outer_l'] - 30} {y_bottom - 88} "
        f"L{GRID['outer_l'] - 16} {y_bottom - 38} "
        f"Q{GRID['outer_l']} {y_bottom} {GRID['outer_l'] + 30} {y_bottom} "
        f"L{GRID['outer_r'] - 40} {y_bottom} "
        f"Q{GRID['outer_r']} {y_bottom} {GRID['outer_r'] + 18} {y_bottom - 48} "
        f"L{GRID['outer_r'] + 30} {y_bottom - 96} "
        f"L{GRID['outer_r'] + 30} {y_top + 96} "
        f"L{GRID['outer_r'] + 18} {y_top + 48} "
        f"Q{GRID['outer_r']} {y_top} {GRID['outer_r'] - 40} {y_top} Z",
        "frame",
    )


def hull_side(y_mid: int) -> str:
    top = y_mid - 92
    bottom = y_mid + 90
    return path(
        f"M{GRID['outer_l'] + 45} {bottom} "
        f"Q{GRID['outer_l']} {bottom} {GRID['outer_l'] - 18} {bottom - 36} "
        f"Q{GRID['outer_l'] - 36} {y_mid} {GRID['outer_l'] - 10} {top + 30} "
        f"Q{GRID['outer_l'] + 16} {top} {GRID['outer_l'] + 70} {top} "
        f"L{GRID['outer_r'] - 70} {top} "
        f"Q{GRID['outer_r'] + 6} {top + 4} {GRID['outer_r'] + 34} {y_mid - 22} "
        f"Q{GRID['outer_r'] + 56} {y_mid} {GRID['outer_r'] + 18} {bottom - 24} "
        f"Q{GRID['outer_r'] - 4} {bottom} {GRID['outer_r'] - 64} {bottom} Z",
        "frame",
    )


def bulkheads(y1: int, y2: int) -> str:
    parts = []
    for x in (GRID["b1"], GRID["b2"], GRID["b3"]):
        parts.append(line(x, y1, x, y2, "bulkhead"))
        parts.append(line(x, y1 + 86, x, y1 + 152, "doorGap"))
    return "".join(parts)


def deck_grid(y1: int, y2: int) -> str:
    parts = []
    for x in (GRID["inner_l"], GRID["b1"], GRID["b2"], GRID["b3"], GRID["inner_r"]):
        parts.append(line(x, y1, x, y2, "grid"))
    return "".join(parts)


def plate_02() -> str:
    body = [header("Planche 2 - Kit d'assets par systeme", "Inventaire de production du premier sous-marin. On raisonne en familles fonctionnelles et en dependances.")]
    col_x = [60, 406, 752, 1098, 1444]
    widths = [310, 310, 310, 310, 296]
    titles = [
        ("Commande / navigation", "navSolid"),
        ("Ballast / pompage", "ballSolid"),
        ("Propulsion / energie", "propSolid"),
        ("Confinement / reparation", "confSolid"),
        ("Combat / senseur", "senseSolid"),
    ]
    cards = [
        [("P1", "SM_HelmConsole_A"), ("P1", "SM_CommandDesk_A"), ("P1", "SM_RadarConsole_A"), ("P2", "SM_NavRepeater_A"), ("P2", "SM_ChartShelf_A")],
        [("P1", "SM_BallastForeTank_A"), ("P1", "SM_BallastRearTank_A"), ("P1", "SM_BallastConsole_A"), ("P1", "SM_PumpModule_A"), ("P1", "SM_ValveManifold_A")],
        [("P1", "SM_EngineBlock_A"), ("P1", "SM_PowerCabinet_A"), ("P2", "SM_ShaftHousing_A"), ("P2", "SM_CableTray_A"), ("P2", "SM_BatteryRack_A")],
        [("P1", "SM_BulkheadFrame_A"), ("P1", "SM_BulkheadDoor_A"), ("P1", "SM_RepairWallPanel_A"), ("P1", "SM_HatchTrunk_A"), ("P2", "SM_CompartmentSign_A")],
        [("P1", "SM_RadarMast_A"), ("P1", "SM_TurretBase_A"), ("P2", "SM_TurretControl_A"), ("P2", "SM_SensorCabinet_A"), ("P2", "SM_AntennaCluster_A")],
    ]
    notes = [
        ["Role : noeud lisible, frontal, visible a 10 m.", "Ne jamais noyer le helm dans des props lateraux."],
        ["Role : faire lire le flux et la masse d'eau.", "Les deux ballasts doivent exister comme actifs distincts."],
        ["Role : masse lourde, bruit, maintenance, energie.", "Le moteur se lit par volume, pas par greeble."],
        ["Role : couper la propagation et reserver la reparation.", "Une porte sauve le run ou le condamne."],
        ["Role : point haut, lecture exterieure, extension future.", "Le premier proto garde cette famille tres sobre."],
    ]

    for idx, x in enumerate(col_x):
        w = widths[idx]
        body.append(rect(x, 150, w, 500, "panel", 20))
        body.append(rect(x, 150, w, 62, titles[idx][1], 20))
        body.append(text_block(x + 18, 190, [titles[idx][0]], "h2", 26))
        y = 250
        for tier, asset in cards[idx]:
            pill_cls = "tag" if tier == "P1" else "tag2"
            body.append(rect(x + 18, y - 16, 58, 28, pill_cls, 14))
            body.append(text_block(x + 38, y + 2, [tier], "tagText", 18, "middle"))
            body.append(text_block(x + 92, y + 2, [asset], "text", 24))
            y += 54
        body.append(line(x + 18, 488, x + w - 18, 488, "tableLine"))
        body.append(text_block(x + 18, 538, notes[idx], "small", 24))

    body.append(rect(60, 690, 1680, 520, "panel", 20))
    body.append(text_block(84, 730, ["Ordre reel de fabrication pour un proto jouable"], "h1", 30))
    rows = [
        ("Phase 0", "2 meshes", "SM_HullShell_Ext_A, SM_HullShell_Int_A", "Sans eux, aucune lecture spatiale n'existe."),
        ("Phase 1", "3 meshes", "SM_BulkheadFrame_A, SM_BulkheadDoor_A, SM_HatchTrunk_A", "Le confinement et le sas doivent apparaitre avant les stations."),
        ("Phase 2", "5 meshes", "SM_HelmConsole_A, SM_BallastConsole_A, SM_EngineBlock_A, SM_PumpModule_A, SM_RepairWallPanel_A", "Ce sont les cinq reperes de gameplay obligatoires."),
        ("Phase 3", "8 a 10 meshes", "tuyaux, cables, echelle, rampe, luminaire, coffre, manifolds", "Seulement apres validation du graybox et du test de circulation."),
        ("Phase 4", "variantes d'etat", "nominal, utilisable, endommage, critique, inonde", "Les etats arrivent en dernier, jamais avant la lisibilite de base."),
    ]
    y = 790
    body.append(line(84, 758, 1716, 758, "tableLine"))
    for phase, count, assets, why in rows:
        body.append(text_block(92, y, [phase], "h2", 24))
        body.append(text_block(240, y, [count], "mono", 24))
        body.append(text_block(430, y, [assets], "text", 24))
        body.append(text_block(430, y + 26, [why], "small", 24))
        y += 88
        body.append(line(84, y - 22, 1716, y - 22, "tableLine"))

    body.append(text_block(84, 1180, ["Interdit avant validation :", "petits props decoratifs, tuyaux micro-detail, mobilier gratuit, doubles variantes de consoles, clutter de sol."], "small", 24))
    return svg_doc("Kit d'assets par systeme", "Inventaire de production des assets du premier sous-marin.", "".join(body))


def plate_03() -> str:
    body = [header("Planche 3 - Regles de lisibilite et silhouettes", "Une station doit se reconnaitre en une forme, un axe et une zone d'approche. Pas en accumulation de petits details.")]
    card_specs = [
        (60, 150, "Console de helm", "navSolid", "arc large + base stable", "face frontale forte", "aire libre de 1,2 m min."),
        (470, 150, "Console ballast", "ballSolid", "bloc carre + vannes", "flux et pression visibles", "ballast avant / arriere lisibles."),
        (880, 150, "Bloc moteur", "propSolid", "masse unique lourde", "un flanc de maintenance", "pas de mille petites pieces."),
        (1290, 150, "Porte etanche", "confSolid", "cadre epais + verrou", "decision de confinement", "doit sembler sauver le run."),
        (60, 560, "Panneau de reparation", "confSolid", "mur reserve et clair", "feedback lisible de loin", "jamais enterre sous le decor."),
        (470, 560, "Tronc de sas", "senseSolid", "verticalite + echelle", "point haut exterieur", "premier proto = ecoutille plafond."),
    ]
    for x, y, title, shape_cls, key_line, rule1, rule2 in card_specs:
        body.append(rect(x, y, 350, 330, "panel", 18))
        body.append(text_block(x + 24, y + 42, [title], "h2", 24))
        body.append(text_block(x + 24, y + 78, [rule1, rule2], "small", 22))
        if title == "Console de helm":
            body.append(rect(x + 70, y + 180, 210, 90, "panelAlt", 16))
            body.append(path(f"M{x+84} {y+264} Q{x+175} {y+156} {x+266} {y+264} L{x+266} {y+286} L{x+84} {y+286} Z", shape_cls))
            body.append(rect(x + 94, y + 176, 162, 118, "grid", 14))
        elif title == "Console ballast":
            body.append(rect(x + 90, y + 196, 180, 72, shape_cls, 16))
            body.append(circle(x + 128, y + 232, 18, "whiteStroke"))
            body.append(circle(x + 198, y + 232, 18, "whiteStroke"))
            body.append(path(f"M{x+270} {y+232} C{x+318} {y+232} {x+326} {y+186} {x+340} {y+170}", "whiteStroke"))
        elif title == "Bloc moteur":
            body.append(rect(x + 72, y + 190, 214, 86, shape_cls, 24))
            body.append(circle(x + 128, y + 233, 28, "whiteStroke"))
            body.append(rect(x + 184, y + 204, 92, 58, "whiteStroke", 10))
        elif title == "Porte etanche":
            body.append(rect(x + 96, y + 182, 160, 118, shape_cls, 20))
            body.append(rect(x + 128, y + 202, 96, 78, "whiteStroke", 26))
            body.append(circle(x + 208, y + 241, 12, "whiteStroke"))
        elif title == "Panneau de reparation":
            body.append(rect(x + 114, y + 186, 120, 114, shape_cls, 10))
            body.append(line(x + 144, y + 216, x + 214, y + 216, "whiteStroke"))
            body.append(line(x + 144, y + 248, x + 214, y + 248, "whiteStroke"))
            body.append(line(x + 144, y + 280, x + 194, y + 280, "whiteStroke"))
        else:
            body.append(rect(x + 148, y + 166, 44, 108, shape_cls, 12))
            body.append(circle(x + 170, y + 274, 58, shape_cls))
            body.append(circle(x + 170, y + 274, 76, "whiteStroke"))
            body.append(line(x + 170, y + 184, x + 170, y + 316, "whiteStroke"))
            body.append(line(x + 156, y + 214, x + 184, y + 214, "whiteStroke"))
            body.append(line(x + 156, y + 238, x + 184, y + 238, "whiteStroke"))
        body.append(text_block(x + 24, y + 310, [f"Cle visuelle : {key_line}."], "small", 22))

    body.append(rect(880, 560, 760, 330, "panel", 18))
    body.append(text_block(904, 602, ["Regles de lecture obligatoires"], "h1", 30))
    body.append(text_block(924, 660, ["1. Une station = une forme maitresse.", "2. Un marqueur fonctionnel sur la face active.", "3. Une zone libre claire devant l'interaction.", "4. Une silhouette differente pour helm, ballast, moteur, porte."], "text", 32))
    body.append(circle(900, 656, 12, "ok"))
    body.append(circle(900, 688, 12, "ok"))
    body.append(circle(900, 720, 12, "ok"))
    body.append(circle(900, 752, 12, "ok"))
    body.append(text_block(1260, 660, ["A eviter :", "1. Consoles interchangeables.", "2. Greeble fin dans la zone de clic.", "3. Reparation, commande et decor superposes.", "4. Accessoires plus visibles que la fonction."], "text", 32))
    body.append(circle(1234, 688, 12, "danger"))
    body.append(circle(1234, 720, 12, "danger"))
    body.append(circle(1234, 752, 12, "danger"))
    body.append(circle(1234, 784, 12, "danger"))
    body.append(text_block(904, 838, ["Seuil de validation visuelle :", "si le joueur ne nomme pas la station en 1 seconde, le mesh n'est pas encore bon."], "small", 24))
    return svg_doc("Regles de lisibilite et silhouettes", "Silhouettes de reference et regles de lisibilite pour les stations du sous-marin.", "".join(body))


def plate_04() -> str:
    body = [header("Planche 4 - Graybox en 4 volumes", "Blocage spatial minimal du premier sous-marin. Le plan et le profil partagent la meme grille en X.")]
    body.append(rect(60, 150, 1120, 470, "panel", 20))
    body.append(text_block(86, 188, ["Vue dessus de blocage"], "h1", 30))
    top_y1, top_y2 = 250, 560
    body.append(hull_top(top_y1, top_y2))
    body.append(deck_grid(top_y1 + 16, top_y2 - 16))
    body.append(bulkheads(top_y1 + 28, top_y2 - 28))
    body.append(rect(GRID["inner_l"], 286, GRID["b1"] - GRID["inner_l"], 238, "ballFill", 22))
    body.append(rect(GRID["b1"], 286, GRID["b2"] - GRID["b1"], 238, "navFill", 22))
    body.append(rect(GRID["b2"], 286, GRID["b3"] - GRID["b2"], 238, "propFill", 22))
    body.append(rect(GRID["b3"], 286, GRID["inner_r"] - GRID["b3"], 238, "confFill", 22))
    body.append(rect(230, 356, 748, 92, "pathBand", 18))
    body.append(path("M250 402 L1000 402", "dim", 'marker-end="url(#arrow)"'))
    body.append(text_block(430, 395, ["Bande de circulation principale"], "small", 22))
    body.append(rect(214, 324, 132, 56, "tank", 14))
    body.append(text_block(230, 358, ["Ballast avant"], "mono", 22))
    body.append(rect(228, 454, 134, 50, "panelAlt", 14))
    body.append(text_block(244, 485, ["Service ballast"], "small", 22))
    body.append(path("M470 492 Q520 428 570 492 L570 522 L470 522 Z", "navSolid"))
    body.append(text_block(498, 472, ["Helm"], "mono", 22))
    body.append(rect(448, 324, 150, 50, "senseSolid", 12))
    body.append(text_block(470, 356, ["Radar / sonar"], "mono", 22))
    body.append(rect(654, 324, 150, 54, "propSolid", 14))
    body.append(text_block(680, 358, ["Station moteur"], "mono", 22))
    body.append(rect(666, 446, 176, 58, "panelAlt", 14))
    body.append(text_block(690, 478, ["Bloc moteur"], "text", 24))
    body.append(rect(870, 324, 118, 54, "ballSolid", 14))
    body.append(text_block(890, 358, ["Pompe"], "mono", 22))
    body.append(rect(866, 446, 96, 58, "tank", 14))
    body.append(text_block(882, 478, ["Ballast arriere"], "small", 20))
    body.append(rect(896, 324, 124, 52, "confSolid", 14))
    body.append(text_block(918, 356, ["Local arriere"], "small", 20))
    body.append(rect(964, 446, 72, 58, "panelAlt", 12))
    body.append(text_block(977, 478, ["Tronc", "de sas"], "small", 20))
    body.append(path("M190 228 L1058 228", "dim", 'marker-start="url(#arrow)" marker-end="url(#arrow)"'))
    body.append(text_block(522, 214, ["Longueur utile interieure : 12,4 m"], "mono", 22, "middle"))
    body.append(path("M90 194 L1184 194", "dim", 'marker-start="url(#arrow)" marker-end="url(#arrow)"'))
    body.append(text_block(634, 180, ["Longueur exterieure cible : 15,2 m"], "mono", 22, "middle"))

    body.append(rect(60, 650, 1120, 540, "panel", 20))
    body.append(text_block(86, 688, ["Vue de profil de blocage"], "h1", 30))
    side_mid = 895
    body.append(hull_side(side_mid))
    body.append(deck_grid(side_mid - 78, side_mid + 74))
    body.append(bulkheads(side_mid - 70, side_mid + 58))
    body.append(rect(210, 840, 156, 82, "ballFill", 12))
    body.append(rect(226, 924, 130, 42, "tank", 12))
    body.append(text_block(244, 950, ["Ballast avant"], "mono", 20))
    body.append(rect(412, 832, 150, 88, "navFill", 12))
    body.append(path("M472 928 Q506 884 540 928 L540 952 L472 952 Z", "navSolid"))
    body.append(text_block(438, 866, ["Helm + poste"], "small", 22))
    body.append(rect(622, 826, 216, 96, "propFill", 12))
    body.append(rect(654, 864, 122, 52, "panelAlt", 12))
    body.append(text_block(684, 896, ["Bloc moteur"], "small", 22))
    body.append(rect(804, 924, 90, 40, "tank", 12))
    body.append(text_block(818, 949, ["Ballast arr."], "small", 20))
    body.append(rect(870, 828, 118, 92, "confFill", 12))
    body.append(text_block(892, 866, ["Local arriere"], "small", 22))
    body.append(rect(932, 710, 62, 112, "confFill", 12))
    body.append(rect(946, 676, 34, 28, "confSolid", 8))
    body.append(text_block(1008, 694, ["Ecoutille"], "small", 22))
    body.append(text_block(902, 744, ["Tronc", "de sas"], "small", 22))
    body.append(line(963, 726, 963, 812, "whiteStroke"))
    body.append(line(950, 746, 976, 746, "whiteStroke"))
    body.append(line(950, 770, 976, 770, "whiteStroke"))
    body.append(line(950, 794, 976, 794, "whiteStroke"))
    body.append(polygon("232,780 248,812 216,812", "danger"))
    body.append(text_block(264, 804, ["Bande de brèche avant"], "small", 22))
    body.append(polygon("694,762 710,794 678,794", "danger"))
    body.append(text_block(730, 786, ["Bande de brèche plafond"], "small", 22))
    body.append(polygon("1032,778 1048,810 1016,810", "danger"))
    body.append(text_block(1064, 802, ["Point haut expose"], "small", 22))

    body.append(rect(1210, 150, 530, 470, "panel", 20))
    body.append(text_block(1238, 188, ["Regles du blocage"], "h1", 30))
    body.append(text_block(1238, 240, ["1. Quatre volumes seulement.", "2. Trois bulkheads lisibles.", "3. Une bande centrale pour la circulation.", "4. Le moteur reste hors de l'axe principal.", "5. Le premier sas est un tronc dorsal simple.", "6. Ballast avant et ballast arriere existent deja au graybox."], "text", 34))
    body.append(text_block(1238, 470, ["Decision :", "le premier proto ne tente pas encore un vrai micro-etage.", "On reserve simplement la masse du tronc dorsal pour l'evolution future."], "small", 24))

    body.append(rect(1210, 650, 530, 540, "panel", 20))
    body.append(text_block(1238, 688, ["Cotes a respecter en modelisation"], "h1", 30))
    body.append(text_block(1238, 744, ["Longueur exterieure cible : 15,2 m", "Longueur utile interieure : 12,4 m", "Passage principal : 2,2 m a 2,3 m", "Porte claire : 1,2 m x 2,1 m", "Hauteur libre interieure : 2,35 m", "Empreinte minimale du tronc : 1,1 m x 1,4 m", "Largeur d'echelle : 0,7 m a 0,8 m", "Frontages reparables : 1,3 m min sur murs utiles"], "text", 34))
    body.append(text_block(1238, 1048, ["Validation :", "si un joueur ne sait pas ou couper la propagation,", "ou il repond pour plonger, le blocage est encore mauvais."], "small", 24))
    return svg_doc("Graybox en 4 volumes", "Blocage du premier sous-marin avec plan et profil alignes.", "".join(body))


def plate_05() -> str:
    body = [header("Planche 5 - Grille de test gameplay et ergonomie", "Questions de validation du premier sous-marin. A lancer sur graybox brut, avant details et avant deco.")]
    body.append(rect(60, 150, 1680, 1020, "panel", 20))
    body.append(text_block(90, 194, ["Les 6 tests a faire en jeu"], "h1", 30))
    cols = [90, 420, 860, 1290]
    headers = ["Question", "Action a faire", "Signal attendu", "Echec a surveiller"]
    for x, head in zip(cols, headers):
        body.append(text_block(x, 248, [head], "h2", 28))
    body.append(line(84, 266, 1716, 266, "tableLine"))
    rows = [
        ("Ou vais-je pour plonger ?", "Entrer depuis le spawn, sans HUD, et chercher la commande de profondeur.", "Le joueur va au helm puis comprend le ballast sans hesiter.", "Il se perd entre helm, radar et ballast."),
        ("Ou puis-je couper la propagation ?", "Provoquer une voie d'eau puis chercher la porte utile.", "La bulkhead door se lit comme decision de confinement.", "La porte ressemble a un mur decoratif ou a un simple passage."),
        ("Ou puis-je reparer ?", "Chercher la surface de reparation en condition d'alarme.", "Le mur reserve se repere vite, sans props parasites.", "Le point reparable est enfoui sous tuyaux, armoires ou clutter."),
        ("Qu'est-ce qui devient dangereux si la coque casse ?", "Casser une zone avant, puis une zone plafond, puis la zone haute arriere.", "Le danger change de place et de nature : fuite, succion, point haut expose.", "Toutes les pannes se ressemblent et aucun endroit ne raconte le risque."),
        ("Deux joueurs se genent-ils ou se coordonnent-ils ?", "Mettre un joueur au helm, un autre a la pompe, puis forcer un deplacement vers la porte.", "Les trajectoires se croisent peu et la coordination reste lisible.", "Les deux joueurs bloquent la meme bande et les stations se mangent entre elles."),
        ("L'axe avant / arriere reste-t-il clair ?", "Faire le test dans le noir ou avec l'eau qui monte.", "Le sens de la proue et du local arriere reste evident.", "Une fois en panique, tout l'interieur devient symetrique."),
    ]
    y = 324
    row_h = 136
    for q, action, success, fail in rows:
        body.append(text_block(cols[0], y, [q], "text", 24))
        body.append(text_block(cols[1], y, [action], "small", 24))
        body.append(text_block(cols[2], y, [success], "small", 24))
        body.append(text_block(cols[3], y, [fail], "small", 24))
        body.append(line(84, y + row_h - 26, 1716, y + row_h - 26, "tableLine"))
        y += row_h

    body.append(rect(90, 1080, 520, 66, "panelAlt", 16))
    body.append(circle(122, 1112, 12, "ok"))
    body.append(text_block(150, 1118, ["Succes : le joueur nomme le bon endroit et s'y rend sans tutoriel."], "small", 22))
    body.append(rect(640, 1080, 500, 66, "panelAlt", 16))
    body.append(circle(672, 1112, 12, "warn"))
    body.append(text_block(700, 1118, ["A revoir : le joueur comprend, mais trop lentement ou avec HUD uniquement."], "small", 22))
    body.append(rect(1170, 1080, 546, 66, "panelAlt", 16))
    body.append(circle(1202, 1112, 12, "danger"))
    body.append(text_block(1230, 1118, ["Echec : il ne sait pas ou aller, ou l'espace bloque la cooperation."], "small", 22))
    return svg_doc("Grille de test gameplay et ergonomie", "Table de test pour valider la lisibilite et l'ergonomie du premier sous-marin.", "".join(body))


def plate_06() -> str:
    body = [header("Planche 6 - Ordre anti-panique pour produire les assets", "Decoupage du travail pour reduire la charge mentale. On fabrique d'abord ce qui rend le jeu lisible et testable.")]
    phases = [
        ("Niveau 0", "Structure", "2 meshes", PALETTE["muted"], ["SM_HullShell_Ext_A", "SM_HullShell_Int_A"], "Sans coque interieure et exterieure, aucun repere n'existe."),
        ("Niveau 1", "Confinement", "3 meshes", PALETTE["conf"], ["SM_BulkheadFrame_A", "SM_BulkheadDoor_A", "SM_HatchTrunk_A"], "Ces meshes donnent le rythme spatial et rendent les compartiments reels."),
        ("Niveau 2", "Reperes de jeu", "5 meshes", PALETTE["nav"], ["SM_HelmConsole_A", "SM_BallastConsole_A", "SM_EngineBlock_A", "SM_PumpModule_A", "SM_RepairWallPanel_A"], "Si ceux-la sont bons, le sous-marin devient deja jouable."),
        ("Niveau 3", "Kit de soutien", "8 a 10 meshes", PALETTE["prop"], ["SM_ValveManifold_A", "SM_PowerCabinet_A", "SM_CableTray_A", "SM_PipeStraight_A", "SM_PipeElbow_A"], "Ils enrichissent sans remettre en cause le layout valide."),
        ("Niveau 4", "Extensions", "facultatif proto", PALETTE["sense"], ["SM_RadarMast_A", "SM_TurretBase_A", "SM_RadarConsole_A"], "Seulement quand le proto principal est stable."),
    ]
    y = 170
    for lvl, name, count, color, assets, why in phases:
        body.append(rect(60, y, 1680, 182, "panel", 20))
        body.append(f'<rect x="60" y="{y}" width="18" height="182" fill="{color}" rx="8"/>')
        body.append(text_block(98, y + 42, [lvl, name], "h1", 28))
        body.append(text_block(316, y + 40, [count], "mono", 24))
        body.append(text_block(470, y + 40, [", ".join(assets[:3])], "text", 24))
        if len(assets) > 3:
            body.append(text_block(470, y + 68, [", ".join(assets[3:])], "text", 24))
        body.append(text_block(470, y + 116, [why], "small", 24))
        y += 206

    body.append(rect(60, 1020, 820, 140, "panelAlt", 18))
    body.append(text_block(88, 1062, ["Regle psychologique"], "h2", 28))
    body.append(text_block(88, 1102, ["Tu ne fabriques pas 'un sous-marin entier'.", "Tu fabriques d'abord 10 meshes qui rendent le sous-marin jouable."], "text", 28))
    body.append(rect(920, 1020, 820, 140, "panelAlt", 18))
    body.append(text_block(948, 1062, ["Interdit avant validation"], "h2", 28))
    body.append(text_block(948, 1102, ["Ne pas passer du temps sur : boulons hero, clutter, tuyaux tres fins, micro variantes,", "matiere finale complexe, decalcomanies de lore, greeble de tourelle."], "text", 28))
    return svg_doc("Ordre anti-panique pour produire les assets", "Ordre de travail pragmatique pour produire les assets du premier sous-marin.", "".join(body))


def plate_07() -> str:
    body = [header("Planche 7 - Architecture precise du premier sous-marin", "Implantation detaillee en plan et en profil. La planche verrouille les besoins reels avant le design de surface.")]
    body.append(rect(60, 150, 1120, 500, "panel", 20))
    body.append(text_block(86, 188, ["Plan d'implantation"], "h1", 30))
    top_y1, top_y2 = 250, 590
    body.append(hull_top(top_y1, top_y2))
    body.append(deck_grid(top_y1 + 16, top_y2 - 16))
    body.append(bulkheads(top_y1 + 28, top_y2 - 28))
    body.append(rect(GRID["inner_l"], 296, GRID["b1"] - GRID["inner_l"], 248, "ballFill", 22))
    body.append(rect(GRID["b1"], 296, GRID["b2"] - GRID["b1"], 248, "navFill", 22))
    body.append(rect(GRID["b2"], 296, GRID["b3"] - GRID["b2"], 248, "propFill", 22))
    body.append(rect(GRID["b3"], 296, GRID["inner_r"] - GRID["b3"], 248, "confFill", 22))
    body.append(rect(228, 370, 724, 94, "pathBand", 18))
    body.append(path("M248 417 L972 417", "dim", 'marker-end="url(#arrow)"'))
    body.append(text_block(444, 406, ["Spine de circulation", "joueur 1 <-> joueur 2"], "small", 22))
    body.append(text_block(212, 332, ["A - Volume avant", "ballast avant + service"], "h2", 24))
    body.append(rect(220, 350, 132, 58, "tank", 14))
    body.append(text_block(236, 384, ["Ballast avant"], "mono", 22))
    body.append(rect(224, 470, 150, 52, "panelAlt", 14))
    body.append(text_block(242, 500, ["Vannes + manifold"], "small", 20))
    body.append(path("M205 308 L205 532", "repair"))
    body.append(text_block(214, 560, ["Frontage reparable reserve"], "small", 20))
    body.append(text_block(424, 332, ["B - Volume central", "helm + radar"], "h2", 24))
    body.append(path("M482 510 Q520 458 558 510 L558 540 L482 540 Z", "navSolid"))
    body.append(text_block(498, 492, ["Helm"], "mono", 22))
    body.append(rect(454, 350, 160, 52, "senseSolid", 12))
    body.append(text_block(478, 382, ["Radar / sonar"], "mono", 22))
    body.append(rect(448, 544, 156, 34, "panelAlt", 12))
    body.append(text_block(470, 566, ["Zone de stance"], "small", 20))
    body.append(path("M592 308 L592 532", "repair"))
    body.append(text_block(634, 332, ["C - Volume machine", "moteur + pompe + ballast arr."], "h2", 24))
    body.append(rect(654, 350, 150, 54, "propSolid", 14))
    body.append(text_block(676, 382, ["Station moteur"], "mono", 22))
    body.append(rect(664, 462, 180, 60, "panelAlt", 14))
    body.append(text_block(690, 494, ["Bloc moteur"], "text", 22))
    body.append(rect(862, 350, 120, 54, "ballSolid", 14))
    body.append(text_block(884, 382, ["Pompe"], "mono", 22))
    body.append(rect(856, 462, 104, 58, "tank", 14))
    body.append(text_block(868, 492, ["Ballast", "arriere"], "small", 20))
    body.append(path("M864 308 L864 532", "repair"))
    body.append(text_block(872, 332, ["D - Volume arriere", "local de sas + tronc"], "h2", 24))
    body.append(rect(886, 350, 116, 54, "confSolid", 14))
    body.append(text_block(908, 382, ["Service"], "mono", 22))
    body.append(rect(980, 458, 64, 70, "panelAlt", 12))
    body.append(text_block(992, 486, ["Echelle", "+ acces"], "small", 20))
    body.append(path("M1022 308 L1022 532", "repair"))

    body.append(rect(60, 680, 1120, 510, "panel", 20))
    body.append(text_block(86, 718, ["Profil et besoins verticaux"], "h1", 30))
    side_mid = 930
    body.append(hull_side(side_mid))
    body.append(deck_grid(side_mid - 82, side_mid + 72))
    body.append(bulkheads(side_mid - 70, side_mid + 58))
    body.append(rect(214, 874, 152, 82, "ballFill", 12))
    body.append(rect(228, 958, 128, 42, "tank", 12))
    body.append(text_block(244, 984, ["Ballast avant"], "mono", 20))
    body.append(rect(416, 866, 150, 88, "navFill", 12))
    body.append(path("M478 962 Q512 916 546 962 L546 986 L478 986 Z", "navSolid"))
    body.append(text_block(438, 900, ["Poste de commandement"], "small", 22))
    body.append(rect(622, 860, 216, 96, "propFill", 12))
    body.append(rect(654, 898, 126, 52, "panelAlt", 12))
    body.append(text_block(686, 930, ["Bloc moteur"], "small", 22))
    body.append(rect(804, 958, 90, 42, "tank", 12))
    body.append(text_block(818, 983, ["Ballast arr."], "small", 20))
    body.append(rect(870, 862, 124, 92, "confFill", 12))
    body.append(text_block(890, 900, ["Local de sas"], "small", 22))
    body.append(rect(938, 744, 60, 112, "confFill", 12))
    body.append(rect(950, 706, 36, 30, "confSolid", 8))
    body.append(text_block(1008, 724, ["Ecoutille plafond"], "small", 22))
    body.append(text_block(910, 780, ["Tronc dorsal"], "small", 22))
    body.append(line(968, 760, 968, 846, "whiteStroke"))
    body.append(line(956, 782, 980, 782, "whiteStroke"))
    body.append(line(956, 806, 980, 806, "whiteStroke"))
    body.append(line(956, 830, 980, 830, "whiteStroke"))
    body.append(polygon("234,806 250,838 218,838", "danger"))
    body.append(text_block(268, 832, ["Brèche avant = fuite + succion locale"], "small", 22))
    body.append(polygon("700,790 716,822 684,822", "danger"))
    body.append(text_block(736, 816, ["Brèche plafond = danger vertical"], "small", 22))
    body.append(polygon("1030,804 1046,836 1014,836", "danger"))
    body.append(text_block(1062, 830, ["Point haut = zone la plus exposee"], "small", 22))
    body.append(path("M190 824 L1058 824", "dim", 'marker-start="url(#arrow)" marker-end="url(#arrow)"'))
    body.append(text_block(624, 808, ["12,4 m utiles"], "mono", 22, "middle"))

    body.append(rect(1210, 150, 530, 1040, "panel", 20))
    body.append(text_block(1238, 188, ["Besoins verrouilles par le code"], "h1", 30))
    body.append(text_block(1238, 246, ["1. Deux masses de ballast existent deja : avant et arriere.", "2. Le helm reste le repere interieur principal.", "3. Le moteur et la pompe travaillent ensemble mais doivent se distinguer.", "4. Les portes sont du gameplay de confinement, pas du decor.", "5. Les frontages de reparation doivent rester reserves.", "6. Le premier sas recommande est dorsal, simple, a ecoutille."], "text", 34))
    body.append(text_block(1238, 520, ["Dimensions de reference"], "h2", 28))
    body.append(text_block(1238, 562, ["Longueur exterieure : 15,2 m", "Longueur utile : 12,4 m", "Passage clair : 2,2 m a 2,3 m", "Porte claire : 1,2 m x 2,1 m", "Empreinte tronc : 1,1 m x 1,4 m", "Largeur echelle : 0,7 m a 0,8 m"], "text", 32))
    body.append(text_block(1238, 792, ["Choix retenu"], "h2", 28))
    body.append(text_block(1238, 834, ["On construit maintenant :", "local arriere + tronc dorsal + ecoutille simple.", "On reserve l'enveloppe pour evoluer plus tard", "vers un micro-etage avec vrai sas a deux portes."], "text", 32))
    body.append(text_block(1238, 1012, ["Ce qu'il ne faut pas faire"], "h2", 28))
    body.append(text_block(1238, 1054, ["Ne pas caser dans D : sas, tourelle, trunk, service et decor au meme niveau.", "Ne pas faire un volume arriere trop petit pour y comprendre l'echelle.", "Ne pas casser la spine centrale avec des machines saillantes."], "small", 26))
    return svg_doc("Architecture precise du premier sous-marin", "Implantation precise du premier sous-marin en plan et en profil.", "".join(body))


def plate_08() -> str:
    body = [header("Planche 8 - Systeme de symboles et regles de lecture", "Cette planche explique la grammaire graphique. Chaque signe a une seule signification et reste stable d'une page a l'autre.")]
    body.append(rect(60, 150, 760, 1020, "panel", 20))
    body.append(text_block(88, 188, ["Legende des symboles"], "h1", 30))
    legend = [
        ("ballFill", "Compartiment ballast / pompage", "zone dediee a l'eau, au pompage et au trim."),
        ("navFill", "Compartiment commande", "volume ou le helm sert de repere principal."),
        ("propFill", "Compartiment machine", "masse lourde, energie, moteur, bruit."),
        ("confFill", "Compartiment confinement / sas", "porte, service arriere, tronc, fermeture."),
        ("tank", "Ballast", "masse d'eau identifiee. Toujours avant ou arriere."),
        ("panelAlt", "Surface technique", "mur ou bloc de maintenance, jamais lecture principale."),
    ]
    y = 246
    for cls, label, desc in legend:
        body.append(rect(96, y - 20, 88, 52, cls, 12))
        body.append(text_block(210, y, [label], "h2", 24))
        body.append(text_block(210, y + 30, [desc], "small", 22))
        y += 104
    body.append(line(88, 874, 792, 874, "tableLine"))
    body.append(text_block(88, 916, ["Autres signes"], "h2", 28))
    body.append(line(120, 970, 204, 970, "bulkhead"))
    body.append(line(162, 948, 162, 992, "doorGap"))
    body.append(text_block(228, 964, ["Cloison + ouverture de porte"], "h2", 24))
    body.append(text_block(228, 994, ["Le trait epais dit : decision de confinement."], "small", 22))
    body.append(path("M114 1048 L206 1048", "repair"))
    body.append(text_block(228, 1042, ["Frontage reparable"], "h2", 24))
    body.append(text_block(228, 1072, ["Toujours contre un mur ou un panneau reserve."], "small", 22))
    body.append(polygon("132,1122 148,1154 116,1154", "danger"))
    body.append(text_block(228, 1144, ["Triangle danger / brèche"], "h2", 24))
    body.append(text_block(228, 1174, ["Signale fuite, succion ou exposition verticale."], "small", 22))

    body.append(rect(860, 150, 880, 620, "panel", 20))
    body.append(text_block(888, 188, ["Exemple minimal de lecture correcte"], "h1", 30))
    body.append(hull_top(270, 620))
    body.append(deck_grid(286, 604))
    body.append(bulkheads(304, 586))
    body.append(rect(GRID["inner_l"], 316, GRID["b1"] - GRID["inner_l"], 258, "ballFill", 22))
    body.append(rect(GRID["b1"], 316, GRID["b2"] - GRID["b1"], 258, "navFill", 22))
    body.append(rect(GRID["b2"], 316, GRID["b3"] - GRID["b2"], 258, "propFill", 22))
    body.append(rect(GRID["b3"], 316, GRID["inner_r"] - GRID["b3"], 258, "confFill", 22))
    body.append(rect(228, 394, 726, 90, "pathBand", 18))
    body.append(rect(214, 348, 132, 56, "tank", 14))
    body.append(text_block(230, 382, ["Ballast avant"], "mono", 22))
    body.append(path("M482 532 Q520 480 558 532 L558 562 L482 562 Z", "navSolid"))
    body.append(rect(454, 346, 160, 54, "senseSolid", 12))
    body.append(rect(654, 346, 150, 54, "propSolid", 14))
    body.append(rect(854, 346, 120, 54, "ballSolid", 14))
    body.append(rect(888, 468, 112, 56, "confSolid", 14))
    body.append(rect(982, 468, 64, 70, "panelAlt", 12))
    body.append(path("M205 328 L205 562", "repair"))
    body.append(path("M592 328 L592 562", "repair"))
    body.append(path("M864 328 L864 562", "repair"))
    body.append(polygon("702,248 718,280 686,280", "danger"))
    body.append(text_block(734, 276, ["Exemple : danger plafond"], "small", 22))
    body.append(text_block(888, 662, ["Lecture attendue :", "on identifie d'abord les volumes, puis la spine, puis les stations, puis les dangers."], "small", 24))

    body.append(rect(860, 810, 880, 360, "panel", 20))
    body.append(text_block(888, 848, ["Regles d'usage"], "h1", 30))
    body.append(text_block(888, 900, ["1. Un symbole = une fonction. Ne jamais reutiliser le meme signe pour deux sens.", "2. Le plan et le profil partagent la meme grille en X.", "3. Le texte ne compense pas un symbole flou. Si le texte devient long, le signe est mauvais.", "4. Les couleurs marquent la famille. La forme marque l'objet exact.", "5. Une porte etanche doit toujours rester plus lisible qu'un tuyau ou qu'une lampe.", "6. Si deux libelles se chevauchent, la densite de la planche est deja trop forte."], "text", 34))
    return svg_doc("Systeme de symboles et regles de lecture", "Legende de symboles et mini exemple de lecture pour les planches du sous-marin.", "".join(body))


def plate_09() -> str:
    body = [header("Planche 9 - Sas : option retenue et evolution future", "Comparaison franche entre le sas dorsal simple du premier proto et le micro-etage a deux portes envisage plus tard.")]
    body.append(rect(60, 150, 810, 1000, "panel", 20))
    body.append(text_block(88, 188, ["Option A - Recommandee maintenant"], "h1", 30))
    body.append(text_block(88, 230, ["Local arriere + tronc dorsal + ecoutille plafond"], "small", 24))
    body.append(hull_side(520))
    body.append(rect(224, 468, 150, 82, "ballFill", 12))
    body.append(rect(424, 460, 150, 88, "navFill", 12))
    body.append(rect(632, 454, 214, 96, "propFill", 12))
    body.append(rect(872, 456, 132, 92, "confFill", 12))
    body.append(rect(936, 338, 60, 112, "confFill", 12))
    body.append(rect(948, 300, 36, 30, "confSolid", 8))
    body.append(text_block(1008, 316, ["Ecoutille"], "small", 22))
    body.append(text_block(902, 376, ["Tronc"], "small", 22))
    body.append(line(966, 356, 966, 442, "whiteStroke"))
    body.append(line(954, 378, 978, 378, "whiteStroke"))
    body.append(line(954, 402, 978, 402, "whiteStroke"))
    body.append(line(954, 426, 978, 426, "whiteStroke"))
    body.append(text_block(88, 720, ["Avantages :", "1. Compatible avec le code actuel et avec la lecture en 4 volumes.", "2. Faible cout de mesh, faible risque de layout.", "3. Laisse intacte la bande de circulation principale.", "4. Suffisant pour un premier prototype testable."], "text", 34))
    body.append(text_block(88, 950, ["Contrainte :", "le volume arriere doit quand meme etre dimensionne", "pour qu'une future version a deux portes reste possible."], "small", 24))

    body.append(rect(930, 150, 810, 1000, "panel", 20))
    body.append(text_block(958, 188, ["Option B - Evolution future"], "h1", 30))
    body.append(text_block(958, 230, ["Micro-etage superieur avec vrai sas a deux portes"], "small", 24))
    body.append(hull_side(520))
    body.append(rect(1090, 360, 108, 82, "confFill", 12))
    body.append(rect(1124, 244, 72, 110, "confFill", 12))
    body.append(rect(1136, 198, 48, 32, "confSolid", 8))
    body.append(line(1160, 262, 1160, 348, "whiteStroke"))
    body.append(line(1148, 286, 1172, 286, "whiteStroke"))
    body.append(line(1148, 310, 1172, 310, "whiteStroke"))
    body.append(text_block(1212, 218, ["Porte exterieure"], "small", 22))
    body.append(text_block(1212, 286, ["Sas superieur"], "small", 22))
    body.append(text_block(1212, 402, ["Porte interieure"], "small", 22))
    body.append(text_block(958, 720, ["Interet :", "1. Plus riche pour sortie, pression et procedures.", "2. Plus fort en fantasy sous-marin classique.", "3. Ouvre du gameplay specifique de verrouillage."], "text", 34))
    body.append(text_block(958, 900, ["Couts :", "1. Volume vertical supplementaire a assumer.", "2. Plus d'assets et plus de contraintes de circulation.", "3. Plus de travail pour rester lisible et ergonomique."], "small", 24))

    body.append(rect(60, 1180, 1680, 70, "panelAlt", 18))
    body.append(text_block(90, 1222, ["Verdict : construire l'Option A maintenant, mais reserver la masse et les ancrages pour migrer plus tard vers l'Option B sans refaire toute la coque."], "text", 26))
    return svg_doc("Sas : option retenue et evolution future", "Comparatif entre le sas dorsal simple et le micro-etage a deux portes.", "".join(body))


PLATES = {
    "02-kit-assets-systemes.svg": plate_02,
    "03-regles-lisibilite-silhouettes.svg": plate_03,
    "04-graybox-4-volumes.svg": plate_04,
    "05-grille-test-gameplay-ergonomie.svg": plate_05,
    "06-ordre-anti-panique-assets.svg": plate_06,
    "07-architecture-precise-plan-profile.svg": plate_07,
    "08-grammar-symbols-reading.svg": plate_08,
    "09-sas-options-recommendation.svg": plate_09,
}


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for file_name, factory in PLATES.items():
        (OUT_DIR / file_name).write_text(factory(), encoding="utf-8")
    print(f"Generated {len(PLATES)} SVG files in {OUT_DIR}")


if __name__ == "__main__":
    main()
