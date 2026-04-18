const fs = require("fs");
const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  Header, Footer, AlignmentType, HeadingLevel, BorderStyle, WidthType,
  ShadingType, PageNumber, PageBreak, LevelFormat
} = require("docx");

// ─── Colors ───
const ACCENT = "1B3A4B";
const ACCENT_LIGHT = "D5E8F0";
const DARK = "2C2C2C";
const GRAY = "666666";
const WHITE = "FFFFFF";

// ─── Borders ───
const border = { style: BorderStyle.SINGLE, size: 1, color: "CCCCCC" };
const borders = { top: border, bottom: border, left: border, right: border };
const noBorders = {
  top: { style: BorderStyle.NONE, size: 0 },
  bottom: { style: BorderStyle.NONE, size: 0 },
  left: { style: BorderStyle.NONE, size: 0 },
  right: { style: BorderStyle.NONE, size: 0 },
};
const cellMargins = { top: 80, bottom: 80, left: 120, right: 120 };

// ─── Helpers ───
function h1(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 360, after: 200 },
    children: [new TextRun({ text, bold: true, size: 32, font: "Arial", color: ACCENT })],
  });
}

function h2(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 280, after: 160 },
    children: [new TextRun({ text, bold: true, size: 26, font: "Arial", color: ACCENT })],
  });
}

function p(text, opts = {}) {
  return new Paragraph({
    spacing: { after: 120 },
    children: [new TextRun({ text, size: 22, font: "Arial", color: opts.color || DARK, ...opts })],
  });
}

function bold(text) {
  return new TextRun({ text, bold: true, size: 22, font: "Arial", color: DARK });
}

function normal(text) {
  return new TextRun({ text, size: 22, font: "Arial", color: DARK });
}

function para(...runs) {
  return new Paragraph({ spacing: { after: 120 }, children: runs });
}

function specRow(label, value, highlight = false) {
  return new TableRow({
    children: [
      new TableCell({
        borders,
        width: { size: 3500, type: WidthType.DXA },
        margins: cellMargins,
        shading: { fill: highlight ? ACCENT_LIGHT : WHITE, type: ShadingType.CLEAR },
        children: [new Paragraph({ children: [new TextRun({ text: label, bold: true, size: 20, font: "Arial" })] })],
      }),
      new TableCell({
        borders,
        width: { size: 5860, type: WidthType.DXA },
        margins: cellMargins,
        children: [new Paragraph({ children: [new TextRun({ text: value, size: 20, font: "Arial" })] })],
      }),
    ],
  });
}

