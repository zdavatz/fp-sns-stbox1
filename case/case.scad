// =========================================================================
// PumpLogger Case — v3 — 2026-05-22
//
// Architecture (Peter's sketches 2026-05-21 + train reviews 2026-05-22):
//   - Parting plane JUST BELOW the ST PCB → lid off = SD card + USB access.
//   - BODY = shallow hollow tray (~11.5mm): floor, Qi coil, battery, the
//     ST-PCB standoffs. LID = deep hollow box (~26.5mm): covers ST-PCB top,
//     the standing GPS module, the GPS antenna (glued to the lid roof).
//   - Both hollow shells → stiff → even O-ring compression.
//   - GPS module PCB 31×51, 11mm thick, STANDS on its 51mm edge → 31mm tall
//     (the box-height driver).
//
// SCREWS OUTSIDE THE SEAL (Peter, 2026-05-22):
//   A 2.5mm wall cannot host a ~9mm M3 screw boss, so:
//   - The O-ring groove WEAVES INWARD around each corner screw — the seal
//     "drives around" the screw, leaving it outside the sealed zone.
//   - Each screw sits on a solid corner BOSS (floor→rim post) that intrudes
//     a few mm into the cavity corner (the ST PCB has scalloped corners
//     there, and battery/Qi sit central — so the corners are free).
//   - Box outline stays a clean rounded rectangle, no external lobes.
//
// STAGE 1 = shells + wavy-seal + corner screws. Stage 2: montage wall,
// lanyard, internal mounts (ST standoffs, GPS holder, Hall console).
//
// OpenSCAD:  F5 preview · F6 render · File→Export→STL
// =========================================================================


// ===== RENDER SELECTOR =====================================================
RENDER = "exploded";   // "body" | "lid" | "exploded" | "assembled"

body_color = "DodgerBlue";
lid_color  = "red";              // opak — Details besser sichtbar als translucent


// ===== COMPONENTS (mm) =====================================================
qi_x = 49;  qi_y = 32;  qi_z = 1;
bat_x = 38; bat_y = 24; bat_z = 8;

// SensorTile.box PRO PCB — BARE outline aus Gerber. Long axis entlang Box-X.
st_x = 57.5;    // long side (X)
st_y = 35.6;    // short side (Y)
st_z = 11;      // PCB + Bauteile

// Konnektor-Überstände über die PCB-Kante (Peter 2026-05-26):
usb_overhang = 2.0;  // USB-C: 2mm raus an der rechten kurzen Seite (X+)
sd_overhang  = 1.0;  // SD-Slot: 1mm raus an der FERNEN Längsseite (Y=ext_y, gegenüber GPS)

// (scallop_inset war für die alte vollzylindrische Schrauben-Variante,
// nicht mehr nötig — PCB-Pfosten sind 1/4-Kreise an den PCB-Ecken selbst.)

// GPS-Modul — NEU 2026-05-28: kleines flaches SMD-Modul mit 16×28 mm
// Footprint, ~5 mm dick, wird ans Lid-Dach geklebt (neben die Antenne).
// Treibt die Gehäusehöhe NICHT mehr (alt: stehendes 11×31×51 mm Modul).
gps_x = 28; gps_y = 16; gps_z = 5;
gant_x = 25; gant_y = 26; gant_z = 8;
hall_x = 12; hall_y = 8;  hall_z = 1;

// Y=0-seitiger Reserve-Strip im Cavity (war der alte GPS-Stehschacht, 11 mm).
// Bewusst auf 11 mm belassen — Cavity-Breite bleibt unverändert. Strip
// beherbergt jetzt das Hall-PCB (12×8) + das flache GPS-Modul (16×28) +
// etwas Reserve / Future-Proofing.
y0_reserve_strip = 11;


// ===== SHELL ===============================================================
wall     = 3.0;       // bumped from 2.5 for robustness
floor_th = 3.0;
roof_th  = 3.0;
clr      = 1.0;
corner_r = 8.0;


// ===== O-RING (here — forward refs from PLAN section below) ================
// rim_inset/rim_clear need groove_w + seal_edge_inset + rim_lip → defined here
// up front. OpenSCAD 2021.01 doesn't resolve forward references for variables.
oring_cord      = 1.5;
groove_d        = 1.05;    // 30% Quetschung
groove_w        = 2.1;     // 80% Füllgrad
seal_edge_inset = 3.0;
rim_lip         = 1.0;
lid_flange_h    = 3.0;     // war 2.0 — bei 3 ist die Cavity-Verbreiterung 45° = FDM-druckbar selbsttragend


