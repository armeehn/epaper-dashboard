// =====================================================================
//  7.5" Tri-color e-Paper (Waveshare raw panel, ASIN B09JSFTGV6)
//  Desk + wall case for a custom ESP32 carrier board, TP4056-style
//  USB-C charger module, and LiPo pouch cell.
//  Units: mm  |  OpenSCAD 2021.01
// =====================================================================
//  PARTS - set `part` (or CLI: -D 'part="body"'):
//    "body"     front frame            (print face down, no supports)
//    "lid"      rear lid / chassis     (print back face down)
//    "plate"    ESP32-board mount plate(print flat, standoffs up)
//    "stand"    desk cradle            (print base down)
//    "assembly" preview everything
// =====================================================================

part = "assembly"; // ["body","lid","plate","stand","assembly"]

$fn = 48;
EPS = 0.01;

// ------------------ PANEL (datasheet values) -------------------------
panel_w   = 170.2;  // glass outline width
panel_h   = 111.2;  // glass outline height
panel_t   = 1.3;    // glass thickness incl. slack (nominal 1.25)
aa_w      = 163.2;  // active area width
aa_h      = 97.92;  // active area height
aa_top    = 3.4;    // TOP border: glass edge -> active area  ** MEASURE & TWEAK **
fpc_w     = 34;     // relief slot width for the ribbon (ribbon itself ~14)

// ------------------ CASE SHELL ---------------------------------------
clr       = 0.3;    // panel-to-pocket clearance per side
wall      = 2.6;    // outer wall thickness
bezel_t   = 2.0;    // front face thickness in front of the glass
reveal    = 1.0;    // window opening beyond active area, per side
win_ch    = 2.0;    // 45-deg outward chamfer around the window
foam_t    = 1.0;    // foam tape on panel rear border, compressed by lid rim
rim_len   = 17.0;   // interior depth for electronics
rim_t     = 2.0;    // lid rim wall thickness
lid_floor = 2.4;    // lid back plate thickness
corner_r  = 5.0;    // outer corner radius
pocket_r  = 1.2;    // pocket corner radius (small - panel corners are square)
lid_clr   = 0.25;   // lid-to-body sliding clearance per side

// ------------------ CUSTOM ESP32 CARRIER BOARD -----------------------
board_l        = 60;    // size along X (USB-C edge faces the right wall)
board_w        = 40;    // size along Y
board_hole_dx  = 53;    // mounting-hole spacing X  ** MEASURE YOUR BOARD **
board_hole_dy  = 33;    // mounting-hole spacing Y  ** MEASURE YOUR BOARD **
board_pilot    = 2.05;  // standoff pilot: 2.05 for M2.5 self-tap, 2.5 for M3
board_soff_h   = 2.5;   // board standoff height above the plate
board_gap_right= 1.0;   // board USB edge -> lid rim inner face
board_from_bot = 18;    // board lower edge above cavity inner bottom
usb_h          = 14.8;  // CENTER of the DevKitC USB-C above lid inner floor ** TWEAK **
usb_slot_w     = 14;    // wall slot width (Y) - sized to pass the plug overmold
usb_slot_h     = 7.5;   // wall slot height (Z)

// plate-to-lid interface (fixed, never re-measure)
plate_t       = 2.0;
plate_hole_dx = 40;
plate_hole_dy = 24;
plate_boss_h  = 3.5;

// ------------------ CHARGER (TP4056-style USB-C) ---------------------
chg_l         = 28.4;   // module length (USB edge to rim)
chg_w         = 17.9;   // module width
chg_pcb_t     = 1.7;
chg_rail_h    = 5.5;    // PCB rest height above inner floor (USB+LEDs face the floor)
chg_from_bot  = 68;     // module lower edge above cavity inner bottom
chg_usb_w     = 13.0;   // wall slot width  (fits the plug overmold)
chg_usb_h     = 7.0;    // wall slot height
led_slot      = true;   // peek slot in the lid over the charge LEDs

// ------------------ BATTERY ------------------------------------------
batt_l        = 51;     // pocket inner length (103450 pouch = 50x34x11)
batt_w        = 35;
batt_fence_h  = 7;
batt_fence_t  = 1.6;

