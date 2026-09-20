// =====================================================================
//  RUTA series: cases for the 7.5" 800x480 e-paper panel on the XIAO
//  ESP32-C3 + Seeed ePaper Driver Board (docs/LOW_COST.md).
//  Units: mm  |  OpenSCAD 2021.01
// ---------------------------------------------------------------------
//  One front FRAME, three BACKS, one STAND. Every product in the series
//  is the frame plus one back, so a wall unit becomes a pegboard unit or
//  a fridge unit by swapping one printed part, and every part prints flat
//  with no supports and no fasteners.
//
//    RUTA  frame + back_wall    two keyholes, 80 mm apart
//    KROK  RUTA + 2 x hook      SKADIS pegboard hooks keyed into the back
//    FAST  frame + back_magnet  four 10x3 mm disc magnets, fridge/whiteboard
//    STOD  RUTA + stand         desk plinth, 12 deg lean, cable channel
//
//  THE PANEL IS REAR-LOADED AND THE POCKET IS THE ONLY THING IN ITS WAY.
//  The glass drops in along +z from the open back and reaches its seat
//  unobstructed; `part="obstruction"` renders whatever violates that and
//  fit_check.sh fails the build if it is not empty. The back snaps into
//  the pocket walls with four bumps; `part="clash"` proves it closes.
// =====================================================================
//  PARTS - set `part` (or CLI: -D 'part="frame"'):
//    "frame"        front frame              (print face down)
//    "back_wall"    RUTA back, keyholes + hook slots (print outer face down)
//    "hook"         KROK pegboard hook, x2   (prints on its side, flat)
//    "back_magnet"  FAST back, magnet pockets(print outer face down)
//    "stand"        STOD desk plinth         (print base down)
//    "ruta" "krok" "fast" "stod"  assembled previews
//    "exploded"     the series, pulled apart
//    "obstruction" "clash" "board_clash" "board_sweep"   fit checks
// =====================================================================

part = "ruta";

// The XIAO ships on the driver board's female headers. Soldered flat onto
// the board's castellated pads instead, the whole stack is 8 mm shorter
// and so is the case: 13 mm deep instead of 21.
xiao_headers = true;

$fn = 48;
EPS = 0.01;

// ------------------ PANEL (GDEY075T7 / GDEW075T7 datasheet) ---------
panel_w  = 170.2;   // glass outline
panel_h  = 111.2;
panel_t  = 1.3;     // glass incl. slack (nominal 1.18)
aa_w     = 163.2;   // active area
aa_h     = 97.92;
aa_top   = 3.4;     // glass edge -> active area at the top   ** MEASURE **
fpc_w    = 36;      // ribbon relief width in the back's rim (ribbon ~24)

// ------------------ FRAME -------------------------------------------
clr      = 0.6;     // glass-to-pocket clearance per side (170 mm PLA span)
wall     = 1.8;     // thinnest wall: below the glass, where the border is set
bezel_t  = 1.6;     // face in front of the glass
reveal   = 1.0;     // window past the active area, per side
win_ch   = 1.2;     // 45 deg outward chamfer round the window
foam_fr  = 0.8;     // foam tape under the glass front border
foam_t   = 0.8;     // foam tape behind the glass, pressed by the back's rim
corner_r = 4.0;     // outer corner radius
pocket_r = 1.2;     // pocket corner radius
relief_r = 3.0;     // corner relief for the square glass corners
relief_ov= 0.8;
lead_in  = 1.0;     // flare at the open back
scallop_w= 18;      // finger scallops in the side walls, to lift the glass out
scallop_d= 3;

// ------------------ BACK --------------------------------------------
back_t   = 1.8;     // plate thickness
rim_t    = 1.6;     // rim wall, runs inside the pocket
lid_clr  = 0.35;    // rim-to-pocket clearance per side
snap_h   = 0.7;     // snap bump height (and recess depth in the side walls)
snap_len = 12;      // bump length along the wall
snap_z   = 6;       // bump centre above the rim's front edge
snap_dx  = 38;      // bump centres from the middle, along the side walls