// ===== PLAN — internal cavity ==============================================
rim_inset = seal_edge_inset + groove_w/2 + rim_lip;     // 5.05
rim_clear = rim_inset - wall + 1.0;                     // 3.05

// X: Asymmetrisch — PCB-Linke-Kante MAXIMAL nach links (nur 0.5mm passage
// für Lid-Senkung), USB-Seite kriegt das frei werdende Raum.
pcb_left_passage  = 0.5;   // PCB-links: minimaler Sicherheitsabstand
pcb_right_passage = 1.0;   // USB-Seite: standard passage_clr
in_x = (rim_inset - wall) + pcb_left_passage + st_x + usb_overhang
       + (rim_inset - wall) + pcb_right_passage;        // 2.05+0.5+57.5+2+2.05+1 = 65.1

// Y: Y=0-Seite hat den Reserve-Strip (Hall-PCB + flaches GPS-Modul + Reserve).
// PCB-Pfosten sind 1/4-Kreise nach +Y (in PCB Mitte) → sie ragen NICHT in
// Richtung Strip → Spalt clr_strip_pcb = 1 mm.
clr_strip_pcb = 1.0;
in_y = rim_clear + y0_reserve_strip + clr_strip_pcb + st_y + sd_overhang + rim_clear;  // 3.05+11+1+35.6+1+3.05 = 54.7


// ===== VERTICAL ============================================================
// 2026-05-28: Neue Höhenableitung. Der alte stehende GPS-Modul (51 mm hoch)
// gab das Lid vor. Das neue flache Modul (16×28×5) wird ans Dach geklebt
// und fällt als Höhentreiber weg. Lid-Höhe = PCB-Komponenten-Stack im Lid
// + Antenne am Dach + Reserve.
//
// Body-Höhe weiterhin durch standoff_h (Qi + Batterie + Reserve) bestimmt.
standoff_h = 10.5;                                       // = qi_z + bat_z + 1.5 reserve
// Trennebene 2.5 mm UNTER der PCB-Unterseite → PCB+Komponenten liegen
// entspannt über dem Lid-Rim-Bereich, mehr Reserve für SD/USB.
body_in_z  = standoff_h - 2.5;                           // 8 — parting plane

// Lid-Höhentreiber: PCB-Top über Trennebene + Antennen-Tiefe + Reserve.
//   PCB-Unterseite (standoff_h - body_in_z) = 2.5 mm über Trennebene.
//   PCB+Bauteile st_z = 11 mm → PCB-Top = 13.5 mm über Trennebene.
//   Antenne (gant_z = 8 mm) klebt am Lid-Dach. Footprint 25×26 mm passt
//   NICHT komplett in den 11 mm Reserve-Strip → überlappt zwingend die
//   PCB-Säule → braucht clearance darüber.
pcb_top_above_parting = (standoff_h - body_in_z) + st_z;          // 13.5
lid_roof_clearance    = 1.0;                                       // Antennen-Unterseite ↔ PCB-Top
lid_in_z   = pcb_top_above_parting + gant_z + lid_roof_clearance;  // 22.5
total_in   = body_in_z + lid_in_z;                                  // 30.5


// ===== EXTERNAL ============================================================
ext_x = in_x + 2 * wall;                  // 66
ext_y = in_y + 2 * wall;                  // 56.5
body_ext_z = body_in_z + floor_th;        // 11.5  — parting plane height
lid_ext_z  = lid_in_z + roof_th;          // 26.5


// ===== O-RING + SEAL PATH — see top of file (moved up for OpenSCAD 2021.01) =
// oring_cord, groove_d/w, seal_edge_inset, rim_lip, lid_flange_h sind oben
// definiert (vor PLAN). Hier nur Doku:
// - O-Ring EPDM 72 × 1.5, ~1.7% Vordehnung, 30% Quetschung, 80% Füllgrad
// - Seal-Pfad sauberes Rundrechteck (keine Screw-Detours)


