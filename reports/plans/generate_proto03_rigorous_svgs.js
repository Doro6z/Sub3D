const fs = require("fs");
const path = require("path");

const OUT_DIR = path.join(__dirname, "20260325-proto03-pack-illustre-assets");
const W = 1800;
const H = 1280;

const PALETTE = {
  bg: "#0f1419",
  panel: "#172028",
  panelAlt: "#131b22",
  stroke: "#43525d",
  soft: "#34414a",
  ink: "#f4efe6",
  muted: "#d1c9b8",
  nav: "#d4a93a",
  navDark: "#4a3b17",
  ball: "#66b7c8",
  ballDark: "#274854",
  prop: "#5a95dc",
  propDark: "#233c61",
  conf: "#c66e3a",
  confDark: "#4f3022",
  sense: "#bb8638",
  senseDark: "#4a341a",
  danger: "#de5a49",
  ok: "#82c97b",
  warn: "#e3b65b",
};

const GRID = {
  outerL: 90,
  outerR: 1154,
  innerL: 190,
  b1: 400,
  b2: 610,
  b3: 848,
  innerR: 1058,
};

function esc(value) {
  return String(value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&apos;");
}

function textBlock(x, y, lines, cls = "text", lineH = 28, anchor = "start") {
  const tspans = lines
    .map((line, index) => `<tspan x="${x}" dy="${index === 0 ? 0 : lineH}">${esc(line)}</tspan>`)
    .join("");
  return `<text class="${cls}" x="${x}" y="${y}" text-anchor="${anchor}">${tspans}</text>`;
}

function rect(x, y, w, h, cls, rx = 0, extra = "") {
  return `<rect class="${cls}" x="${x}" y="${y}" width="${w}" height="${h}" rx="${rx}" ${extra}/>`;
}

function line(x1, y1, x2, y2, cls, extra = "") {
  return `<line class="${cls}" x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" ${extra}/>`;
}

function circle(cx, cy, r, cls, extra = "") {
  return `<circle class="${cls}" cx="${cx}" cy="${cy}" r="${r}" ${extra}/>`;
}

function pathEl(d, cls, extra = "") {
  return `<path class="${cls}" d="${d}" ${extra}/>`;
}

function polygon(points, cls, extra = "") {
  return `<polygon class="${cls}" points="${points}" ${extra}/>`;
}

function baseStyle() {
  return `
  <defs>
    <marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
      <path d="M 0 0 L 10 5 L 0 10 z" fill="${PALETTE.ink}"/>
    </marker>
    <style><![CDATA[
      .bg { fill: ${PALETTE.bg}; }
      .panel { fill: ${PALETTE.panel}; stroke: ${PALETTE.stroke}; stroke-width: 2; }
      .panelAlt { fill: ${PALETTE.panelAlt}; stroke: ${PALETTE.soft}; stroke-width: 2; }
      .frame { fill: none; stroke: #9ca8af; stroke-width: 4; }
      .grid { fill: none; stroke: ${PALETTE.soft}; stroke-width: 1; stroke-dasharray: 6 8; opacity: 0.45; }
      .bulkhead { stroke: #98a4ad; stroke-width: 8; }
      .doorGap { stroke: ${PALETTE.bg}; stroke-width: 14; }
      .repair { fill: none; stroke: #ead59b; stroke-width: 4; stroke-dasharray: 12 10; }
      .pathBand { fill: #f0eadb; opacity: 0.12; stroke: ${PALETTE.ink}; stroke-width: 2; }
      .dim { fill: none; stroke: ${PALETTE.ink}; stroke-width: 2.2; }
      .danger { fill: ${PALETTE.danger}; }
      .ok { fill: ${PALETTE.ok}; }
      .warn { fill: ${PALETTE.warn}; }
      .title { fill: ${PALETTE.ink}; font: 700 42px Arial, 'Segoe UI', sans-serif; }
      .subtitle { fill: ${PALETTE.muted}; font: 19px Arial, 'Segoe UI', sans-serif; }
      .h1 { fill: ${PALETTE.ink}; font: 700 28px Arial, 'Segoe UI', sans-serif; }
      .h2 { fill: ${PALETTE.ink}; font: 700 22px Arial, 'Segoe UI', sans-serif; }
      .text { fill: ${PALETTE.ink}; font: 19px Arial, 'Segoe UI', sans-serif; }
      .small { fill: ${PALETTE.muted}; font: 17px Arial, 'Segoe UI', sans-serif; }
      .mono { fill: ${PALETTE.ink}; font: 17px Consolas, 'Courier New', monospace; }
      .navFill { fill: ${PALETTE.navDark}; stroke: ${PALETTE.nav}; stroke-width: 2.5; }
      .ballFill { fill: ${PALETTE.ballDark}; stroke: ${PALETTE.ball}; stroke-width: 2.5; }
      .propFill { fill: ${PALETTE.propDark}; stroke: ${PALETTE.prop}; stroke-width: 2.5; }
      .confFill { fill: ${PALETTE.confDark}; stroke: ${PALETTE.conf}; stroke-width: 2.5; }
      .senseFill { fill: ${PALETTE.senseDark}; stroke: ${PALETTE.sense}; stroke-width: 2.5; }
      .navSolid { fill: ${PALETTE.nav}; }
      .ballSolid { fill: ${PALETTE.ball}; }
      .propSolid { fill: ${PALETTE.prop}; }
      .confSolid { fill: ${PALETTE.conf}; }
      .senseSolid { fill: ${PALETTE.sense}; }
      .whiteStroke { fill: none; stroke: ${PALETTE.ink}; stroke-width: 3; }
      .tank { fill: ${PALETTE.ball}; stroke: #d8f3fb; stroke-width: 2; }
      .tag { fill: #e7d8a2; }
      .tag2 { fill: #6a7c86; }
      .tagText { fill: ${PALETTE.bg}; font: 700 16px Arial, 'Segoe UI', sans-serif; }
      .tableLine { stroke: ${PALETTE.soft}; stroke-width: 2; }
    ]]></style>
  </defs>
`;
}

function svgDoc(title, desc, body, width = W, height = H) {
  return `<svg viewBox="0 0 ${width} ${height}" xmlns="http://www.w3.org/2000/svg" role="img" aria-labelledby="title desc">
  <title id="title">${esc(title)}</title>
  <desc id="desc">${esc(desc)}</desc>
${baseStyle()}
  <rect class="bg" x="0" y="0" width="${width}" height="${height}"/>
${body}
</svg>
`;
}

function header(title, subtitle) {
  return `<text class="title" x="60" y="62">${esc(title)}</text><text class="subtitle" x="60" y="98">${esc(subtitle)}</text>`;
}

function hullTop(yTop, yBottom) {
  return pathEl(
    `M${GRID.outerL + 30} ${yTop} Q${GRID.outerL} ${yTop} ${GRID.outerL - 16} ${yTop + 38} ` +
      `L${GRID.outerL - 30} ${yTop + 88} L${GRID.outerL - 30} ${yBottom - 88} ` +
      `L${GRID.outerL - 16} ${yBottom - 38} Q${GRID.outerL} ${yBottom} ${GRID.outerL + 30} ${yBottom} ` +
      `L${GRID.outerR - 40} ${yBottom} Q${GRID.outerR} ${yBottom} ${GRID.outerR + 18} ${yBottom - 48} ` +
      `L${GRID.outerR + 30} ${yBottom - 96} L${GRID.outerR + 30} ${yTop + 96} ` +
      `L${GRID.outerR + 18} ${yTop + 48} Q${GRID.outerR} ${yTop} ${GRID.outerR - 40} ${yTop} Z`,
    "frame"
  );
}

function hullSide(yMid) {
  const top = yMid - 92;
  const bottom = yMid + 90;
  return pathEl(
    `M${GRID.outerL + 45} ${bottom} Q${GRID.outerL} ${bottom} ${GRID.outerL - 18} ${bottom - 36} ` +
      `Q${GRID.outerL - 36} ${yMid} ${GRID.outerL - 10} ${top + 30} ` +
      `Q${GRID.outerL + 16} ${top} ${GRID.outerL + 70} ${top} ` +
      `L${GRID.outerR - 70} ${top} Q${GRID.outerR + 6} ${top + 4} ${GRID.outerR + 34} ${yMid - 22} ` +
      `Q${GRID.outerR + 56} ${yMid} ${GRID.outerR + 18} ${bottom - 24} ` +
      `Q${GRID.outerR - 4} ${bottom} ${GRID.outerR - 64} ${bottom} Z`,
    "frame"
  );
}

function bulkheads(y1, y2) {
  const xs = [GRID.b1, GRID.b2, GRID.b3];
  return xs
    .map((x) => line(x, y1, x, y2, "bulkhead") + line(x, y1 + 86, x, y1 + 152, "doorGap"))
    .join("");
}

function deckGrid(y1, y2) {
  return [GRID.innerL, GRID.b1, GRID.b2, GRID.b3, GRID.innerR]
    .map((x) => line(x, y1, x, y2, "grid"))
    .join("");
}

function wrapLines(text, maxChars) {
  const words = String(text).split(/\s+/).filter(Boolean);
  const lines = [];
  let current = "";
  words.forEach((word) => {
    const next = current ? `${current} ${word}` : word;
    if (next.length > maxChars && current) {
      lines.push(current);
      current = word;
    } else {
      current = next;
    }
  });
  if (current) lines.push(current);
  return lines.length ? lines : [""];
}

function wrappedText(x, y, text, maxChars, cls = "small", lineH = 24, anchor = "start") {
  return textBlock(x, y, wrapLines(text, maxChars), cls, lineH, anchor);
}

function plate02() {
  const body = [header("Planche 2 - Kit d'assets par systeme", "Inventaire de production du premier sous-marin. On raisonne en familles fonctionnelles et en dependances.")];
  const colX = [60, 406, 752, 1098, 1444];
  const widths = [310, 310, 310, 310, 296];
  const titles = [
    ["Commande / navigation", "navSolid"],
    ["Ballast / pompage", "ballSolid"],
    ["Propulsion / energie", "propSolid"],
    ["Confinement / reparation", "confSolid"],
    ["Combat / senseur", "senseSolid"],
  ];
  const cards = [
    [["P1", "SM_HelmConsole_A"], ["P1", "SM_CommandDesk_A"], ["P1", "SM_RadarConsole_A"], ["P2", "SM_NavRepeater_A"], ["P2", "SM_ChartShelf_A"]],
    [["P1", "SM_BallastForeTank_A"], ["P1", "SM_BallastRearTank_A"], ["P1", "SM_BallastConsole_A"], ["P1", "SM_PumpModule_A"], ["P1", "SM_ValveManifold_A"]],
    [["P1", "SM_EngineBlock_A"], ["P1", "SM_PowerCabinet_A"], ["P2", "SM_ShaftHousing_A"], ["P2", "SM_CableTray_A"], ["P2", "SM_BatteryRack_A"]],
    [["P1", "SM_BulkheadFrame_A"], ["P1", "SM_BulkheadDoor_A"], ["P1", "SM_RepairWallPanel_A"], ["P1", "SM_HatchTrunk_A"], ["P2", "SM_CompartmentSign_A"]],
    [["P1", "SM_RadarMast_A"], ["P1", "SM_TurretBase_A"], ["P2", "SM_TurretControl_A"], ["P2", "SM_SensorCabinet_A"], ["P2", "SM_AntennaCluster_A"]],
  ];
  const notes = [
    ["Role : noeud lisible, frontal, visible a 10 m.", "Ne jamais noyer le helm dans des props lateraux."],
    ["Role : faire lire le flux et la masse d'eau.", "Les deux ballasts doivent exister comme actifs distincts."],
    ["Role : masse lourde, bruit, maintenance, energie.", "Le moteur se lit par volume, pas par greeble."],
    ["Role : couper la propagation et reserver la reparation.", "Une porte sauve le run ou le condamne."],
    ["Role : point haut, lecture exterieure, extension future.", "Le premier proto garde cette famille tres sobre."],
  ];

  colX.forEach((x, idx) => {
    const w = widths[idx];
    body.push(rect(x, 150, w, 500, "panel", 20));
    body.push(rect(x, 150, w, 62, titles[idx][1], 20));
    body.push(textBlock(x + 18, 190, [titles[idx][0]], "h2", 26));
    let y = 250;
    cards[idx].forEach(([tier, asset]) => {
      const pillCls = tier === "P1" ? "tag" : "tag2";
      body.push(rect(x + 18, y - 16, 58, 28, pillCls, 14));
      body.push(textBlock(x + 38, y + 2, [tier], "tagText", 18, "middle"));
      body.push(textBlock(x + 92, y + 2, [asset], "text", 24));
      y += 54;
    });
    body.push(line(x + 18, 488, x + w - 18, 488, "tableLine"));
    body.push(textBlock(x + 18, 538, notes[idx], "small", 24));
  });

  body.push(rect(60, 690, 1680, 520, "panel", 20));
  body.push(textBlock(84, 730, ["Ordre reel de fabrication pour un proto jouable"], "h1", 30));
  const rows = [
    ["Phase 0", "2 meshes", "SM_HullShell_Ext_A, SM_HullShell_Int_A", "Sans eux, aucune lecture spatiale n'existe."],
    ["Phase 1", "3 meshes", "SM_BulkheadFrame_A, SM_BulkheadDoor_A, SM_HatchTrunk_A", "Le confinement et le sas doivent apparaitre avant les stations."],
    ["Phase 2", "5 meshes", "SM_HelmConsole_A, SM_BallastConsole_A, SM_EngineBlock_A, SM_PumpModule_A, SM_RepairWallPanel_A", "Ce sont les cinq reperes de gameplay obligatoires."],
    ["Phase 3", "8 a 10 meshes", "tuyaux, cables, echelle, rampe, luminaire, coffre, manifolds", "Seulement apres validation du graybox et du test de circulation."],
    ["Phase 4", "variantes d'etat", "nominal, utilisable, endommage, critique, inonde", "Les etats arrivent en dernier, jamais avant la lisibilite de base."],
  ];
  let y = 790;
  body.push(line(84, 758, 1716, 758, "tableLine"));
  rows.forEach(([phase, count, assets, why]) => {
    body.push(textBlock(92, y, [phase], "h2", 24));
    body.push(textBlock(240, y, [count], "mono", 24));
    body.push(textBlock(430, y, [assets], "text", 24));
    body.push(textBlock(430, y + 26, [why], "small", 24));
    y += 88;
    body.push(line(84, y - 22, 1716, y - 22, "tableLine"));
  });
  body.push(textBlock(84, 1180, ["Interdit avant validation :", "petits props decoratifs, tuyaux micro-detail, mobilier gratuit, doubles variantes de consoles, clutter de sol."], "small", 24));
  return svgDoc("Kit d'assets par systeme", "Inventaire de production des assets du premier sous-marin.", body.join(""));
}

function plate03() {
  const body = [header("Planche 3 - Regles de lisibilite et silhouettes", "Une station doit se reconnaitre en une forme, un axe et une zone d'approche. Pas en accumulation de petits details.")];
  const cardSpecs = [
    [60, 150, "Console de helm", "navSolid", "arc large + base stable", "face frontale forte", "aire libre de 1,2 m min."],
    [470, 150, "Console ballast", "ballSolid", "bloc carre + vannes", "flux et pression visibles", "ballast avant / arriere lisibles."],
    [880, 150, "Bloc moteur", "propSolid", "masse unique lourde", "un flanc de maintenance", "pas de mille petites pieces."],
    [1290, 150, "Porte etanche", "confSolid", "cadre epais + verrou", "decision de confinement", "doit sembler sauver le run."],
    [60, 560, "Panneau de reparation", "confSolid", "mur reserve et clair", "feedback lisible de loin", "jamais enterre sous le decor."],
    [470, 560, "Tronc de sas", "senseSolid", "verticalite + echelle", "point haut exterieur", "premier proto = ecoutille plafond."],
  ];
  cardSpecs.forEach(([x, y, title, shapeCls, keyLine, rule1, rule2]) => {
    body.push(rect(x, y, 350, 330, "panel", 18));
    body.push(textBlock(x + 24, y + 42, [title], "h2", 24));
    body.push(textBlock(x + 24, y + 78, [rule1, rule2], "small", 22));
    if (title === "Console de helm") {
      body.push(rect(x + 70, y + 180, 210, 90, "panelAlt", 16));
      body.push(pathEl(`M${x + 84} ${y + 264} Q${x + 175} ${y + 156} ${x + 266} ${y + 264} L${x + 266} ${y + 286} L${x + 84} ${y + 286} Z`, shapeCls));
      body.push(rect(x + 94, y + 176, 162, 118, "grid", 14));
    } else if (title === "Console ballast") {
      body.push(rect(x + 90, y + 196, 180, 72, shapeCls, 16));
      body.push(circle(x + 128, y + 232, 18, "whiteStroke"));
      body.push(circle(x + 198, y + 232, 18, "whiteStroke"));
      body.push(pathEl(`M${x + 270} ${y + 232} C${x + 318} ${y + 232} ${x + 326} ${y + 186} ${x + 340} ${y + 170}`, "whiteStroke"));
    } else if (title === "Bloc moteur") {
      body.push(rect(x + 72, y + 190, 214, 86, shapeCls, 24));
      body.push(circle(x + 128, y + 233, 28, "whiteStroke"));
      body.push(rect(x + 184, y + 204, 92, 58, "whiteStroke", 10));
    } else if (title === "Porte etanche") {
      body.push(rect(x + 96, y + 182, 160, 118, shapeCls, 20));
      body.push(rect(x + 128, y + 202, 96, 78, "whiteStroke", 26));
      body.push(circle(x + 208, y + 241, 12, "whiteStroke"));
    } else if (title === "Panneau de reparation") {
      body.push(rect(x + 114, y + 186, 120, 114, shapeCls, 10));
      body.push(line(x + 144, y + 216, x + 214, y + 216, "whiteStroke"));
      body.push(line(x + 144, y + 248, x + 214, y + 248, "whiteStroke"));
      body.push(line(x + 144, y + 280, x + 194, y + 280, "whiteStroke"));
    } else {
      body.push(rect(x + 148, y + 166, 44, 108, shapeCls, 12));
      body.push(circle(x + 170, y + 274, 58, shapeCls));
      body.push(circle(x + 170, y + 274, 76, "whiteStroke"));
      body.push(line(x + 170, y + 184, x + 170, y + 316, "whiteStroke"));
      body.push(line(x + 156, y + 214, x + 184, y + 214, "whiteStroke"));
      body.push(line(x + 156, y + 238, x + 184, y + 238, "whiteStroke"));
    }
    body.push(textBlock(x + 24, y + 310, [`Cle visuelle : ${keyLine}.`], "small", 22));
  });

  body.push(rect(880, 560, 760, 330, "panel", 18));
  body.push(textBlock(904, 602, ["Regles de lecture obligatoires"], "h1", 30));
  body.push(textBlock(924, 660, ["1. Une station = une forme maitresse.", "2. Un marqueur fonctionnel sur la face active.", "3. Une zone libre claire devant l'interaction.", "4. Une silhouette differente pour helm, ballast, moteur, porte."], "text", 32));
  [656, 688, 720, 752].forEach((yy) => body.push(circle(900, yy, 12, "ok")));
  body.push(textBlock(1260, 660, ["A eviter :", "1. Consoles interchangeables.", "2. Greeble fin dans la zone de clic.", "3. Reparation, commande et decor superposes.", "4. Accessoires plus visibles que la fonction."], "text", 32));
  [688, 720, 752, 784].forEach((yy) => body.push(circle(1234, yy, 12, "danger")));
  body.push(textBlock(904, 838, ["Seuil de validation visuelle :", "si le joueur ne nomme pas la station en 1 seconde, le mesh n'est pas encore bon."], "small", 24));
  return svgDoc("Regles de lisibilite et silhouettes", "Silhouettes de reference et regles de lisibilite pour les stations du sous-marin.", body.join(""));
}

function plate04() {
  const body = [header("Planche 4 - Graybox en 4 volumes", "Blocage spatial minimal du premier sous-marin. Le plan et le profil partagent la meme grille en X.")];
  body.push(rect(60, 150, 1120, 470, "panel", 20));
  body.push(textBlock(86, 188, ["Vue dessus de blocage"], "h1", 30));
  const topY1 = 250;
  const topY2 = 560;
  body.push(hullTop(topY1, topY2));
  body.push(deckGrid(topY1 + 16, topY2 - 16));
  body.push(bulkheads(topY1 + 28, topY2 - 28));
  body.push(rect(GRID.innerL, 286, GRID.b1 - GRID.innerL, 238, "ballFill", 22));
  body.push(rect(GRID.b1, 286, GRID.b2 - GRID.b1, 238, "navFill", 22));
  body.push(rect(GRID.b2, 286, GRID.b3 - GRID.b2, 238, "propFill", 22));
  body.push(rect(GRID.b3, 286, GRID.innerR - GRID.b3, 238, "confFill", 22));
  body.push(rect(230, 356, 748, 92, "pathBand", 18));
  body.push(pathEl("M250 402 L1000 402", "dim", 'marker-end="url(#arrow)"'));
  body.push(textBlock(430, 395, ["Bande de circulation principale"], "small", 22));
  body.push(rect(214, 324, 132, 56, "tank", 14));
  body.push(textBlock(230, 358, ["Ballast avant"], "mono", 22));
  body.push(rect(228, 454, 134, 50, "panelAlt", 14));
  body.push(textBlock(244, 485, ["Service ballast"], "small", 22));
  body.push(pathEl("M470 492 Q520 428 570 492 L570 522 L470 522 Z", "navSolid"));
  body.push(textBlock(498, 472, ["Helm"], "mono", 22));
  body.push(rect(448, 324, 150, 50, "senseSolid", 12));
  body.push(textBlock(470, 356, ["Radar / sonar"], "mono", 22));
  body.push(rect(654, 324, 150, 54, "propSolid", 14));
  body.push(textBlock(680, 358, ["Station moteur"], "mono", 22));
  body.push(rect(666, 446, 176, 58, "panelAlt", 14));
  body.push(textBlock(690, 478, ["Bloc moteur"], "text", 24));
  body.push(rect(870, 324, 118, 54, "ballSolid", 14));
  body.push(textBlock(890, 358, ["Pompe"], "mono", 22));
  body.push(rect(866, 446, 96, 58, "tank", 14));
  body.push(textBlock(882, 478, ["Ballast arriere"], "small", 20));
  body.push(rect(896, 324, 124, 52, "confSolid", 14));
  body.push(textBlock(918, 356, ["Local arriere"], "small", 20));
  body.push(rect(964, 446, 72, 58, "panelAlt", 12));
  body.push(textBlock(977, 478, ["Tronc", "de sas"], "small", 20));
  body.push(pathEl("M190 228 L1058 228", "dim", 'marker-start="url(#arrow)" marker-end="url(#arrow)"'));
  body.push(textBlock(522, 214, ["Longueur utile interieure : 12,4 m"], "mono", 22, "middle"));
  body.push(pathEl("M90 194 L1184 194", "dim", 'marker-start="url(#arrow)" marker-end="url(#arrow)"'));
  body.push(textBlock(634, 180, ["Longueur exterieure cible : 15,2 m"], "mono", 22, "middle"));
  body.push(rect(60, 650, 1120, 540, "panel", 20));
  body.push(textBlock(86, 688, ["Vue de profil de blocage"], "h1", 30));
  const sideMid = 895;
  body.push(hullSide(sideMid));
  body.push(deckGrid(sideMid - 78, sideMid + 74));
  body.push(bulkheads(sideMid - 70, sideMid + 58));
  body.push(rect(210, 840, 156, 82, "ballFill", 12));
  body.push(rect(226, 924, 130, 42, "tank", 12));
  body.push(textBlock(244, 950, ["Ballast avant"], "mono", 20));
  body.push(rect(412, 832, 150, 88, "navFill", 12));
  body.push(pathEl("M472 928 Q506 884 540 928 L540 952 L472 952 Z", "navSolid"));
  body.push(textBlock(438, 866, ["Helm + poste"], "small", 22));
  body.push(rect(622, 826, 216, 96, "propFill", 12));
  body.push(rect(654, 864, 122, 52, "panelAlt", 12));
  body.push(textBlock(684, 896, ["Bloc moteur"], "small", 22));
  body.push(rect(804, 924, 90, 40, "tank", 12));
  body.push(textBlock(818, 949, ["Ballast arr."], "small", 20));
  body.push(rect(870, 828, 118, 92, "confFill", 12));
  body.push(textBlock(892, 866, ["Local arriere"], "small", 22));
  body.push(rect(932, 710, 62, 112, "confFill", 12));
  body.push(rect(946, 676, 34, 28, "confSolid", 8));
  body.push(textBlock(1008, 694, ["Ecoutille"], "small", 22));
  body.push(textBlock(902, 744, ["Tronc", "de sas"], "small", 22));
  body.push(line(963, 726, 963, 812, "whiteStroke"));
  body.push(line(950, 746, 976, 746, "whiteStroke"));
  body.push(line(950, 770, 976, 770, "whiteStroke"));
  body.push(line(950, 794, 976, 794, "whiteStroke"));
  body.push(polygon("232,780 248,812 216,812", "danger"));
  body.push(textBlock(264, 804, ["Bande de brèche avant"], "small", 22));
  body.push(polygon("694,762 710,794 678,794", "danger"));
  body.push(textBlock(730, 786, ["Bande de brèche plafond"], "small", 22));
  body.push(polygon("1032,778 1048,810 1016,810", "danger"));
  body.push(textBlock(1064, 802, ["Point haut expose"], "small", 22));
  body.push(rect(1210, 150, 530, 470, "panel", 20));
  body.push(textBlock(1238, 188, ["Regles du blocage"], "h1", 30));
  body.push(textBlock(1238, 240, ["1. Quatre volumes seulement.", "2. Trois bulkheads lisibles.", "3. Une bande centrale pour la circulation.", "4. Le moteur reste hors de l'axe principal.", "5. Le premier sas est un tronc dorsal simple.", "6. Ballast avant et ballast arriere existent deja au graybox."], "text", 34));
  body.push(textBlock(1238, 470, ["Decision :", "le premier proto ne tente pas encore un vrai micro-etage.", "On reserve simplement la masse du tronc dorsal pour l'evolution future."], "small", 24));
  body.push(rect(1210, 650, 530, 540, "panel", 20));
  body.push(textBlock(1238, 688, ["Cotes a respecter en modelisation"], "h1", 30));
  body.push(textBlock(1238, 744, ["Longueur exterieure cible : 15,2 m", "Longueur utile interieure : 12,4 m", "Passage principal : 2,2 m a 2,3 m", "Porte claire : 1,2 m x 2,1 m", "Hauteur libre interieure : 2,35 m", "Empreinte minimale du tronc : 1,1 m x 1,4 m", "Largeur d'echelle : 0,7 m a 0,8 m", "Frontages reparables : 1,3 m min sur murs utiles"], "text", 34));
  body.push(textBlock(1238, 1048, ["Validation :", "si un joueur ne sait pas ou couper la propagation,", "ou il repond pour plonger, le blocage est encore mauvais."], "small", 24));
  return svgDoc("Graybox en 4 volumes", "Blocage du premier sous-marin avec plan et profil alignes.", body.join(""));
}

function plate05() {
  const body = [header("Planche 5 - Grille de test gameplay et ergonomie", "Questions de validation du premier sous-marin. A lancer sur graybox brut, avant details et avant deco.")];
  body.push(rect(60, 150, 1680, 1080, "panel", 20));
  body.push(textBlock(90, 194, ["Les 6 tests a faire en jeu"], "h1", 30));
  const cols = [90, 390, 810, 1205];
  ["Question", "Action a faire", "Signal attendu", "Echec a surveiller"].forEach((head, idx) => body.push(textBlock(cols[idx], 248, [head], "h2", 28)));
  body.push(line(84, 266, 1716, 266, "tableLine"));
  body.push(line(360, 266, 360, 1112, "tableLine"));
  body.push(line(780, 266, 780, 1112, "tableLine"));
  body.push(line(1176, 266, 1176, 1112, "tableLine"));
  const rows = [
    ["Ou vais-je pour plonger ?", "Entrer depuis le spawn, sans HUD, et chercher la commande de profondeur.", "Le joueur va au helm puis comprend le ballast sans hesiter.", "Il se perd entre helm, radar et ballast."],
    ["Ou puis-je couper la propagation ?", "Provoquer une voie d'eau puis chercher la porte utile.", "La bulkhead door se lit comme decision de confinement.", "La porte ressemble a un mur decoratif ou a un simple passage."],
    ["Ou puis-je reparer ?", "Chercher la surface de reparation en condition d'alarme.", "Le mur reserve se repere vite, sans props parasites.", "Le point reparable est enfoui sous tuyaux, armoires ou clutter."],
    ["Qu'est-ce qui devient dangereux si la coque casse ?", "Casser une zone avant, puis une zone plafond, puis la zone haute arriere.", "Le danger change de place et de nature : fuite, succion, point haut expose.", "Toutes les pannes se ressemblent et aucun endroit ne raconte le risque."],
    ["Deux joueurs se genent-ils ou se coordonnent-ils ?", "Mettre un joueur au helm, un autre a la pompe, puis forcer un deplacement vers la porte.", "Les trajectoires se croisent peu et la coordination reste lisible.", "Les deux joueurs bloquent la meme bande et les stations se mangent entre elles."],
    ["L'axe avant / arriere reste-t-il clair ?", "Faire le test dans le noir ou avec l'eau qui monte.", "Le sens de la proue et du local arriere reste evident.", "Une fois en panique, tout l'interieur devient symetrique."],
  ];
  let y = 324;
  rows.forEach(([q, action, success, fail]) => {
    const qLines = wrapLines(q, 28);
    const aLines = wrapLines(action, 44);
    const sLines = wrapLines(success, 42);
    const fLines = wrapLines(fail, 40);
    const maxLines = Math.max(qLines.length, aLines.length, sLines.length, fLines.length);
    const rowH = Math.max(110, 38 + maxLines * 26);
    body.push(textBlock(cols[0], y, qLines, "text", 24));
    body.push(textBlock(cols[1], y, aLines, "small", 24));
    body.push(textBlock(cols[2], y, sLines, "small", 24));
    body.push(textBlock(cols[3], y, fLines, "small", 24));
    body.push(line(84, y + rowH - 20, 1716, y + rowH - 20, "tableLine"));
    y += rowH;
  });
  body.push(rect(90, 1128, 500, 88, "panelAlt", 16));
  body.push(circle(122, 1172, 12, "ok"));
  body.push(wrappedText(150, 1170, "Succes : le joueur nomme le bon endroit et s'y rend sans tutoriel.", 48, "small", 22));
  body.push(rect(620, 1128, 500, 88, "panelAlt", 16));
  body.push(circle(652, 1172, 12, "warn"));
  body.push(wrappedText(680, 1170, "A revoir : le joueur comprend, mais trop lentement ou avec HUD uniquement.", 48, "small", 22));
  body.push(rect(1150, 1128, 566, 88, "panelAlt", 16));
  body.push(circle(1182, 1172, 12, "danger"));
  body.push(wrappedText(1210, 1170, "Echec : il ne sait pas ou aller, ou l'espace bloque la cooperation.", 54, "small", 22));
  return svgDoc("Grille de test gameplay et ergonomie", "Table de test pour valider la lisibilite et l'ergonomie du premier sous-marin.", body.join(""), 1800, 1320);
}

function plate06() {
  const body = [header("Planche 6 - Ordre anti-panique pour produire les assets", "Decoupage du travail pour reduire la charge mentale. On fabrique d'abord ce qui rend le jeu lisible et testable.")];
  const phases = [
    ["Niveau 0", "Structure", "2 meshes", PALETTE.muted, ["SM_HullShell_Ext_A", "SM_HullShell_Int_A"], "Sans coque interieure et exterieure, aucun repere n'existe."],
    ["Niveau 1", "Confinement", "3 meshes", PALETTE.conf, ["SM_BulkheadFrame_A", "SM_BulkheadDoor_A", "SM_HatchTrunk_A"], "Ces meshes donnent le rythme spatial et rendent les compartiments reels."],
    ["Niveau 2", "Reperes de jeu", "5 meshes", PALETTE.nav, ["SM_HelmConsole_A", "SM_BallastConsole_A", "SM_EngineBlock_A", "SM_PumpModule_A", "SM_RepairWallPanel_A"], "Si ceux-la sont bons, le sous-marin devient deja jouable."],
    ["Niveau 3", "Kit de soutien", "8 a 10 meshes", PALETTE.prop, ["SM_ValveManifold_A", "SM_PowerCabinet_A", "SM_CableTray_A", "SM_PipeStraight_A", "SM_PipeElbow_A"], "Ils enrichissent sans remettre en cause le layout valide."],
    ["Niveau 4", "Extensions", "facultatif proto", PALETTE.sense, ["SM_RadarMast_A", "SM_TurretBase_A", "SM_RadarConsole_A"], "Seulement quand le proto principal est stable."],
  ];
  let y = 170;
  phases.forEach(([lvl, name, count, color, assets, why]) => {
    body.push(rect(60, y, 1680, 182, "panel", 20));
    body.push(`<rect x="60" y="${y}" width="18" height="182" fill="${color}" rx="8"/>`);
    body.push(textBlock(98, y + 42, [lvl, name], "h1", 28));
    body.push(textBlock(316, y + 40, [count], "mono", 24));
    body.push(wrappedText(470, y + 40, assets.join(", "), 62, "text", 24));
    body.push(textBlock(470, y + 116, [why], "small", 24));
    y += 206;
  });
  const footerY = y + 18;
  body.push(rect(60, footerY, 820, 160, "panelAlt", 18));
  body.push(textBlock(88, footerY + 42, ["Regle psychologique"], "h2", 28));
  body.push(textBlock(88, footerY + 86, ["Tu ne fabriques pas 'un sous-marin entier'.", "Tu fabriques d'abord 10 meshes qui rendent le sous-marin jouable."], "text", 28));
  body.push(rect(920, footerY, 820, 160, "panelAlt", 18));
  body.push(textBlock(948, footerY + 42, ["Interdit avant validation"], "h2", 28));
  body.push(textBlock(948, footerY + 86, ["Ne pas passer du temps sur : boulons hero, clutter, tuyaux tres fins, micro variantes,", "matiere finale complexe, decalcomanies de lore, greeble de tourelle."], "text", 28));
  return svgDoc("Ordre anti-panique pour produire les assets", "Ordre de travail pragmatique pour produire les assets du premier sous-marin.", body.join(""), 1800, 1420);
}

function plate07() {
  const body = [header("Planche 7 - Architecture precise du premier sous-marin", "Implantation detaillee en plan et en profil. La planche verrouille les besoins reels avant le design de surface.")];
  body.push(rect(60, 150, 1120, 500, "panel", 20));
  body.push(textBlock(86, 188, ["Plan d'implantation"], "h1", 30));
  const topY1 = 250;
  const topY2 = 590;
  body.push(hullTop(topY1, topY2));
  body.push(deckGrid(topY1 + 16, topY2 - 16));
  body.push(bulkheads(topY1 + 28, topY2 - 28));
  body.push(rect(GRID.innerL, 296, GRID.b1 - GRID.innerL, 248, "ballFill", 22));
  body.push(rect(GRID.b1, 296, GRID.b2 - GRID.b1, 248, "navFill", 22));
  body.push(rect(GRID.b2, 296, GRID.b3 - GRID.b2, 248, "propFill", 22));
  body.push(rect(GRID.b3, 296, GRID.innerR - GRID.b3, 248, "confFill", 22));
  body.push(rect(228, 370, 724, 94, "pathBand", 18));
  body.push(pathEl("M248 417 L972 417", "dim", 'marker-end="url(#arrow)"'));
  body.push(textBlock(444, 406, ["Spine de circulation", "joueur 1 <-> joueur 2"], "small", 22));
  body.push(textBlock(212, 332, ["A - Volume avant", "ballast avant + service"], "h2", 24));
  body.push(rect(220, 350, 132, 58, "tank", 14));
  body.push(textBlock(236, 384, ["Ballast avant"], "mono", 22));
  body.push(rect(224, 470, 150, 52, "panelAlt", 14));
  body.push(textBlock(242, 500, ["Vannes + manifold"], "small", 20));
  body.push(pathEl("M205 308 L205 532", "repair"));
  body.push(textBlock(214, 560, ["Frontage reparable reserve"], "small", 20));
  body.push(textBlock(424, 332, ["B - Volume central", "helm + radar"], "h2", 24));
  body.push(pathEl("M482 510 Q520 458 558 510 L558 540 L482 540 Z", "navSolid"));
  body.push(textBlock(498, 492, ["Helm"], "mono", 22));
  body.push(rect(454, 350, 160, 52, "senseSolid", 12));
  body.push(textBlock(478, 382, ["Radar / sonar"], "mono", 22));
  body.push(rect(448, 544, 156, 34, "panelAlt", 12));
  body.push(textBlock(470, 566, ["Zone de stance"], "small", 20));
  body.push(pathEl("M592 308 L592 532", "repair"));
  body.push(textBlock(634, 332, ["C - Volume machine", "moteur + pompe + ballast arr."], "h2", 24));
  body.push(rect(654, 350, 150, 54, "propSolid", 14));
  body.push(textBlock(676, 382, ["Station moteur"], "mono", 22));
  body.push(rect(664, 462, 180, 60, "panelAlt", 14));
  body.push(textBlock(690, 494, ["Bloc moteur"], "text", 22));
  body.push(rect(862, 350, 120, 54, "ballSolid", 14));
  body.push(textBlock(884, 382, ["Pompe"], "mono", 22));
  body.push(rect(856, 462, 104, 58, "tank", 14));
  body.push(textBlock(868, 492, ["Ballast", "arriere"], "small", 20));
  body.push(pathEl("M864 308 L864 532", "repair"));
  body.push(textBlock(872, 332, ["D - Volume arriere", "local de sas + tronc"], "h2", 24));
  body.push(rect(886, 350, 116, 54, "confSolid", 14));
  body.push(textBlock(908, 382, ["Service"], "mono", 22));
  body.push(rect(980, 458, 64, 70, "panelAlt", 12));
  body.push(textBlock(992, 486, ["Echelle", "+ acces"], "small", 20));
  body.push(pathEl("M1022 308 L1022 532", "repair"));

  body.push(rect(60, 680, 1120, 510, "panel", 20));
  body.push(textBlock(86, 718, ["Profil et besoins verticaux"], "h1", 30));
  const sideMid = 930;
  body.push(hullSide(sideMid));
  body.push(deckGrid(sideMid - 82, sideMid + 72));
  body.push(bulkheads(sideMid - 70, sideMid + 58));
  body.push(rect(214, 874, 152, 82, "ballFill", 12));
  body.push(rect(228, 958, 128, 42, "tank", 12));
  body.push(textBlock(244, 984, ["Ballast avant"], "mono", 20));
  body.push(rect(416, 866, 150, 88, "navFill", 12));
  body.push(pathEl("M478 962 Q512 916 546 962 L546 986 L478 986 Z", "navSolid"));
  body.push(textBlock(438, 900, ["Poste de commandement"], "small", 22));
  body.push(rect(622, 860, 216, 96, "propFill", 12));
  body.push(rect(654, 898, 126, 52, "panelAlt", 12));
  body.push(textBlock(686, 930, ["Bloc moteur"], "small", 22));
  body.push(rect(804, 958, 90, 42, "tank", 12));
  body.push(textBlock(818, 983, ["Ballast arr."], "small", 20));
  body.push(rect(870, 862, 124, 92, "confFill", 12));
  body.push(textBlock(890, 900, ["Local de sas"], "small", 22));
  body.push(rect(938, 744, 60, 112, "confFill", 12));
  body.push(rect(950, 706, 36, 30, "confSolid", 8));
  body.push(textBlock(1008, 724, ["Ecoutille plafond"], "small", 22));
  body.push(textBlock(910, 780, ["Tronc dorsal"], "small", 22));
  body.push(line(968, 760, 968, 846, "whiteStroke"));
  body.push(line(956, 782, 980, 782, "whiteStroke"));
  body.push(line(956, 806, 980, 806, "whiteStroke"));
  body.push(line(956, 830, 980, 830, "whiteStroke"));
  body.push(polygon("234,806 250,838 218,838", "danger"));
  body.push(textBlock(268, 832, ["Breche avant = fuite + succion locale"], "small", 22));
  body.push(polygon("700,790 716,822 684,822", "danger"));
  body.push(textBlock(736, 816, ["Breche plafond = danger vertical"], "small", 22));
  body.push(polygon("1030,804 1046,836 1014,836", "danger"));
  body.push(textBlock(1062, 830, ["Point haut = zone la plus exposee"], "small", 22));
  body.push(pathEl("M190 824 L1058 824", "dim", 'marker-start="url(#arrow)" marker-end="url(#arrow)"'));
  body.push(textBlock(624, 808, ["12,4 m utiles"], "mono", 22, "middle"));

  body.push(rect(1210, 150, 530, 1040, "panel", 20));
  body.push(textBlock(1238, 188, ["Besoins verrouilles par le code"], "h1", 30));
  body.push(textBlock(1238, 246, ["1. Deux masses de ballast existent deja : avant et arriere.", "2. Le helm reste le repere interieur principal.", "3. Le moteur et la pompe travaillent ensemble mais doivent se distinguer.", "4. Les portes sont du gameplay de confinement, pas du decor.", "5. Les frontages de reparation doivent rester reserves.", "6. Le premier sas recommande est dorsal, simple, a ecoutille."], "text", 34));
  body.push(textBlock(1238, 520, ["Dimensions de reference"], "h2", 28));
  body.push(textBlock(1238, 562, ["Longueur exterieure : 15,2 m", "Longueur utile : 12,4 m", "Passage clair : 2,2 m a 2,3 m", "Porte claire : 1,2 m x 2,1 m", "Empreinte tronc : 1,1 m x 1,4 m", "Largeur echelle : 0,7 m a 0,8 m"], "text", 32));
  body.push(textBlock(1238, 792, ["Choix retenu"], "h2", 28));
  body.push(textBlock(1238, 834, ["On construit maintenant :", "local arriere + tronc dorsal + ecoutille simple.", "On reserve l'enveloppe pour evoluer plus tard", "vers un micro-etage avec vrai sas a deux portes."], "text", 32));
  body.push(textBlock(1238, 1012, ["Ce qu'il ne faut pas faire"], "h2", 28));
  body.push(textBlock(1238, 1054, ["Ne pas caser dans D : sas, tourelle, trunk, service et decor au meme niveau.", "Ne pas faire un volume arriere trop petit pour y comprendre l'echelle.", "Ne pas casser la spine centrale avec des machines saillantes."], "small", 26));
  return svgDoc("Architecture precise du premier sous-marin", "Implantation precise du premier sous-marin en plan et en profil.", body.join(""));
}

function plate08() {
  const body = [header("Planche 8 - Systeme de symboles et regles de lecture", "Cette planche explique la grammaire graphique. Chaque signe a une seule signification et reste stable d'une page a l'autre.")];
  body.push(rect(60, 150, 760, 1020, "panel", 20));
  body.push(textBlock(88, 188, ["Legende des symboles"], "h1", 30));
  const legend = [
    ["ballFill", "Compartiment ballast / pompage", "zone dediee a l'eau, au pompage et au trim."],
    ["navFill", "Compartiment commande", "volume ou le helm sert de repere principal."],
    ["propFill", "Compartiment machine", "masse lourde, energie, moteur, bruit."],
    ["confFill", "Compartiment confinement / sas", "porte, service arriere, tronc, fermeture."],
    ["tank", "Ballast", "masse d'eau identifiee. Toujours avant ou arriere."],
    ["panelAlt", "Surface technique", "mur ou bloc de maintenance, jamais lecture principale."],
  ];
  let y = 246;
  legend.forEach(([cls, label, desc]) => {
    body.push(rect(96, y - 20, 88, 52, cls, 12));
    body.push(textBlock(210, y, [label], "h2", 24));
    body.push(textBlock(210, y + 30, [desc], "small", 22));
    y += 104;
  });
  body.push(line(88, 874, 792, 874, "tableLine"));
  body.push(textBlock(88, 916, ["Autres signes"], "h2", 28));
  body.push(line(120, 970, 204, 970, "bulkhead"));
  body.push(line(162, 948, 162, 992, "doorGap"));
  body.push(textBlock(228, 964, ["Cloison + ouverture de porte"], "h2", 24));
  body.push(textBlock(228, 994, ["Le trait epais dit : decision de confinement."], "small", 22));
  body.push(pathEl("M114 1048 L206 1048", "repair"));
  body.push(textBlock(228, 1042, ["Frontage reparable"], "h2", 24));
  body.push(textBlock(228, 1072, ["Toujours contre un mur ou un panneau reserve."], "small", 22));
  body.push(polygon("132,1122 148,1154 116,1154", "danger"));
  body.push(textBlock(228, 1144, ["Triangle danger / breche"], "h2", 24));
  body.push(textBlock(228, 1174, ["Signale fuite, succion ou exposition verticale."], "small", 22));

  body.push(rect(860, 150, 880, 620, "panel", 20));
  body.push(textBlock(888, 188, ["Exemple minimal de lecture correcte"], "h1", 30));
  body.push(rect(900, 282, 640, 260, "frame", 72));
  body.push(line(1038, 300, 1038, 524, "grid"));
  body.push(line(1220, 300, 1220, 524, "grid"));
  body.push(line(1388, 300, 1388, 524, "grid"));
  body.push(line(1080, 316, 1080, 506, "bulkhead"));
  body.push(line(1080, 392, 1080, 450, "doorGap"));
  body.push(line(1318, 316, 1318, 506, "bulkhead"));
  body.push(line(1318, 392, 1318, 450, "doorGap"));
  body.push(rect(938, 332, 190, 190, "ballFill", 18));
  body.push(rect(1130, 332, 200, 190, "navFill", 18));
  body.push(rect(1332, 332, 158, 190, "propFill", 18));
  body.push(rect(1492, 332, 108, 190, "confFill", 18));
  body.push(rect(960, 362, 132, 56, "tank", 14));
  body.push(textBlock(975, 396, ["Ballast avant"], "mono", 20));
  body.push(pathEl("M1182 498 Q1214 452 1246 498 L1246 522 L1182 522 Z", "navSolid"));
  body.push(rect(1180, 360, 124, 48, "senseSolid", 12));
  body.push(rect(1372, 360, 108, 48, "propSolid", 14));
  body.push(rect(1492, 454, 84, 52, "confSolid", 14));
  body.push(rect(1444, 360, 86, 48, "ballSolid", 14));
  body.push(rect(1542, 454, 62, 70, "panelAlt", 12));
  body.push(rect(984, 416, 530, 66, "pathBand", 18));
  body.push(pathEl("M946 338 L946 514", "repair"));
  body.push(pathEl("M1260 338 L1260 514", "repair"));
  body.push(pathEl("M1494 338 L1494 514", "repair"));
  body.push(polygon("1160,298 1176,330 1144,330", "danger"));
  body.push(textBlock(1190, 324, ["Exemple : danger plafond"], "small", 22));
  body.push(textBlock(888, 662, ["Lecture attendue :", "on identifie d'abord les volumes,", "puis la spine, puis les stations, puis les dangers."], "small", 24));

  body.push(rect(860, 810, 880, 360, "panel", 20));
  body.push(textBlock(888, 848, ["Regles d'usage"], "h1", 30));
  body.push(textBlock(888, 900, ["1. Un symbole = une fonction. Ne jamais reutiliser le meme signe pour deux sens.", "2. Le plan et le profil partagent la meme grille en X.", "3. Le texte ne compense pas un symbole flou. Si le texte devient long, le signe est mauvais.", "4. Les couleurs marquent la famille. La forme marque l'objet exact.", "5. Une porte etanche doit toujours rester plus lisible qu'un tuyau ou qu'une lampe.", "6. Si deux libelles se chevauchent, la densite de la planche est deja trop forte."], "text", 34));
  return svgDoc("Systeme de symboles et regles de lecture", "Legende de symboles et mini exemple de lecture pour les planches du sous-marin.", body.join(""));
}

function plate09() {
  const body = [header("Planche 9 - Sas : option retenue et evolution future", "Comparaison franche entre le sas dorsal simple du premier proto et le micro-etage a deux portes envisage plus tard.")];
  body.push(rect(60, 150, 810, 1000, "panel", 20));
  body.push(textBlock(88, 188, ["Option A - Recommandee maintenant"], "h1", 30));
  body.push(textBlock(88, 230, ["Local arriere + tronc dorsal + ecoutille plafond"], "small", 24));
  body.push(rect(122, 448, 620, 188, "frame", 94));
  body.push(rect(222, 488, 110, 82, "ballFill", 12));
  body.push(rect(414, 482, 110, 88, "navFill", 12));
  body.push(rect(612, 476, 150, 96, "propFill", 12));
  body.push(rect(690, 352, 58, 110, "confFill", 12));
  body.push(rect(704, 316, 32, 28, "confSolid", 8));
  body.push(textBlock(760, 334, ["Ecoutille"], "small", 22));
  body.push(textBlock(654, 386, ["Tronc"], "small", 22));
  body.push(line(719, 372, 719, 454, "whiteStroke"));
  body.push(line(707, 392, 731, 392, "whiteStroke"));
  body.push(line(707, 416, 731, 416, "whiteStroke"));
  body.push(line(707, 440, 731, 440, "whiteStroke"));
  body.push(textBlock(88, 720, ["Avantages :", "1. Compatible avec le code actuel et avec la lecture en 4 volumes.", "2. Faible cout de mesh, faible risque de layout.", "3. Laisse intacte la bande de circulation principale.", "4. Suffisant pour un premier prototype testable."], "text", 34));
  body.push(textBlock(88, 950, ["Contrainte :", "le volume arriere doit quand meme etre dimensionne", "pour qu'une future version a deux portes reste possible."], "small", 24));

  body.push(rect(930, 150, 810, 1000, "panel", 20));
  body.push(textBlock(958, 188, ["Option B - Evolution future"], "h1", 30));
  body.push(textBlock(958, 230, ["Micro-etage superieur avec vrai sas a deux portes"], "small", 24));
  body.push(rect(1002, 448, 620, 188, "frame", 94));
  body.push(rect(1204, 490, 118, 82, "confFill", 12));
  body.push(rect(1240, 360, 76, 116, "confFill", 12));
  body.push(rect(1252, 316, 52, 32, "confSolid", 8));
  body.push(line(1278, 378, 1278, 464, "whiteStroke"));
  body.push(line(1266, 402, 1290, 402, "whiteStroke"));
  body.push(line(1266, 426, 1290, 426, "whiteStroke"));
  body.push(textBlock(1334, 338, ["Porte ext."], "small", 22));
  body.push(textBlock(1334, 404, ["Sas haut"], "small", 22));
  body.push(textBlock(1334, 526, ["Porte int."], "small", 22));
  body.push(textBlock(958, 720, ["Interet :", "1. Plus riche pour sortie, pression et procedures.", "2. Plus fort en fantasy sous-marin classique.", "3. Ouvre du gameplay specifique de verrouillage."], "text", 34));
  body.push(textBlock(958, 900, ["Couts :", "1. Volume vertical supplementaire a assumer.", "2. Plus d'assets et plus de contraintes de circulation.", "3. Plus de travail pour rester lisible et ergonomique."], "small", 24));

  body.push(rect(60, 1180, 1680, 70, "panelAlt", 18));
  body.push(textBlock(90, 1222, ["Verdict : construire l'Option A maintenant, mais reserver la masse et les ancrages pour migrer plus tard vers l'Option B sans refaire toute la coque."], "text", 26));
  return svgDoc("Sas : option retenue et evolution future", "Comparatif entre le sas dorsal simple et le micro-etage a deux portes.", body.join(""));
}

const PLATES = {
  "02-kit-assets-systemes.svg": plate02,
  "03-regles-lisibilite-silhouettes.svg": plate03,
  "04-graybox-4-volumes.svg": plate04,
  "05-grille-test-gameplay-ergonomie.svg": plate05,
  "06-ordre-anti-panique-assets.svg": plate06,
  "07-architecture-precise-plan-profile.svg": plate07,
  "08-grammar-symbols-reading.svg": plate08,
  "09-sas-options-recommendation.svg": plate09,
};

function main() {
  fs.mkdirSync(OUT_DIR, { recursive: true });
  Object.entries(PLATES).forEach(([fileName, factory]) => {
    fs.writeFileSync(path.join(OUT_DIR, fileName), factory(), "utf8");
  });
  console.log(`Generated ${Object.keys(PLATES).length} SVG files in ${OUT_DIR}`);
}

main();