// ------------------ ELECTRONICS BAY ---------------------------------
// Seeed ePaper Driver Board, XIAO edge toward the top of the case, FPC
// connector toward the panel's bottom edge where the ribbon arrives.
drv_l    = 50;      // board along X                          ** MEASURE **
drv_w    = 30;      // board along Y                          ** MEASURE **
drv_t    = 1.6;
drv_parts= 3.5;     // tallest part on the board's top face, FPC latch etc.
xiao_stack = xiao_headers ? 8.5 + 1.6 + 3.3 : 1.6 + 3.3;  // header + XIAO + USB-C body
bay      = drv_t + max(drv_parts, xiao_stack) + 1.0;     // + 1 mm air
drv_from_bot = 4;   // board lower edge above the cavity's inner bottom
rail_t   = 1.6;     // side rails (along X) the board slides between
rail_clr = 0.3;
lip      = 1.0;     // lips over the board's long edges
lip_t    = 1.0;
slot_z   = 0.25;    // vertical slack under the lips
usb_w    = 10.0;    // USB-C slot in the bottom wall (XIAO receptacle 8.9 x 3.2)
usb_h    = 4.2;
usb_x    = 0;       // slot centre along X                     ** MEASURE **

// ------------------ WALL / PEGBOARD / MAGNETS / STAND ---------------
key_pitch = 80;     // keyhole centres
key_head  = 8.5;    // screw head clearance
key_shank = 4.2;
key_len   = 8;
key_pad_t = 1.6;    // extra plate thickness round each keyhole
skadis_pitch = 40;  // SKADIS slot pitch, horizontal
skadis_slot  = [4.6, 14.2];   // peg through the 5 x 15 slot
skadis_board = 5.1;           // board thickness
skadis_lip   = 4;             // hook drop behind the board
hook_slot    = [4.8, 14.4];   // through the back, the hook's peg keys in here
hook_flange  = [9, 19, 1.6];  // inside the case, what the load pulls against
hook_y       = 12;            // slot centres below the back's top edge
mag_d     = 10.3;   // 10 x 3 mm disc magnets, press fit
mag_t     = 3.2;
mag_inset = 14;     // magnet centres from the plate's edges
stand_d   = 42;     // plinth depth (front to back)
stand_h   = 18;     // plinth height
stand_tilt= 12;     // backward lean
stand_slot= 12;     // how deep the frame sits in the plinth
stand_clr = 0.5;

// ------------------ DERIVED -----------------------------------------
pocket_w  = panel_w + 2*clr;
pocket_h  = panel_h + 2*clr;
panel_seat= bezel_t + foam_fr;              // glass front face, from the frame face
panel_back= panel_seat + panel_t;
rim_front = panel_back + foam_t;            // the back's rim presses foam here
body_d    = rim_front + bay;                // frame depth, walls end here
depth     = body_d + back_t;                // whole case
rim_h     = body_d - rim_front;             // rim reaches the foam

aa_bottom = panel_h - aa_h - aa_top;        // ribbon-side border (~9.9)
win_w     = aa_w + 2*reveal;
win_h     = aa_h + 2*reveal;
win_dy    = (aa_bottom - aa_top)/2;         // window centre above pocket centre
border    = aa_bottom - reveal + clr + wall;// the thinnest border sets them all
outer_w   = win_w + 2*border;
outer_h   = win_h + 2*border;
shell_dy  = win_dy;                         // shell centred on the window; pocket at y=0
relief_c  = relief_r - relief_ov;

rim_ow = pocket_w - 2*lid_clr;
rim_oh = pocket_h - 2*lid_clr;
rim_iw = rim_ow - 2*rim_t;
rim_ih = rim_oh - 2*rim_t;
cav_y0 = -rim_ih/2;                         // cavity inner bottom (y)
drv_cy = cav_y0 + drv_from_bot + drv_w/2;
drv_z0 = depth - back_t;                    // back's inner face (z, frame coords)
rail_h = drv_t + slot_z + lip_t;
usb_zc = drv_t + xiao_stack - 3.3/2;        // USB-C centre above the board's underside