// ─── Document ───
const doc = new Document({
  styles: {
    default: { document: { run: { font: "Arial", size: 22, color: DARK } } },
    paragraphStyles: [
      {
        id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 32, bold: true, font: "Arial", color: ACCENT },
        paragraph: { spacing: { before: 360, after: 200 }, outlineLevel: 0 },
      },
      {
        id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 26, bold: true, font: "Arial", color: ACCENT },
        paragraph: { spacing: { before: 280, after: 160 }, outlineLevel: 1 },
      },
    ],
  },
  numbering: {
    config: [
      {
        reference: "bullets",
        levels: [{
          level: 0, format: LevelFormat.BULLET, text: "\u2022", alignment: AlignmentType.LEFT,
          style: { paragraph: { indent: { left: 720, hanging: 360 } } },
        }],
      },
      {
        reference: "numbers",
        levels: [{
          level: 0, format: LevelFormat.DECIMAL, text: "%1.", alignment: AlignmentType.LEFT,
          style: { paragraph: { indent: { left: 720, hanging: 360 } } },
        }],
      },
    ],
  },
  sections: [
    {
      properties: {
        page: {
          size: { width: 11906, height: 16838 },
          margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 },
        },
      },
      headers: {
        default: new Header({
          children: [new Paragraph({
            alignment: AlignmentType.RIGHT,
            children: [new TextRun({ text: "Sub3D \u2014 Submarine Blockout Brief", size: 18, font: "Arial", color: GRAY, italics: true })],
          })],
        }),
      },
      footers: {
        default: new Footer({
          children: [new Paragraph({
            alignment: AlignmentType.CENTER,
            children: [
              new TextRun({ text: "Page ", size: 18, font: "Arial", color: GRAY }),
              new TextRun({ children: [PageNumber.CURRENT], size: 18, font: "Arial", color: GRAY }),
            ],
          })],
        }),
      },
      children: [
        // ═══ TITLE ═══
        new Paragraph({
          alignment: AlignmentType.CENTER,
          spacing: { after: 80 },
          children: [new TextRun({ text: "FREELANCE BRIEF", size: 44, bold: true, font: "Arial", color: ACCENT })],
        }),
        new Paragraph({
          alignment: AlignmentType.CENTER,
          spacing: { after: 40 },
          children: [new TextRun({ text: "Submarine Interior Blockout \u2014 3D Modeling", size: 28, font: "Arial", color: DARK })],
        }),
        new Paragraph({
          alignment: AlignmentType.CENTER,
          spacing: { after: 400 },
          children: [new TextRun({ text: "Sub3D \u2014 Unreal Engine 5.7 Submarine Simulation Game", size: 20, font: "Arial", color: GRAY })],
        }),

        // ═══ PROJECT OVERVIEW ═══
        h1("1. Project Overview"),
        p("Sub3D is an indie submarine simulation game built in Unreal Engine 5.7. The player controls a crew inside a submarine, navigating compartments, managing flooding, repairing breaches, and operating stations."),
        p("We need a low-poly blockout mesh of the submarine interior and exterior hull for gameplay prototyping (First Playable milestone). This is NOT a final art asset \u2014 it is a structural blockout for gameplay validation."),

        // ═══ WHAT WE NEED ═══
        h1("2. Deliverables"),
        h2("2.1 One submarine mesh set:"),

        new Paragraph({
          numbering: { reference: "numbers", level: 0 },
          spacing: { after: 80 },
          children: [bold("Outer hull"), normal(" \u2014 single closed mesh, smooth exterior, 15cm wall thickness (solidify inward). Organic cigar shape with tapered bow/stern.")],
        }),
        new Paragraph({
          numbering: { reference: "numbers", level: 0 },
          spacing: { after: 80 },
          children: [bold("Deck plates"), normal(" \u2014 3 horizontal floors (upper, main, lower). Continuous, no holes. Variable width per deck level.")],
        }),
        new Paragraph({
          numbering: { reference: "numbers", level: 0 },
          spacing: { after: 80 },
          children: [bold("Bulkheads"), normal(" \u2014 vertical walls between compartments, with rectangular door openings (90\u00d7185cm). One template, duplicated per boundary.")],
        }),
        new Paragraph({
          numbering: { reference: "numbers", level: 0 },
          spacing: { after: 80 },
          children: [bold("10 compartments"), normal(" \u2014 interior rooms forming the submarine layout. Each is a box-room with walls, floor, ceiling, and door openings.")],
        }),
        new Paragraph({
          numbering: { reference: "numbers", level: 0 },
          spacing: { after: 80 },
          children: [bold("SAS/Airlock"), normal(" \u2014 small room on top of the upper deck (bridge area), accessible from below. Open face for exterior hatch door.")],
        }),

        h2("2.2 File format"),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("FBX export, centimeters, X-forward Y-lateral Z-up")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Each major element as a separate object (hull, each deck, each compartment, each bulkhead)")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Blender source file (.blend) included")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          spacing: { after: 200 },
          children: [normal("Clean topology, quads preferred, normals consistent")],
        }),

        // ═══ DIMENSIONS ═══
        h1("3. Dimensions & Layout"),

        new Table({
          width: { size: 9360, type: WidthType.DXA },
          columnWidths: [3500, 5860],
          rows: [
            specRow("Total length", "~44 meters (4400 cm)", true),
            specRow("Max hull diameter", "~8 meters (800 cm)"),
            specRow("Hull wall thickness", "15 cm (solidify inward)", true),
            specRow("Deck plate thickness", "18 cm"),
            specRow("Bulkhead thickness", "14 cm", true),
            specRow("Standard door opening", "90 cm wide \u00d7 185 cm tall"),
            specRow("Number of decks", "3 (upper, main, lower)", true),
            specRow("Number of compartments", "10 + 1 SAS"),
          ],
        }),

        new Paragraph({ spacing: { after: 200 }, children: [] }),

        h2("3.1 Compartment layout (side view)"),
        new Paragraph({
          spacing: { after: 200 },
          children: [new TextRun({
            text: `                    [SAS]
              [ NAV  ][ BRIDGE ]         <- Upper deck
  [TORP][SONAR][ CREW ][ MEDBAY ][ ENG ][PROP]  <- Main deck
              [ MACH ][ REACTOR]         <- Lower deck`,
            font: "Courier New", size: 18, color: DARK,
          })],
        }),

        h2("3.2 Compartment dimensions"),
        new Table({
          width: { size: 9360, type: WidthType.DXA },
          columnWidths: [2000, 1200, 1600, 1600, 1400, 1560],
          rows: [
            new TableRow({
              children: ["Name", "Deck", "Length (cm)", "Width (cm)", "Height (cm)", "Doors"].map(t =>
                new TableCell({
                  borders, margins: cellMargins,
                  shading: { fill: ACCENT, type: ShadingType.CLEAR },
                  width: { size: 1560, type: WidthType.DXA },
                  children: [new Paragraph({ children: [new TextRun({ text: t, bold: true, size: 18, font: "Arial", color: WHITE })] })],
                })
              ),
            }),
            ...([
              ["Torpedo",    "Main",  "400", "280", "200", "Aft only"],
              ["Sonar",      "Main",  "350", "360", "200", "Fore + Aft"],
              ["CrewQuarters","Main", "500", "480", "200", "Fore + Aft"],
              ["MedBay",     "Main",  "400", "480", "200", "Fore + Aft"],
              ["Engine",     "Main",  "400", "400", "200", "Fore + Aft"],
              ["Propulsion", "Main",  "350", "300", "200", "Fore only"],
              ["Navigation", "Upper", "500", "380", "195", "Fore + Aft"],
              ["Bridge",     "Upper", "400", "400", "195", "Fore + Aft"],
              ["Machinery",  "Lower", "500", "420", "180", "Fore + Aft"],
              ["Reactor",    "Lower", "400", "440", "180", "Fore + Aft"],
              ["SAS",        "Top",   "250", "220", "210", "None (vertical)"],
            ].map((row, i) =>
              new TableRow({
                children: row.map(t =>
                  new TableCell({
                    borders, margins: cellMargins,
                    shading: { fill: i % 2 === 0 ? WHITE : "F5F5F5", type: ShadingType.CLEAR },
                    width: { size: 1560, type: WidthType.DXA },
                    children: [new Paragraph({ children: [new TextRun({ text: t, size: 18, font: "Arial" })] })],
                  })
                ),
              })
            )),
          ],
        }),

        new Paragraph({ children: [new PageBreak()] }),

        // ═══ STYLE ═══
        h1("4. Visual Style & References"),
        p("This is a blockout, not final art. We need:"),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Clean, readable geometry \u2014 player must understand the space instantly")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Flat shading / simple materials (one color per compartment is fine)")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("No UV mapping needed (materials applied in UE)")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("No interior props/furniture (we add those later)")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          spacing: { after: 120 },
          children: [normal("Hull shape inspired by WW2 French submarines (Surcouf class) \u2014 organic, elongated, with a superstructure fairing on top")],
        }),

        h2("4.1 Reference submarines"),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Surcouf (1929) \u2014 elongated hull, prominent sail, multi-deck interior")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Barotrauma (game) \u2014 modular compartments, clear deck separation, gameplay-first layout")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          spacing: { after: 200 },
          children: [normal("Generic diesel-electric submarine silhouette \u2014 cigar body, tapered bow, stubby stern")],
        }),

        // ═══ CONSTRAINTS ═══
        h1("5. Technical Constraints"),
        para(bold("CRITICAL \u2014 "), normal("These dimensions are driven by gameplay code and cannot be changed:")),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Door openings: exactly 90cm wide \u00d7 185cm tall, centered on wall, sill at floor level")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Standing height: minimum 186cm floor-to-ceiling in every compartment")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Deck plates: must be SOLID \u2014 no holes for hatches (those are separate game actors)")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Hull: one continuous mesh, solidified inward. Player sees the interior surface of the hull.")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Each compartment: separate Blender object for individual export")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Coordinate system: X = forward (bow), Y = lateral, Z = up. Origin at submarine center.")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          spacing: { after: 200 },
          children: [normal("Scale: 1 Blender unit = 1 centimeter")],
        }),

        // ═══ NOT INCLUDED ═══
        h1("6. What is NOT needed"),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("No textures or UV maps")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("No interior props (chairs, consoles, pipes, etc.)")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("No rigging or animation")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("No LODs")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("No exterior details (periscope, rudder, propeller, etc.)")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          spacing: { after: 200 },
          children: [normal("No collision meshes (generated in-engine)")],
        }),

        // ═══ PROVIDED ═══
        h1("7. What We Provide"),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Blender script that generates the current blockout (for reference/starting point)")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("Reference images (Surcouf, Barotrauma, technical drawings)")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          children: [normal("This brief document with all dimensions")],
        }),
        new Paragraph({
          numbering: { reference: "bullets", level: 0 },
          spacing: { after: 200 },
          children: [normal("Direct communication for feedback during modeling")],
        }),

        // ═══ BUDGET & TIMELINE ═══
        h1("8. Budget & Timeline"),
        new Table({
          width: { size: 9360, type: WidthType.DXA },
          columnWidths: [3500, 5860],
          rows: [
            specRow("Budget range", "50\u2013150 EUR (negotiable based on quality)", true),
            specRow("Timeline", "3\u20135 days"),
            specRow("Revisions", "2 rounds included", true),
            specRow("Communication", "English or French"),
          ],
        }),

        new Paragraph({ spacing: { after: 400 }, children: [] }),

        new Paragraph({
          alignment: AlignmentType.CENTER,
          spacing: { after: 120 },
          border: { top: { style: BorderStyle.SINGLE, size: 2, color: ACCENT, space: 8 } },
          children: [new TextRun({ text: "Contact: via Fiverr/Upwork messaging", size: 22, font: "Arial", color: ACCENT, bold: true })],
        }),
        new Paragraph({
          alignment: AlignmentType.CENTER,
          children: [new TextRun({ text: "Blender scripts and reference images provided upon acceptance.", size: 20, font: "Arial", color: GRAY })],
        }),
      ],
    },
  ],
});

// ─── Generate ───
const OUTPUT = "C:/Dev/Sub3D/reports/Sub3D_Submarine_Blockout_Brief.docx";
Packer.toBuffer(doc).then(buffer => {
  fs.writeFileSync(OUTPUT, buffer);
  console.log(`Written: ${OUTPUT}`);
});