// ===== SCREWS (4 corner LOBES, OUTSIDE the seal) ===========================
insert_d = 4.5;  insert_h = 5.0;
m3_clear = 3.4;
// Countersunk-screw recess (DIN 7991 M3, 90° cone) — conical, not cylindrical
cs_top_d = 6.4;                          // recess diameter at the surface
cs_depth = (cs_top_d - m3_clear) / 2;    // 90° cone → depth = radius difference

// Corner lobe geometry — small cylindrical bulges at the 4 corners that
// host the M3 inserts OUTSIDE the seal loop. Each lobe extends diagonally
// out from the main box outline at the corner, fully rounded.
lobe_r                = 4.0;     // lobe cylinder radius (insert 2.25 + 1.75 wall)
screw_diagonal_offset = 10.4;    // screw distance from corner-arc-centre along
                                  // diagonal (= seal_corner_r + groove_w/2 + rim_lip
                                  //   + insert_r + clearance = 5+1.4+1+2.25+0.75)


// ===== LANYARD EYE (on X=0 short end, mid-height) ==========================
eye_od       = 8.0;     // D-shape disc outer diameter
eye_id       = 3.0;     // through-hole (2.5mm paracord)
eye_th       = 4.0;     // disc thickness (along Y axis)
eye_protrude = 4.0;     // how far the disc sticks out in -X


// ===== BOARD-MOUNT FEATURES (on Y=0 long side, vertical) ===================
// 2 spring-steel-strip guide channels. Each channel is a FULLY ENCLOSED
// rectangular tube spanning body+lid:
//   - closed at body floor (positive Z stop)
//   - open at the parting plane (body & lid channels join when assembled)
//   - closed at lid top, ending spring_slot_top_cap mm below the box top
// Strip insertion: with the lid OFF, slide the strip down into the body
// channel from above; then close the lid (it encloses the upper portion).
// 2-3mm of material surrounds the channel on all 4 sides for robustness.
// An OUTWARD BUMP on the wall adds the material needed since the lid wall
// alone (3mm) is too thin. Bump runs body+lid as a continuous column.

// Strip is 0.6 × 20 mm spring steel. CAD slot needs strip nominal + 0.5-0.7mm
// extra so it slides easily after FDM shrinkage (printed holes come out tight).
spring_slot_w         = 20.6;   // channel width along X (= strip 20 + 0.6mm CAD slack)
spring_slot_d         = 1.2;    // channel depth in Y (= strip 0.6 + 0.6mm CAD slack)
spring_slot_floor     = 3.5;    // 0.5mm ÜBER dem Floor-Top → kein sichtbarer Spalt im Boden,
                                 // Strip-Boden sitzt 0.5mm über Body-Innenfloor.
spring_slot_top_cap   = 5.0;    // material above channel at the lid top
spring_bump_outward   = 3.0;    // bump protrusion (-Y) beyond the wall
spring_bump_margin    = 2.0;    // bump width margin beside the channel (X)
// Channels pushed as far OUT as possible toward the corners, and their
// bumps MERGE with the corner lobes (no Einbuchtung between bump and lobe).
// Limit: channel must not cut into the screw insert. With insert at
// screw_xy() and r=insert_d/2, the channel edge can be ~1mm past the
// insert outer → bump left edge ends up inside the lobe footprint at the
// outer wall → seamless bump+lobe bulge.
spring_slot_offset_from_corner = 14;
spring_slot_x1 = spring_slot_offset_from_corner;
spring_slot_x2 = ext_x - spring_slot_offset_from_corner;   // mirror symmetric

// Dovetail-Schiene in der Mitte zwischen den beiden Federblech-Schächten.
// Durchgehend vertikal von Boden bis Lid-Top (positive feature aussen am Y=0).
dt_rail_w_base = 5.0;       // Basis-Breite (am Wall)
dt_rail_w_top  = 8.0;       // Aussenseite-Breite (breiter → Dovetail-Form)
dt_rail_depth  = 3.0;       // Auskragung in -Y Richtung
dt_rail_x      = ext_x / 2;  // X-Zentrum der Schiene (= Box-Mitte)

// Screw position = corner-arc-centre (corner_r, corner_r) shifted diagonally
// OUT toward the box corner by screw_diagonal_offset. Result is OUTSIDE the
// main box outline → screw lives in the lobe that bulges past the outline.
function screw_xy() =
  let (s = corner_r - screw_diagonal_offset / sqrt(2))   // ≈ 0.65 — close to box corner
  [
    [s,         s        ],
    [ext_x - s, s        ],
    [s,         ext_y - s],
    [ext_x - s, ext_y - s],
  ];