// --- invariants ---
assert(clr >= 0.5, "panel clearance below print tolerance on a 170 mm span");
assert(norm([pocket_w/2 - relief_c, pocket_h/2 - relief_c] - [panel_w/2, panel_h/2])
       < relief_r - 0.5, "corner relief too small: square glass corners bind");
assert(drv_l + 2*(rail_t + rail_clr) < rim_iw, "board rails do not fit inside the rim");
assert(drv_from_bot + drv_w + 1 < rim_ih, "board does not fit the cavity height");
assert(usb_zc + usb_h/2 < bay - 0.5, "USB-C slot breaks out of the bay");
assert(snap_dx + snap_len/2 < pocket_h/2 - 6, "snap bumps run past the side walls");
echo(str(">> RUTA outer ", outer_w, " x ", outer_h, " x ", depth, " mm; border ", border,
         "; bay ", bay, " (xiao_headers=", xiao_headers, ")"));

// =====================================================================
// HELPERS
// =====================================================================
module rrect(w, h, r) offset(r=r) square([w-2*r, h-2*r], center=true);
module rbox(w, h, d, r) linear_extrude(d) rrect(w, h, r);
module keyhole() {
    hull() { cylinder(d=key_shank, h=20, center=true);
             translate([0, key_len, 0]) cylinder(d=key_shank, h=20, center=true); }
    cylinder(d=key_head, h=20, center=true);
}

// =====================================================================
// PANEL POCKET (one negative, nothing is ever unioned back into it)
// =====================================================================
module panel_pocket() {
    translate([0,0,bezel_t]) rbox(pocket_w, pocket_h, body_d, pocket_r);
    for (sx=[-1,1], sy=[-1,1])
        translate([sx*(pocket_w/2 - relief_c), sy*(pocket_h/2 - relief_c), bezel_t])
            cylinder(r=relief_r, h=body_d);
    translate([0,0,body_d-lead_in]) hull() {
        rbox(pocket_w, pocket_h, EPS, pocket_r);
        translate([0,0,lead_in])
            rbox(pocket_w+2*lead_in, pocket_h+2*lead_in, EPS, pocket_r+lead_in);
    }
    // finger scallops in the side walls, front of the rim only
    for (sx=[-1,1]) translate([sx*pocket_w/2, 0, panel_seat])
        rotate([0,0,90]) linear_extrude(rim_front - panel_seat + EPS)
            rrect(scallop_w, 2*scallop_d, 1.4);
    // snap recesses in the side walls
    for (sx=[-1,1], sy=[-1,1])
        translate([sx*(pocket_w/2 + snap_h/2 - EPS), sy*snap_dx, rim_front + snap_z])
            cube([snap_h + 2*EPS, snap_len, 2.2], center=true);
}

// the volume the glass sweeps from the open back to its seat
module panel_sweep() {
    translate([0,0,panel_seat]) linear_extrude(body_d)
        square([panel_w, panel_h], center=true);
}

module window() {
    translate([0, win_dy, -EPS]) rbox(win_w, win_h, bezel_t + 2*EPS, 1.0);
    translate([0, win_dy, -EPS]) hull() {
        rbox(win_w + 2*win_ch, win_h + 2*win_ch, EPS, 1.0 + win_ch);
        translate([0,0,win_ch]) rbox(win_w, win_h, EPS, 1.0);
    }
}

// USB-C slot through the frame's bottom wall and the back's rim
module usb_cut() {
    translate([usb_x, -pocket_h/2 - wall/2, drv_z0 - usb_zc])
        cube([usb_w, wall + 2*rim_t + 4, usb_h], center=true);
}

// =====================================================================
// FRAME
// =====================================================================
module frame() {
    difference() {
        translate([0, shell_dy, 0]) rbox(outer_w, outer_h, body_d, corner_r);
        window();
        panel_pocket();
        usb_cut();
    }
}