// ------------------ WALL / DESK / FASTENERS --------------------------
keyhole_pitch = 80;     // horizontal spacing of the two wall keyholes
stand_len     = 150;
stand_tilt    = 12;     // backward lean
pillar        = 8.5;    // corner screw pillar size
lid_pilot     = 2.5;    // M3 self-tap pilot in the pillars

// =====================================================================
// DERIVED
// =====================================================================
pocket_w  = panel_w + 2*clr;                  // 170.8
pocket_h  = panel_h + 2*clr;                  // 111.8
panel_back= bezel_t + panel_t;
rim_front = panel_back + foam_t;              // lid rim presses foam here
depth     = rim_front + rim_len + lid_floor;  // total case depth
lid_iz    = depth - lid_floor;                // z of lid inner floor

aa_bottom = panel_h - aa_h - aa_top;          // FPC-side border (~9.9)
win_w     = aa_w + 2*reveal;
win_h     = aa_h + 2*reveal;
win_dy    = (aa_bottom - aa_top)/2;           // window sits this far above pocket center

// uniform picture-frame border all around the window.
// minimum is set by the wide FPC-side border of the glass.
band      = aa_bottom - reveal + clr + wall;  // ~11.8
outer_w   = max(win_w + 2*band, pocket_w + 2*wall);
outer_h   = win_h + 2*band;                   // shell is centered on the WINDOW
shell_dy  = win_dy;                           // shell center offset (pocket stays at y=0)

rim_ow = pocket_w - 2*lid_clr;
rim_oh = pocket_h - 2*lid_clr;
rim_iw = rim_ow - 2*rim_t;
rim_ih = rim_oh - 2*rim_t;
cav_x0 = -rim_iw/2;
cav_y0 = -rim_ih/2;

board_cx = rim_iw/2 - board_gap_right - board_l/2;
board_cy = cav_y0 + board_from_bot + board_w/2;
chg_cy   = cav_y0 + chg_from_bot + chg_w/2;
chg_usb_ctr = chg_rail_h - 1.6;               // charger USB center above inner floor (USB faces the floor)

px = pocket_w/2 - pillar/2 + 0.2;             // pillar centers
py = pocket_h/2 - pillar/2 + 0.2;

stack = plate_boss_h + plate_t + board_soff_h + 1.6 + 2 + 1.6 + 3.2;

assert(aa_bottom > 7, "aa_top looks wrong: bottom (FPC) border should be ~9.9");
assert(stack < rim_len - 0.5, str("component stack ", stack, " too tall for rim_len ", rim_len));
assert(usb_h + usb_slot_h/2 < rim_len + 2.6, "usb slot pokes past cavity depth");
assert(board_cx - board_l/2 > cav_x0 + 2, "board hits the left rim");
assert(board_cy - board_w/2 > cav_y0 + 1, "board hangs below cavity");
assert(board_cx - 1.75 + (board_l+4.5)/2 < rim_iw/2 - 0.4, "plate hits the lid rim");
echo(str(">> outer: ", outer_w, " x ", outer_h, " x ", depth, " mm, window ", win_w, " x ", win_h));
echo(str(">> component stack ", stack, " / ", rim_len, " mm; usb_h=", usb_h));

// =====================================================================
// HELPERS
// =====================================================================
module rrect(w, h, r) offset(r=r) square([w-2*r, h-2*r], center=true);
module rbox(w, h, d, r) linear_extrude(d) rrect(w, h, r);

// slot through the body's right wall, chamfered outside. (y,z) = center
module wall_slot(y, z, sw, sh, ch=1.5) {
    translate([pocket_w/2 - 1, y, z]) rotate([0,90,0])
        linear_extrude((outer_w - pocket_w)/2 + 2) rrect(sh, sw, 1.5);
    translate([outer_w/2 - 0.7, y, z]) rotate([0,90,0]) hull() {
        linear_extrude(EPS) rrect(sh, sw, 1.5);
        translate([0,0,0.7+EPS]) linear_extrude(EPS) rrect(sh+2*ch, sw+2*ch, 1.5+ch);
    }
}
// matching slot through the lid rim's right wall
module rim_slot(y, z, sw, sh) {
    translate([rim_iw/2 - 1, y, z]) rotate([0,90,0])
        linear_extrude(rim_t + 2) rrect(sh, sw, 1.5);
}