// Corner lobe — small cylindrical bulge at screw position, hosts insert.
module corner_lobe(p, h) {
  translate([p[0], p[1], 0])
    cylinder(h = h, r = lobe_r, $fn = 48);
}


// ===== 2D HELPERS ==========================================================
module rrect2d(w, h, r) {
  hull() for (px = [r, w-r], py = [r, h-r])
    translate([px, py]) circle(r = r, $fn = 48);
}

// CLEAN rounded-rectangle seal shape — NO screw detours (screws are in
// external lobes, outside the seal loop). Smooth perimeter = reliable seal.
module seal_shape(inset) {
  translate([inset, inset])
    rrect2d(ext_x - 2*inset, ext_y - 2*inset,
            max(corner_r - inset, 0.5));
}

// O-ring groove band (= outer band − inner band)
module groove_2d() {
  difference() {
    seal_shape(seal_edge_inset - groove_w/2);
    seal_shape(seal_edge_inset + groove_w/2);
  }
}

// Cavity opening at the rim — inboard of the groove (+ a small lip).
module rim_opening_2d() {
  seal_shape(seal_edge_inset + groove_w/2 + rim_lip);
}


// ===== 3D HELPER ===========================================================
module rbox(x, y, z, r) {
  hull() for (px = [r, x-r], py = [r, y-r])
    translate([px, py, 0]) cylinder(h = z, r = r, $fn = 64);
}


// ===== PCB STANDOFFS (Stage 2b) ============================================
// PCB hat KEINE Bohrungen — nur 1/4-Kreis Einschnitte an den 4 Ecken.
// PCB wird NICHT verschraubt sondern GEKLEMMT zwischen Pfosten-Absatz und Lid.
// Pro Ecke: 1/4-Kreis-Pfosten mit STUFE:
//   - Lower (Boden → PCB-Unterseite): R_lower > Scallop-R → PCB ruht auf
//     der Stufe (Material aussen vom Scallop liegt auf dem Pfosten-Top)
//   - Upper (PCB-Unterseite → +upper_h): R_upper ≤ Scallop-R → fittet in
//     den Scallop, hält die PCB horizontal in Position
//   - Quartiers-Orientierung: Richtung PCB-Mitte (= weg von Box-Ecke)
pcb_scallop_r      = 3.45;    // aus Gerber GKO (13581 unit-arcs ≈ 3.45 mm)
pcb_pillar_lower_r = 5.0;     // breiter als Scallop → PCB-Material auf dem Absatz
pcb_pillar_upper_r = 3.3;     // schmaler als Scallop, 0.15 mm clearance
pcb_pillar_upper_h = 2.0;     // 2mm über PCB-Unterseite (= durch PCB-Dicke ~1.6mm)

// PCB-Position in Box-Coords. PCB BL-Ecke: ASYMMETRISCH, PCB nach links
// geschoben (pcb_left_passage statt rim_clear), USB-Seite bekommt mehr Raum.
function pcb_corner_BL_box() = [
  wall + (rim_inset - wall) + pcb_left_passage,                 // = 3 + 2.05 + 0.5 = 5.55
  wall + rim_clear + y0_reserve_strip + clr_strip_pcb,           // = 18.05 unverändert
];

// 4 PCB-Eckenpositionen in BOX coords (= Ecken des bare PCB-Rechtecks)
function pcb_corner_xy() =
  let (p = pcb_corner_BL_box())
  [
    [p[0],        p[1]       ],   // BL
    [p[0] + st_x, p[1]       ],   // BR
    [p[0],        p[1] + st_y],   // TL
    [p[0] + st_x, p[1] + st_y],   // TR
  ];

// Quadrant-Richtung für jede Ecke (zur PCB-Mitte hin)
function pcb_corner_dirs() = [
  [+1, +1],   // BL → +X, +Y
  [-1, +1],   // BR → -X, +Y
  [+1, -1],   // TL → +X, -Y
  [-1, -1],   // TR → -X, -Y
];