// =====================================================================
// BACKS: plate + rim + board rails, then one of three outsides
// =====================================================================
module post_L(sx, sy) {
    post_h = drv_t + 3;
    x0 = sx*(drv_l/2 + rail_clr);
    y0 = drv_cy + sy*(drv_w/2 + rail_clr);
    // leg along Y (outside the board's short edge) and leg along X (outside the long edge)
    translate([sx > 0 ? x0 : x0 - rail_t, sy > 0 ? y0 - 6 : y0 - rail_t, 0]) cube([rail_t, 6 + rail_t, post_h]);
    translate([sx > 0 ? x0 - 6 : x0, sy > 0 ? y0 : y0 - rail_t, 0]) cube([6 + rail_t, rail_t, post_h]);
}
module rails() {   // kept as the name the fit checks use
    for (sx=[-1,1], sy=[-1,1]) post_L(sx, sy);
}
module back_core() {
    // in back-local coords: z=0 is the inner face, +z toward the frame
    union() {
        translate([0,0,-back_t]) rbox(rim_ow, rim_oh, back_t, pocket_r);
        difference() {
            rbox(rim_ow, rim_oh, rim_h, pocket_r);
            translate([0,0,-EPS]) rbox(rim_iw, rim_ih, rim_h + 2*EPS, pocket_r);
            // ribbon relief in the bottom rim
            translate([0, -rim_oh/2, rim_h - 4]) cube([fpc_w, 2*rim_t + 2, 8.1], center=true);
            // USB-C
            translate([usb_x, -rim_oh/2, usb_zc]) cube([usb_w, 2*rim_t + 2, usb_h], center=true);
        }
        // snap bumps on the rim's outer faces (side walls only, they are thick)
        for (sx=[-1,1], sy=[-1,1])
            translate([sx*(rim_ow/2 + snap_h/2 - EPS), sy*snap_dx, rim_h - snap_z])
                hull() {
                    cube([EPS, snap_len, 2], center=true);
                    translate([sx*snap_h/2, 0, 0]) cube([EPS, snap_len - 2*snap_h, 0.6], center=true);
                }
        rails();
    }
}
module back_wall() {
    difference() {
        union() {
            back_core();
            for (sx=[-1,1]) translate([sx*key_pitch/2, 10, 0])
                cylinder(d=key_head + 8, h=key_pad_t);
        }
        for (sx=[-1,1]) translate([sx*key_pitch/2, 10, 0]) keyhole();
        // KROK's hooks key into these; on a wall they are two small slots
        for (sx=[-1,1]) translate([sx*skadis_pitch/2, rim_oh/2 - hook_y, 0])
            cube([hook_slot[0], hook_slot[1], 3*back_t], center=true);
    }
}
// A pegboard hook: flange inside the case, peg through the back and the
// board, lip that drops behind the board. Flat on its side it is one 2D
// profile 4.6 mm thick, so it prints with nothing to support.
module hook() {   // back-local coords, at a slot centre; outside is -z
    reach = back_t + skadis_board + 1.2;
    translate([-hook_flange[0]/2, -hook_flange[1]/2, 0]) cube(hook_flange);
    translate([-skadis_slot[0]/2, -skadis_slot[1]/2, -reach]) cube([skadis_slot[0], skadis_slot[1], reach + EPS]);
    translate([-skadis_slot[0]/2, -skadis_slot[1]/2 - skadis_lip, -reach]) cube([skadis_slot[0], skadis_lip + EPS, 1.6]);
}
module hooks_placed() {
    for (sx=[-1,1]) translate([sx*skadis_pitch/2, rim_oh/2 - hook_y, 0]) hook();
}
module back_magnet() {
    difference() {
        union() {
            back_core();
            for (sx=[-1,1], sy=[-1,1])
                translate([sx*(rim_ow/2 - mag_inset), sy*(rim_oh/2 - mag_inset), 0])
                    cylinder(d=mag_d + 4, h=mag_t + 1.2 - back_t);
        }
        for (sx=[-1,1], sy=[-1,1])
            translate([sx*(rim_ow/2 - mag_inset), sy*(rim_oh/2 - mag_inset), -back_t - EPS])
                cylinder(d=mag_d, h=mag_t + EPS);
    }
}

// a back, placed in frame coordinates (inner face at z = drv_z0, rim toward -z)
module place_back() { translate([0,0,drv_z0]) mirror([0,0,1]) children(); }