// =====================================================================
// FRONT BODY  (z=0 front face, +z rearward)
// =====================================================================
module body() {
    difference() {
        hull() {   // shell with a small front-edge chamfer, centered on the window
            translate([0,shell_dy,0.8]) rbox(outer_w, outer_h, depth-0.8, corner_r);
            translate([0,shell_dy,0])   rbox(outer_w-1.6, outer_h-1.6, depth, corner_r-0.8);
        }
        // window with 45-deg outward chamfer
        translate([0, win_dy, 0]) {
            hull() {
                translate([0,0,-EPS]) rbox(win_w+2*win_ch, win_h+2*win_ch, EPS, 2+win_ch);
                translate([0,0,bezel_t]) rbox(win_w, win_h, EPS, 2);
            }
            translate([0,0,bezel_t-EPS]) rbox(win_w, win_h, 1, 2);
        }
        // panel pocket + electronics cavity (straight walls to the back)
        translate([0,0,bezel_t]) rbox(pocket_w, pocket_h, depth, pocket_r);
        // port openings
        wall_slot(board_cy, lid_iz - usb_h, usb_slot_w, usb_slot_h);
        wall_slot(chg_cy, lid_iz - chg_usb_ctr, chg_usb_w, chg_usb_h, 1.2);
    }
    // corner screw pillars (fused to the walls; stop above the glass)
    for (sx=[-1,1], sy=[-1,1]) translate([sx*px, sy*py, 0])
        difference() {
            translate([0,0,panel_back+0.7]) rbox(pillar, pillar, lid_iz-panel_back-0.7, 2);
            translate([0,0,lid_iz-10.5]) cylinder(d=lid_pilot, h=11);
        }
}