// 1/4-Kreis-Pfosten mit Absatz + optionalen Merge-Brackets zu Wänden.
// pos       = PCB-Eckposition in Box-Coords
// dir       = [dx, dy] Quadrant-Richtung zur PCB-Mitte (+/-1)
// merge_x   = Box-X-Koord der X-Seitenwand zum Mergen (oder undef)
// merge_y   = Box-Y-Koord der Y-Wand zum Mergen (oder undef — NIE GPS-Wand!)
module pcb_pillar(pos, dir, merge_x = undef, merge_y = undef) {
  bracket_h     = body_in_z;            // bracket nur bis Body-Top, nicht in Lid
  bracket_overlap = 0.5;                // 0.5mm überlappen ins Wand-Material

  union() {
    // Lower quarter + Upper quarter (Pfosten)
    translate([pos[0], pos[1], floor_th])
      intersection() {
        union() {
          cylinder(h = standoff_h, r = pcb_pillar_lower_r, $fn = 48);
          translate([0, 0, standoff_h])
            cylinder(h = pcb_pillar_upper_h, r = pcb_pillar_upper_r, $fn = 48);
        }
        translate([dir[0] < 0 ? -50 : -0.01,
                   dir[1] < 0 ? -50 : -0.01, -0.01])
          cube([50, 50, standoff_h + pcb_pillar_upper_h + 1]);
      }

    // X-side merge bracket
    if (!is_undef(merge_x)) {
      bx1 = merge_x < pos[0] ? merge_x - bracket_overlap : pos[0];
      bx2 = merge_x < pos[0] ? pos[0]                    : merge_x + bracket_overlap;
      by1 = dir[1] > 0 ? pos[1] : pos[1] - pcb_pillar_lower_r;
      by2 = dir[1] > 0 ? pos[1] + pcb_pillar_lower_r : pos[1];
      translate([bx1, by1, floor_th])
        cube([bx2 - bx1, by2 - by1, bracket_h]);
    }

    // Y-side merge bracket
    if (!is_undef(merge_y)) {
      by1 = merge_y < pos[1] ? merge_y - bracket_overlap : pos[1];
      by2 = merge_y < pos[1] ? pos[1]                    : merge_y + bracket_overlap;
      bx1 = dir[0] > 0 ? pos[0] : pos[0] - pcb_pillar_lower_r;
      bx2 = dir[0] > 0 ? pos[0] + pcb_pillar_lower_r : pos[0];
      translate([bx1, by1, floor_th])
        cube([bx2 - bx1, by2 - by1, bracket_h]);
    }
  }
}


// ===== EXTERNAL FEATURES (Stage 2a) ========================================

// Lanyard eye — D-shape lug on the X=0 short end, mid-height. Cylinder
// along the Y axis, half embedded in the wall, half protruding in -X.
module lanyard_eye_positive() {
  translate([0, ext_y / 2, body_ext_z / 2])
    rotate([90, 0, 0])
      cylinder(h = eye_th, d = eye_od, center = true, $fn = 32);
}
module lanyard_eye_hole() {
  // Vertical through-hole in the protruding half
  translate([-eye_protrude / 2, ext_y / 2, body_ext_z / 2])
    cylinder(h = eye_od + 1, d = eye_id, center = true, $fn = 24);
}

// Outward bump on Y=0 wall at a channel position. Adds material so the
// 1mm channel inside has 2.5mm material on the outer side (in the bump)
// + 2.5mm on the inner side (in the wall). Width = channel + 2× margin.
module spring_bump(x_center, height) {
  bump_w = spring_slot_w + 2 * spring_bump_margin;
  translate([x_center - bump_w / 2, -spring_bump_outward, 0])
    cube([bump_w, spring_bump_outward + 0.01, height]);   // +0.01 mm overlap into shell
}

// Dovetail-Schiene zwischen den beiden Federblech-Schächten (positiv).
module dovetail_rail(height) {
  translate([dt_rail_x, 0, 0])
    hull() {
      translate([-dt_rail_w_base / 2, -0.01, 0])
        cube([dt_rail_w_base, 0.02, height]);
      translate([-dt_rail_w_top / 2, -dt_rail_depth, 0])
        cube([dt_rail_w_top, 0.02, height]);
    }
}

