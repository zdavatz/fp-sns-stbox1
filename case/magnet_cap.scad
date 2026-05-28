// =========================================================================
// PumpLogger Magnet-Cap — v0 — 2026-05-20
//
// Bright red cover that snap-clips over the case lid. Has an embedded
// neodymium magnet that activates the Hall sensor inside the box, putting
// the box into OFF state. Remove cap → box ON.
//
// Design philosophy:
//   - WHOLE-LID coverage in signal red = impossible to forget while pumping
//   - Snap tabs on the two SHORT ends grip the lid → won't fall off
//   - Print in red PLA or PETG
//   - Tactile UX: clear "click" on insertion, requires firm pull to remove
//
// Must match case.scad's lid dimensions. Import the case.scad parameters
// for guaranteed compatibility.
// =========================================================================

use <case.scad>

// ====== Import constants from case.scad ===================================
// (kept duplicated here for OpenSCAD's scope rules — case.scad's globals
// aren't auto-visible. Edit BOTH if you change them.)

ext_x      = 75;   // lid external X (must match case.scad)
ext_y      = 42.5; // lid external Y
corner_r   = 6.0;
wall       = 2.0;
top_th     = 2.0;
lid_in_z   = 13;
lid_ext_z  = lid_in_z + top_th;

// Cap geometry
cap_thickness    = 3.0;       // top plate thickness (carries magnet)
cap_overhang     = 2.0;       // how much the cap extends beyond the lid edges
cap_tab_z        = 7.0;       // how far down the tabs reach (must cover snap recess)

// Snap-tab geometry (must match cap_snap_* in case.scad's lid)
cap_snap_w   = 30;            // tab width along the lid short edge
cap_snap_h   = 2;             // nub height
cap_snap_d   = 0.8;           // nub protrusion inward
cap_snap_z   = 4;             // nub Z position from cap top

// Magnet pocket — N42 neodymium disc, 8mm diameter × 3mm height
magnet_d         = 8.0;
magnet_h         = 3.0;
magnet_pocket_clearance = 0.2;   // press-fit clearance

// Hall sensor position in CASE coordinates — pulled from case.scad
// (this is where the magnet must be aligned with — i.e., centred above
// the Hall sensor)
hall_pos = [ext_x - wall - 6, ext_y / 2];   // ≈ matches case.scad's hall_sensor_xy_in_body

// Optional: embossed text on cap top
emboss_text   = "OFF";
emboss_depth  = 0.8;
emboss_height = 6;            // character size


// ====== HELPER MODULES ====================================================

module rounded_box(x, y, z, r) {
  hull() {
    for (px = [r, x-r], py = [r, y-r])
      translate([px, py, 0]) cylinder(h=z, r=r, $fn=48);
  }
}


// ====== CAP ===============================================================

module magnet_cap() {
  // Cap covers the full lid + small overhang for grip
  cap_x = ext_x + 2 * cap_overhang;
  cap_y = ext_y + 2 * cap_overhang;
  cap_r = corner_r + cap_overhang;

  difference() {
    union() {
      // Top plate
      rounded_box(cap_x, cap_y, cap_thickness, cap_r);
      // Two side tabs on the SHORT ends, hanging down to grip the lid
      for (side = [0, 1])
        translate([side == 0 ? 0 : cap_x - 2 * cap_overhang - wall - 0.5,
                   (cap_y - cap_snap_w) / 2 - 1,
                   cap_thickness])
          tab();
    }

    // Magnet pocket — hole on the inside (bottom) of the cap plate,
    // positioned over the Hall sensor
    translate([hall_pos[0] + cap_overhang, hall_pos[1] + cap_overhang,
               -0.1])
      cylinder(h=magnet_h + magnet_pocket_clearance,
               d=magnet_d + 2 * magnet_pocket_clearance,
               $fn=48);

    // Embossed OFF text on the OUTSIDE face of the cap (the side the
    // user sees). The +z face is where the tabs root, i.e., the inside
    // (touches the lid). Carve the text into the -z face instead.
    translate([cap_x / 2, cap_y / 2, -0.01])
      linear_extrude(emboss_depth + 0.1)
        text(emboss_text, size = emboss_height,
             halign = "center", valign = "center", font = "Arial Black");
  }
}

// One side tab — flexes to snap-in over the lid short edge.
module tab() {
  // Solid block running down beside the lid, with snap nub at the right Z
  difference() {
    cube([wall + 0.5, cap_snap_w + 2, cap_tab_z]);
    // Carve out the inside so it's a thin flexing wall
    translate([0, 0, -0.1])
      cube([wall, cap_snap_w + 2, cap_tab_z + 0.2]);
  }
  // The snap-nub itself — sticks INTO the cap interior toward the lid wall
  translate([0, 1, cap_tab_z - cap_snap_z - cap_snap_h])
    cube([cap_snap_d * 2, cap_snap_w, cap_snap_h]);
}


// ====== RENDER ============================================================

cap_color = "red";

color(cap_color) magnet_cap();

echo(str("Cap external: ", ext_x + 2*cap_overhang, " x ",
         ext_y + 2*cap_overhang, " x ", cap_thickness + cap_tab_z, " mm"));
echo(str("Magnet: ", magnet_d, " x ", magnet_h, " mm, glued in cap"));
echo(str("Hall position over lid (X,Y from cap origin): ",
         hall_pos[0] + cap_overhang, " mm, ",
         hall_pos[1] + cap_overhang, " mm"));