// =====================================================================
// LID  (modeled in assembly coords: back face at z=depth)
// =====================================================================
module keyhole_neg() {   // local: z=0 outer face, +z inward, slot runs +y
    web = 2.2; d = 5.4;
    translate([0,0,-EPS]) cylinder(d=8.6, h=d);
    translate([-2.3, 0, -EPS]) cube([4.6, 9, d]);
    translate([0, 9, -EPS]) cylinder(d=4.6, h=d);
    translate([-4.3, 0, web]) cube([8.6, 9, d]);
    translate([0, 9, web]) cylinder(d=8.6, h=d);
}
module ziptie_bridge() {   // arch on the inner floor, extends -z from lid_iz
    translate([0,0,lid_iz]) rotate([180,0,0]) difference() {
        translate([-6,-4,0]) cube([12, 8, 4.6]);
        translate([-6-EPS,-2.2,-EPS]) cube([12.1, 4.4, 3.0]);
    }
}
module charger_mount() {   // at right rim, module slides in USB-first
    x0 = rim_iw/2;
    module dn(h) { translate([0,0,lid_iz-h]) children(); }   // helper: extrude down
    for (s=[-1,1]) {
        // pcb rest ledges
        dn(chg_rail_h) translate([x0-chg_l-1, s*(chg_w/2+0.25) - (s>0?2.5:0) - (s<0?0:0), 0])
            translate([0, s<0 ? 0 : 0, 0]) cube([chg_l, 2.5, chg_rail_h]);
        // side walls
        dn(chg_rail_h+chg_pcb_t+1.8) translate([x0-chg_l-1, s*(chg_w/2+0.25)+(s<0?-1.6:0), 0])
            cube([chg_l, 1.6, chg_rail_h+chg_pcb_t+1.8]);
        // retaining nubs (snap the pcb in)
        for (nx=[x0-6, x0-chg_l+4])
            translate([nx, s*(chg_w/2+0.25), lid_iz-chg_rail_h-chg_pcb_t-0.2])
                sphere(r=0.8, $fn=24);
    }
    // end stop
    dn(chg_rail_h+chg_pcb_t+2.6) translate([x0-chg_l-2.8, -chg_w/2-2, 0])
        cube([1.8, chg_w+4, chg_rail_h+chg_pcb_t+2.6]);
}
module lid_asm() {
    difference() {
        union() {
            translate([0,0,lid_iz]) rbox(rim_ow, rim_oh, lid_floor, 1.5);      // back plate
            difference() {                                                     // rim tube
                translate([0,0,rim_front]) rbox(rim_ow, rim_oh, rim_len, 1.5);
                translate([0,0,rim_front-EPS]) rbox(rim_iw, rim_ih, rim_len+1, 1.2);
            }
            for (sx=[-1,1])                                                    // keyhole pads
                translate([sx*keyhole_pitch/2, rim_ih/2-13, lid_iz-2.7]) rbox(17, 22, 2.7+EPS, 2);
            for (sx=[-1,1], sy=[-1,1])                                         // plate bosses
                translate([board_cx+sx*plate_hole_dx/2, board_cy+sy*plate_hole_dy/2, lid_iz-plate_boss_h])
                    cylinder(d=7.5, h=plate_boss_h+EPS);
            translate([0, chg_cy, 0]) charger_mount();   // mount is modeled around y=0
            // battery fence
            translate([cav_x0+6+(batt_l+2*batt_fence_t)/2, cav_y0+26+(batt_w+2*batt_fence_t)/2, lid_iz-batt_fence_h])
                difference() {
                    rbox(batt_l+2*batt_fence_t, batt_w+2*batt_fence_t, batt_fence_h+EPS, 2);
                    translate([0,0,-EPS]) rbox(batt_l, batt_w, batt_fence_h+1, 1.5);
                    for (s=[-1,1]) translate([-batt_l/2-batt_fence_t-EPS, s*10-2.5, batt_fence_h-3.0])
                        cube([batt_l+2*batt_fence_t+1, 5, 3.1]);   // strap slots
                }
            translate([cav_x0+34, cav_y0+78, 0]) ziptie_bridge();              // spare module
            translate([cav_x0+34, cav_y0+94, 0]) ziptie_bridge();              // tie-downs
        }
        // corner notches so the rim clears the pillars (floor stays intact)
        for (sx=[-1,1], sy=[-1,1])
            translate([sx*px, sy*py, rim_front-1]) rbox(pillar+1.6, pillar+1.6, rim_len-0+1, 1.5);
        // lid screws into pillars: through-hole + head counterbore (from back)
        for (sx=[-1,1], sy=[-1,1]) translate([sx*px, sy*py, 0]) {
            translate([0,0,lid_iz-1]) cylinder(d=3.5, h=lid_floor+2);
            translate([0,0,depth-1.3]) cylinder(d=6.6, h=1.4);
        }
        // wall keyholes (slot must run toward the TOP edge)
        for (sx=[-1,1]) translate([sx*keyhole_pitch/2, rim_ih/2-17.6, depth])
            rotate([180,0,0]) rotate([0,0,180]) keyhole_neg();
        // FPC relief in the rim's pressing edge (bottom center)
        translate([-fpc_w/2, -rim_oh/2-EPS, rim_front-EPS]) cube([fpc_w, rim_t+2.5, 3]);
        // port slots through the rim right wall (bigger than the body slots
        // so the plug overmold passes straight through to the receptacle)
        rim_slot(board_cy, lid_iz - usb_h, usb_slot_w+2, usb_slot_h+1.5);
        rim_slot(chg_cy, lid_iz - chg_usb_ctr, chg_usb_w+2, chg_usb_h+1);
        // pilot holes for the plate screws (blind: 0.6mm skin stays on the back)
        for (sx=[-1,1], sy=[-1,1])
            translate([board_cx+sx*plate_hole_dx/2, board_cy+sy*plate_hole_dy/2, lid_iz-plate_boss_h-EPS])
                cylinder(d=2.5, h=plate_boss_h + lid_floor - 0.6);
        // charge LED peek slot
        if (led_slot)
            translate([rim_iw/2-chg_l+3, chg_cy, lid_iz-EPS]) rbox(7, 12, lid_floor+1, 1.5);
    }
}