// Channel cut in body: from z = spring_slot_floor (closed bottom) up to
// the body top (open at the parting plane — joins lid channel).
// Channel Y range: −d to 0 (= entirely inside the bump material). Outer
// side = 2mm of bump material; inner side = the original wall.
module spring_channel_body(x_center) {
  translate([x_center - spring_slot_w / 2,
             -spring_slot_d, spring_slot_floor])
    cube([spring_slot_w, spring_slot_d,
          body_ext_z - spring_slot_floor + 0.5]);
}

// Channel cut in lid: OPEN AT BOTH ENDS. The bump is spring_slot_top_cap
// mm shorter than the lid, so above the bump the area outside Y=0 is free
// — strip enters from above, the 180° bend wraps over the bump's top
// corner, outer leg comes down OUTSIDE the box.
module spring_channel_lid(x_center) {
  translate([x_center - spring_slot_w / 2,
             -spring_slot_d, -0.5])
    cube([spring_slot_w, spring_slot_d, lid_ext_z + 1]);
}


// ===== BODY (shallow tray) =================================================
// Cavity is UNIFORM width (= rim opening) over the full height — no inward
// flange, so there is NO downward-facing overhang. The body wall is thick
// (~5mm) but the body only holds Qi/battery/GPS-module, which fit the
// smaller opening. Printed floor-down, open side up → the O-ring groove is
// just an upward trench (self-supporting).
module body() color(body_color)
  union() {
    // Body shell with all cuts — but WITHOUT the PCB standoffs (they'd
    // get sliced by the cavity cut otherwise).
    difference() {
      union() {
        // Outer shell
        rbox(ext_x, ext_y, body_ext_z, corner_r);
        // 4 corner lobes — host the screw inserts OUTSIDE the seal loop
        for (p = screw_xy()) corner_lobe(p, body_ext_z);
        // Stage 2a — positive external features
        lanyard_eye_positive();
        // Spring-channel bumps (Y=0 wall, body height)
        spring_bump(spring_slot_x1, body_ext_z);
        spring_bump(spring_slot_x2, body_ext_z);
        // Dovetail-Schiene in der Mitte (durchgehend body + lid)
        dovetail_rail(body_ext_z);
      }

      // Uniform cavity (rim-opening shape, full height)
      translate([0, 0, floor_th])
        linear_extrude(body_ext_z) rim_opening_2d();

      // O-ring groove (clean rounded rect) — trench in the top rim face
      translate([0, 0, body_ext_z - groove_d])
        linear_extrude(groove_d + 1) groove_2d();

      // Screw insert pockets (top) + through-bore to floor
      for (p = screw_xy()) {
        translate([p[0], p[1], body_ext_z - insert_h])
          cylinder(h = insert_h + 1, d = insert_d, $fn = 24);
        translate([p[0], p[1], -0.1])
          cylinder(h = body_ext_z - insert_h + 0.2, d = m3_clear, $fn = 24);
      }

      // Stage 2a — negative external features
      lanyard_eye_hole();
      spring_channel_body(spring_slot_x1);
      spring_channel_body(spring_slot_x2);
    }

    // Stage 2b — PCB-Pfosten (1/4-Kreise mit Absatz) an den 4 PCB-Ecken.
    // NACH dem cavity-cut hinzugefügt → bleiben intakt.
    // Merge-Brackets verbinden Pfosten mit Seitenwänden für Steifigkeit:
    //   - GPS-Seiten-Pfosten (BL, BR): nur mit X-Wand mergen (NICHT mit Y=0,
    //     dort ist das GPS-Modul)
    //   - SD-Seiten-Pfosten (TL, TR): mit BEIDEN nahen Wänden mergen
    pcb_pillar(pcb_corner_xy()[0], pcb_corner_dirs()[0],
               merge_x = rim_inset);                        // BL: -X wall
    pcb_pillar(pcb_corner_xy()[1], pcb_corner_dirs()[1],
               merge_x = ext_x - rim_inset);                // BR: +X wall
    pcb_pillar(pcb_corner_xy()[2], pcb_corner_dirs()[2],
               merge_x = rim_inset, merge_y = ext_y - rim_inset);   // TL: both
    pcb_pillar(pcb_corner_xy()[3], pcb_corner_dirs()[3],
               merge_x = ext_x - rim_inset, merge_y = ext_y - rim_inset); // TR
  }