// =====================================================================
// STAND: a plinth the frame's bottom edge sits in
// =====================================================================
module stand() {
    slot_w = depth + 2*stand_clr;
    difference() {
        hull() {
            translate([-outer_w/2, 0, 0]) cube([outer_w, stand_d, 1]);
            translate([-outer_w/2 + 6, 6, stand_h - 1]) cube([outer_w - 12, stand_d - 12, 1]);
        }
        // tilted slot
        // leans toward +y, the plinth's back
        translate([0, stand_d/2, stand_h - stand_slot]) rotate([-stand_tilt, 0, 0])
            translate([-outer_w/2 - 1, -slot_w/2, 0]) cube([outer_w + 2, slot_w, stand_slot + 20]);
        // cable channel from the slot out the back, under the USB slot
        translate([usb_x - 7, stand_d/2 - 3, stand_h - stand_slot - 8]) cube([14, stand_d, 8.5]);
    }
}

// =====================================================================
// THE BOARD and its sweep (fit checks)
// =====================================================================
module board(dz=0) {          // back-local coords, seated
    translate([-drv_l/2, drv_cy - drv_w/2, 0]) cube([drv_l, drv_w, drv_t]);
}
module board_sweep() {        // the board dropping in from the open side (+z)
    translate([-drv_l/2, drv_cy - drv_w/2, 0]) cube([drv_l, drv_w, bay]);
}

// =====================================================================
// SCENES
// =====================================================================
module glass() {
    color([0.12,0.12,0.12]) translate([0,0,panel_seat]) linear_extrude(panel_t)
        square([panel_w, panel_h], center=true);
}
// the two render surfaces: the glass, and the active area a hair in front
module glass_plate() { translate([0,0,panel_seat]) linear_extrude(panel_t) square([panel_w, panel_h], center=true); }
module aa_plate()    { translate([0, win_dy, panel_seat - 0.05]) linear_extrude(0.05) square([aa_w, aa_h], center=true); }
// the STOD transform, shared by the preview and the catalogue renders
module in_stand() {
    translate([0, stand_d/2, stand_h - stand_slot]) rotate([-stand_tilt, 0, 0])
        translate([0, -depth/2, -(shell_dy - outer_h/2) + 1]) rotate([90, 0, 180]) children();
}
module assembled(kind, with_glass=true) {
    frame();
    if (with_glass) glass();
    place_back() {
        if (kind == "magnet") back_magnet();
        else back_wall();
        if (kind == "hook") hooks_placed();
    }
}

if (part == "frame" || part == "body") frame();   // "body": what fit_check.sh renders last, for the asserts
else if (part == "back_wall")   mirror([0,0,1]) back_wall();
else if (part == "hook")        translate([0, 0, skadis_slot[0]/2]) rotate([0, 90, 0]) hook();
else if (part == "back_magnet") mirror([0,0,1]) back_magnet();
else if (part == "stand") stand();
else if (part == "ruta") assembled("wall");
else if (part == "krok") assembled("hook");
else if (part == "fast") assembled("magnet");
else if (part == "stod") {
    // the frame stands on its bottom edge in the plinth's slot, leaning back
    stand();
    in_stand() assembled("wall");
}
// ---- render exports: bodies without glass, and the two screen surfaces ----
else if (part == "r_ruta") assembled("wall", false);
else if (part == "r_krok") assembled("hook", false);
else if (part == "r_fast") assembled("magnet", false);
else if (part == "r_glass") glass_plate();
else if (part == "r_aa") aa_plate();
else if (part == "exploded") {
    frame();
    translate([0,0,30]) glass();
    translate([0,0,70]) place_back() back_wall();
    translate([0,0,120]) place_back() hooks_placed();
    translate([0,0,170]) place_back() back_magnet();
    translate([0, -outer_h/2 - 60, 0]) stand();
}
// ---- fit checks: each must render EMPTY ----
else if (part == "obstruction") intersection() { frame(); panel_sweep(); }
else if (part == "clash")       intersection() { frame(); place_back() back_wall(); }
else if (part == "board_clash") intersection() { back_wall(); board(); }
else if (part == "board_sweep") intersection() { rails(); board_sweep(); }