// =====================================================================
// BOARD PLATE  (local: z=0 bottom, standoffs +z)
// =====================================================================
module plate() {
    difference() {
        union() {
            // asymmetric outline: 4mm margin left, 0.5mm on the USB side so the
            // plate clears the lid rim while the board edge sits near the wall
            translate([-1.75, 0, 0]) rbox(board_l+4.5, board_w+4, plate_t, 3);
            for (sx=[-1,1], sy=[-1,1])
                translate([sx*board_hole_dx/2, sy*board_hole_dy/2, plate_t-EPS])
                    cylinder(d1=7, d2=6, h=board_soff_h+EPS);
        }
        for (sx=[-1,1], sy=[-1,1]) translate([sx*board_hole_dx/2, sy*board_hole_dy/2, -EPS])
            cylinder(d=board_pilot, h=plate_t+board_soff_h+1);
        for (sx=[-1,1], sy=[-1,1]) translate([sx*plate_hole_dx/2, sy*plate_hole_dy/2, -EPS]) {
            cylinder(d=3.5, h=plate_t+1);
            cylinder(d=6.6, h=1.2);   // head sits below board (use pan-head M3)
        }
    }
}

// =====================================================================
// DESK STAND  (front of case faces +y; case leans toward -y)
// =====================================================================
module stand() {
    ch = depth + 0.6;
    L  = stand_len;
    difference() {
        union() {
            translate([0, -3, 0]) rbox(L, 54, 5, 6);                       // base
            rotate([stand_tilt, 0, 0]) {
                translate([-L/2, -ch/2-3.5, 0]) cube([L, 3.5, 30]);        // rear wall
                translate([-L/2,  ch/2, 0])     cube([L, 3.5, 10]);        // front lip
                translate([-L/2, -ch/2-3.5, 0]) cube([L, ch+7, 3.5]);      // channel floor
            }
        }
        translate([0, 0, -25]) cube([L+20, 200, 50], center=true);         // trim below bed
        for (i=[-1,0,1]) rotate([stand_tilt,0,0])                          // arches
            translate([i*L/3.15, -ch/2+2, 19]) rotate([90,0,0]) cylinder(d=26, h=12);
        rotate([stand_tilt,0,0]) translate([0, 0, 3.5])                    // drop the case in
            translate([-L/2-1, -ch/2, 0]) cube([L+2, ch, 60]);
    }
}

// =====================================================================
// OUTPUT
// =====================================================================
if (part == "section") {   // vertical slice through the DevKitC USB axis
    intersection() {
        union() {
            body();
            lid_asm();
            translate([board_cx, board_cy, lid_iz-plate_boss_h]) rotate([180,0,0]) plate();
            translate([0,0,bezel_t+0.01]) rbox(panel_w, panel_h, panel_t, 0.6);
        }
        translate([0, board_cy, 0]) cube([500, 1.6, 200], center=true);
    }
}
if (part == "section2") {  // vertical slice through the charger USB axis
    intersection() {
        union() { body(); lid_asm(); }
        translate([0, chg_cy, 0]) cube([500, 1.6, 200], center=true);
    }
}
if (part == "body")  body();
if (part == "lid")   rotate([0,180,0]) translate([0,0,-depth]) lid_asm();
if (part == "plate") plate();
if (part == "stand") stand();
if (part == "exploded") {   // front (body) to back (lid), pulled apart along +z
    color("DimGray")    body();
    color("WhiteSmoke") translate([0,0,bezel_t+30]) rbox(panel_w, panel_h, panel_t, 0.6);
    color("Green")      translate([board_cx, board_cy, 12.2+42]) rbox(board_l, board_w, 1.6, 1);
    color("Orange")     translate([board_cx, board_cy, lid_iz-plate_boss_h+31]) rotate([180,0,0]) plate();
    color("SteelBlue")  translate([0,0,72]) lid_asm();
}
if (part == "assembly") {
    color("DimGray")    body();
    color("WhiteSmoke") translate([0,0,bezel_t+0.01]) rbox(panel_w, panel_h, panel_t, 0.6);
    color("SteelBlue")  lid_asm();
    color("Orange")     translate([board_cx, board_cy, lid_iz-plate_boss_h]) rotate([180,0,0]) plate();
    color("Green")      translate([board_cx, board_cy, lid_iz-plate_boss_h-plate_t-board_soff_h-1.6]) rbox(board_l, board_w, 1.6, 1); // board mock
}