// ===== LID (deep hollow box) ===============================================
// The lid must hold the wide ST PCB, so its cavity steps OUT from the
// sealing rim. That step is given a 45° chamfer → self-supporting, no
// overhang. Modelled roof-up; printed roof-on-bed (sealing rim faces up).
module lid()
  color(lid_color)
    difference() {
      union() {
        // Hollow lid: outer shell − cavity
        difference() {
          rbox(ext_x, ext_y, lid_ext_z, corner_r);

          // Cavity, bottom → top — sharp step at lid_flange_h (no chamfer
          // in this iteration; 3mm overhang at the ledge → slicer support
          // or accept slight droop on a non-critical inner face).
          translate([0, 0, -0.1])
            linear_extrude(lid_flange_h + 0.2) rim_opening_2d();
          translate([wall, wall, lid_flange_h - 0.1])
            rbox(in_x, in_y, lid_in_z - lid_flange_h + 0.3, corner_r - wall);
        }

        // 4 corner lobes — same as body, host the screw through-holes
        // OUTSIDE the seal loop.
        for (p = screw_xy()) corner_lobe(p, lid_ext_z);

        // Spring-channel bumps — SHORTER than lid by spring_slot_top_cap
        // (5mm). Strip enters from above, 180° bend wraps over the bump's
        // top corner; outer leg comes down past the box on the outside.
        spring_bump(spring_slot_x1, lid_ext_z - spring_slot_top_cap);
        spring_bump(spring_slot_x2, lid_ext_z - spring_slot_top_cap);
        // Dovetail-Schiene durchgehend mit body — volle Lid-Höhe
        dovetail_rail(lid_ext_z);
      }

      // Screw through-holes + 90° conical countersink (DIN 7991 screws)
      for (p = screw_xy()) {
        translate([p[0], p[1], -0.1])
          cylinder(h = lid_ext_z + 0.2, d = m3_clear, $fn = 24);
        translate([p[0], p[1], lid_ext_z - cs_depth])
          cylinder(h = cs_depth + 0.1, d1 = m3_clear, d2 = cs_top_d, $fn = 24);
      }

      // Spring-channel cuts (matching body's, joined at the parting plane)
      spring_channel_lid(spring_slot_x1);
      spring_channel_lid(spring_slot_x2);
    }


// ===== RENDER ==============================================================
if (RENDER == "body")
  body();
else if (RENDER == "lid")
  // Standalone-Lid: flippe so dass Rim/Nut/Schraubensitzungen nach OBEN
  // zeigen für die Inspektion. Y-Achse wird dabei gespiegelt — Features
  // auf Y=0 erscheinen visuell auf der anderen Seite.
  translate([0, 0, lid_ext_z]) rotate([180, 0, 0]) lid();
else if (RENDER == "exploded") {
  body();
  // Lid in NATÜRLICHER Orientierung über dem Body (Rim unten, Dach oben).
  // Keine Rotation — sonst landen die Bumps auf der falschen Y-Seite.
  translate([0, 0, body_ext_z + 20]) lid();
}
else if (RENDER == "assembled") {
  body();
  translate([0, 0, body_ext_z]) lid();
}
else
  echo("Unknown RENDER:", RENDER);

// Sanity check: standoff_h must be at least Qi + Battery + 1mm margin
echo("##### RELOAD MARKER 2026-05-26 14:55 #####");
echo(str("Standoff_h=", standoff_h, " | Qi+Bat=", qi_z+bat_z, " | margin=", standoff_h-qi_z-bat_z, " mm"));
echo(str("Main external:    ", ext_x, " x ", ext_y, " x ", body_ext_z + lid_ext_z, " mm"));
echo(str("With corner lobes: ~", ext_x + 2*(lobe_r - corner_r + screw_diagonal_offset/sqrt(2)),
         " x ", ext_y + 2*(lobe_r - corner_r + screw_diagonal_offset/sqrt(2)), " mm bounding"));
echo(str("Cavity:            ", in_x, " x ", in_y, " mm"));
echo(str("rim_opening:       ", in_x - 2*(rim_clear-1), " x ", in_y - 2*(rim_clear-1), " mm"));
echo(str("O-ring:            70 x 2 EPDM | groove ", groove_w, " x ", groove_d, " mm"));
echo(str("Compression: 30%, Fill: 80%, Stretch: 3.3% (see case/DESIGN.md §7)"));
