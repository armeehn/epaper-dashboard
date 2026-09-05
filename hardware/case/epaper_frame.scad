// =====================================================================
//  7.5" Tri-color e-Paper (Waveshare raw panel, ASIN B09JSFTGV6)
//  Desk + wall case for a custom ESP32 carrier board, TP4056-style
//  USB-C charger module, and LiPo pouch cell.
//  Units: mm  |  OpenSCAD 2021.01
// ---------------------------------------------------------------------
//  THE PANEL IS REAR-LOADED AND THE POCKET IS THE ONLY THING IN ITS WAY.
//  Nothing may be added inside the pocket footprint: the glass drops in
//  along -z from the open back and has to reach its seat unobstructed.
//  `part="obstruction"` renders whatever violates that; it must be empty,
//  and fit_check.sh fails the build if it is not.
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
// clr is a GLASS clearance, not a plastic one. A 170 mm pocket printed in
// PETG lands within about +/-0.4 mm of nominal, so anything under ~0.5 can
// come out of the printer as an interference fit on a part that cannot be
// filed, tapped or bent. 0.6 per side costs nothing: the panel is hidden
// behind an 11.5 mm border and clamped by foam, so it cannot rattle.
clr       = 0.6;    // panel-to-pocket clearance per side
wall      = 2.6;    // outer wall thickness
bezel_t   = 2.0;    // front face thickness in front of the glass
reveal    = 1.6;    // window opening beyond active area, per side
win_ch    = 2.0;    // 45-deg outward chamfer around the window
foam_fr   = 1.0;    // foam tape on the front ledge, under the glass
foam_t    = 1.0;    // foam tape on panel rear border, compressed by lid rim
rim_len   = 17.0;   // interior depth for electronics
rim_t     = 2.0;    // lid rim wall thickness
lid_floor = 2.4;    // lid back plate thickness
corner_r  = 5.0;    // outer corner radius
pocket_r  = 1.2;    // pocket corner radius (small - panel corners are square)
relief_r  = 3.0;    // corner relief radius (square glass corners need room)
relief_ov = 0.8;    // how far the relief bulges past the pocket wall
lead_in   = 1.0;    // 45-deg flare at the rear mouth, guides panel and rim
scallop_w = 18;     // finger scallop in the pocket side walls (panel lift-out)
scallop_d = 3;      // ...and how deep it cuts into the wall
lid_clr   = 0.4;    // lid-to-body sliding clearance per side

// ------------------ CUSTOM ESP32 CARRIER BOARD -----------------------
// Most ESP32 dev boards, the 38-pin DevKitC included, ship with NO mounting
// holes. So the carrier holds the board by its edges and needs none:
//   "rails"  slide the USB end under the lips, press the trailing end down
//            past the snap hook. Nothing is drilled, nothing is glued.
//   "screws" the old standoff-and-M2.5 pattern, for boards that do have holes.
board_mount    = "rails"; // ["rails","screws"]
// Waveshare e-Paper ESP32 Driver Board: "Outline dimension: 29.46mm x 48.25mm"
// (e-Paper_ESP32_Driver_Board_user_manual_en.pdf, Specifications). The long
// axis runs toward the right wall, so the USB edge is the 29.46 mm one.
board_l        = 48.25; // size along X (USB edge faces the right wall)
board_w        = 29.46; // size along Y
board_pcb_t    = 1.6;   // PCB thickness                                  ** MEASURE **
board_hole_dx  = 53;    // "screws" only: mounting-hole spacing X  ** MEASURE YOUR BOARD **
board_hole_dy  = 33;    // "screws" only: mounting-hole spacing Y  ** MEASURE YOUR BOARD **
board_pilot    = 2.05;  // standoff pilot: 2.05 for M2.5 self-tap, 2.5 for M3
board_soff_h   = 2.5;   // board underside above the plate (room for solder tails)
board_gap_right= 2.0;   // board USB edge -> lid rim inner face (the stop lives here)

// "rails" carrier. Everything here is printed against the board's OUTLINE,
// so board_l / board_w / board_pcb_t are the only numbers you have to get
// right; there is no hole pattern to match.
board_rail_t   = 1.8;   // rail wall thickness
board_rail_clr = 0.3;   // per-side slide clearance on the board width
board_shelf    = 1.5;   // how far the rail shelf reaches under the board
board_slot_z   = 0.25;  // vertical slack between board and lip
board_lip      = 1.2;   // how far the lips reach over the board
board_lip_t    = 1.2;   // lip thickness
board_lip_back = 8;     // trailing length left un-lipped, for the clamp screw
board_stop_t   = 1.0;   // USB-end stop thickness (sits inside board_gap_right)
board_stop_w   = 6;     // ...and how far each stop reaches inward
// The board goes in flat, so nothing may stand behind its trailing edge until
// it is home. One screw, driven last, does that job: its shank passes beside
// the edge and its head laps over the top. No flexures to tune, no extra part.
board_clamp_x  = 1.5;   // clamp screw axis, past the board's trailing edge
board_clamp_d  = 6;     // clamp boss diameter
board_clamp_pl = 2.05;  // clamp pilot: M2.5 self-tap
board_clamp_hd = 4.7;   // M2.5 pan head diameter
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
boss_edge     = 5.0;    // screw boss center -> nearest outer face
boss_gap      = 4.4;    // screw boss center -> pocket wall (must clear it)
lid_pilot     = 2.5;    // M3 self-tap pilot in the side walls
lid_tap_deep  = 10.5;   // pilot depth (M3 x 12 minus the lid floor)

// =====================================================================
// DERIVED
// =====================================================================
pocket_w  = panel_w + 2*clr;                  // 171.4
pocket_h  = panel_h + 2*clr;                  // 112.4
panel_seat= bezel_t + foam_fr;                // glass front face
panel_back= panel_seat + panel_t;
rim_front = panel_back + foam_t;              // lid rim presses foam here
depth     = rim_front + rim_len + lid_floor;  // total case depth
lid_iz    = depth - lid_floor;                // z of lid inner floor
body_d    = lid_iz;                           // the lid caps the body flush

aa_bottom = panel_h - aa_h - aa_top;          // FPC-side border (~9.9)
win_w     = aa_w + 2*reveal;
win_h     = aa_h + 2*reveal;
win_dy    = (aa_bottom - aa_top)/2;           // window sits this far above pocket center

// uniform picture-frame border all around the window.
// minimum is set by the wide FPC-side border of the glass.
band      = aa_bottom - reveal + clr + wall;  // ~11.5
outer_w   = max(win_w + 2*band, pocket_w + 2*wall);
outer_h   = win_h + 2*band;                   // shell is centered on the WINDOW
shell_dy  = win_dy;                           // shell center offset (pocket stays at y=0)

relief_c  = relief_r - relief_ov;             // relief center inset from the pocket corner

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

// board carrier, in plate-local coordinates (origin = board centre, z=0 = plate bottom)
board_rail_h = board_soff_h + board_pcb_t + board_slot_z + board_lip_t;
board_zr     = board_soff_h + board_pcb_t + board_slot_z;   // underside of lips and hook
plate_x1 = board_l/2 + board_stop_t;                        // USB end
board_lip_x0   = -board_l/2 + board_lip_back;
board_clamp_cx = -(board_l/2 + board_clamp_x);
plate_x0 = board_clamp_cx - board_clamp_d/2 - 1.0;
plate_y  = board_w/2 + board_rail_clr + board_rail_t;

// Lid screws live in the LEFT and RIGHT walls, never at the pocket corners:
// the side walls are ~9 mm of solid plastic, the pocket stays empty.
boss_x = pocket_w/2 + boss_gap;
boss_yt = shell_dy + outer_h/2 - boss_edge;
boss_yb = shell_dy - outer_h/2 + boss_edge;

stack = plate_boss_h + plate_t + board_soff_h + board_pcb_t + 2 + 1.6 + 3.2;

assert(aa_bottom > 7, "aa_top looks wrong: bottom (FPC) border should be ~9.9");
assert(stack < rim_len - 0.5, str("component stack ", stack, " too tall for rim_len ", rim_len));
assert(usb_h + usb_slot_h/2 < rim_len + 2.6, "usb slot pokes past cavity depth");
assert(board_cx - board_l/2 > cav_x0 + 2, "board hits the left rim");
assert(board_cy - board_w/2 > cav_y0 + 1, "board hangs below cavity");
assert(board_cx + plate_x1 < rim_iw/2 - 0.4, "plate hits the lid rim");
// --- board retention invariants (your board has no mounting holes) ---
assert(board_gap_right >= board_stop_t + 0.6,
       "the USB-end stop does not fit between the board and the lid rim");
assert(board_clamp_hd/2 - board_clamp_x > 0.5,
       "the clamp screw head does not reach over the board's trailing edge");
assert(board_clamp_x - board_clamp_pl/2 > 0.2,
       "the clamp screw shank fouls the board's trailing edge");
assert(board_slot_z >= 0.15, "no vertical slack under the lips: the board will bind");
assert(board_lip + board_rail_clr < board_w/4, "the lips reach too far over the board");
// the plate-to-lid screws are on a fixed pattern; a small board shrinks the
// plate around them, so check they still land on plate and not on the rails
assert(plate_hole_dy/2 + 1.75 < board_w/2 + board_rail_clr,
       "the plate's lid screws break into the rail walls");
assert(plate_hole_dy/2 + 3.3 < plate_y,
       "the plate is too narrow for its own lid-screw counterbores");
assert(plate_hole_dx/2 + 3.3 < plate_x1 && -plate_hole_dx/2 - 3.3 > plate_x0,
       "the plate is too short for its own lid-screw counterbores");
// --- panel insertion invariants (this is what cracked the first build) ---
assert(boss_x - lid_pilot/2 - 1.6 >= pocket_w/2,
       "lid screw boss eats into the panel pocket: the glass cannot be fitted");
assert(boss_x + lid_pilot/2 + 1.6 <= outer_w/2,
       "lid screw boss breaks out through the outer wall");
assert(norm([pocket_w/2 - relief_c, pocket_h/2 - relief_c] - [panel_w/2, panel_h/2])
       < relief_r - 0.5,
       "corner relief too small: the square glass corners will bind on the fillets");
assert(clr >= 0.5, "panel clearance below print tolerance on a 170 mm span");
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
// the four lid screw positions, in body coordinates
module at_bosses() {
    for (sx=[-1,1], y=[boss_yb, boss_yt]) translate([sx*boss_x, y, 0]) children();
}

// =====================================================================
// PANEL POCKET  (a single negative: everything the glass needs, and
// nothing may be unioned back into it afterwards)
// =====================================================================
module panel_pocket() {
    translate([0,0,bezel_t]) rbox(pocket_w, pocket_h, body_d, pocket_r);
    // square glass corners cannot seat in a filleted corner: relieve all four
    for (sx=[-1,1], sy=[-1,1])
        translate([sx*(pocket_w/2 - relief_c), sy*(pocket_h/2 - relief_c), bezel_t])
            cylinder(r=relief_r, h=body_d);
    // 45-deg flare at the open back so the glass and the lid rim self-centre
    translate([0,0,body_d-lead_in]) hull() {
        rbox(pocket_w, pocket_h, EPS, pocket_r);
        translate([0,0,lead_in])
            rbox(pocket_w+2*lead_in, pocket_h+2*lead_in, EPS, pocket_r+lead_in);
    }
    // finger scallops: reach the glass edge to break the foam bond and lift
    // the panel straight out. Stops at rim_front so the lid rim still seals.
    for (sx=[-1,1]) translate([sx*pocket_w/2, 0, bezel_t]) hull()
        for (sy=[-1,1]) translate([0, sy*(scallop_w/2 - scallop_d), 0])
            cylinder(r=scallop_d, h=rim_front-bezel_t);
}

// the volume the rear-loaded glass sweeps on its way to the seat
module panel_sweep() {
    translate([0,0,panel_seat]) linear_extrude(depth+40)
        square([panel_w, panel_h], center=true);
}

// =====================================================================
// FRONT BODY  (z=0 front face, +z rearward; the lid caps it at body_d)
// =====================================================================
module body() {
    difference() {
        hull() {   // shell with a small front-edge chamfer, centered on the window
            translate([0,shell_dy,0.8]) rbox(outer_w, outer_h, body_d-0.8, corner_r);
            translate([0,shell_dy,0])   rbox(outer_w-1.6, outer_h-1.6, body_d, corner_r-0.8);
        }
        // window with 45-deg outward chamfer
        translate([0, win_dy, 0]) {
            hull() {
                translate([0,0,-EPS]) rbox(win_w+2*win_ch, win_h+2*win_ch, EPS, 2+win_ch);
                translate([0,0,bezel_t]) rbox(win_w, win_h, EPS, 2);
            }
            translate([0,0,bezel_t-EPS]) rbox(win_w, win_h, 1, 2);
        }
        panel_pocket();
        // port openings
        wall_slot(board_cy, lid_iz - usb_h, usb_slot_w, usb_slot_h);
        wall_slot(chg_cy, lid_iz - chg_usb_ctr, chg_usb_w, chg_usb_h, 1.2);
        // lid screws tap blind into the solid side walls
        at_bosses() translate([0,0,body_d-lid_tap_deep])
            cylinder(d=lid_pilot, h=lid_tap_deep+EPS);
    }
}

// =====================================================================
// LID  (modeled in assembly coords: back face at z=depth)
//  The lid is a full-footprint cap, not a plug: it lands on the body's
//  rear shoulder and reaches out to the side-wall screws.
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
            translate([0,shell_dy,0]) hull() {                                 // back plate
                translate([0,0,lid_iz])     rbox(outer_w, outer_h, lid_floor-0.8, corner_r);
                translate([0,0,lid_iz+0.8]) rbox(outer_w-1.6, outer_h-1.6, lid_floor-0.8, corner_r-0.8);
            }
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
        // lid screws into the body side walls: through-hole + head counterbore
        at_bosses() {
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
// BOARD PLATE  (local: z=0 bottom, board features +z)
//  The board has no mounting holes, so it is held by its outline. It slides
//  in FLAT from the trailing end, which is why nothing may stand in that
//  path until the board is home:
//
//   clamp screw, driven last          lips (hold the board down)      stop
//            v                              v            v             v
//           (O)  +----------------------------+----------+           +--+
//        #########|##############################|##########|###########|##|
//        #        |         PCB  ---- slides ---------->    |           |  |
//        #########|##############################|##########|###########|##|
//         ^                                                              ^
//        rail wall + shelf (seat and Y capture)                    lid rim beyond
// =====================================================================
module rail_one() {   // one long-edge rail; mirrored for the other side
    yi = board_w/2 + board_rail_clr;               // rail inner face
    translate([plate_x0, yi, plate_t-EPS])         // wall
        cube([plate_x1-plate_x0, board_rail_t, board_rail_h]);
    translate([plate_x0, yi-board_shelf, plate_t-EPS])   // shelf the board rests on
        cube([plate_x1-plate_x0, board_shelf, board_soff_h]);
    translate([board_lip_x0, yi-board_lip, plate_t+board_zr])   // lip
        cube([board_l/2-board_lip_x0, board_lip, board_lip_t]);
    translate([board_l/2, yi-board_stop_w, plate_t-EPS])        // USB-end stop
        cube([board_stop_t, board_stop_w, board_soff_h+board_pcb_t+0.8]);
}
module board_clamp() {   // boss stops level with the board, so it never blocks
    translate([board_clamp_cx, 0, plate_t-EPS])   // the slide-in; the screw does
        cylinder(d=board_clamp_d, h=board_soff_h+EPS);
}
module board_solid() {   // the PCB itself, seated. Nothing may share this space.
    translate([0,0,plate_t+board_soff_h])
        linear_extrude(board_pcb_t) square([board_l, board_w], center=true);
}
// the volume the board sweeps sliding in flat from the trailing end. Same
// lesson as the panel: the path matters as much as the final position.
module board_sweep() {
    len = board_l + (board_l/2 + abs(plate_x0));
    translate([board_l/2 - len, -board_w/2, plate_t+board_soff_h])
        cube([len, board_w, board_pcb_t]);
}
module plate() {
    difference() {
        union() {
            translate([(plate_x0+plate_x1)/2, 0, 0])
                rbox(plate_x1-plate_x0, 2*plate_y, plate_t, 3);
            if (board_mount == "rails") {
                rail_one();
                mirror([0,1,0]) rail_one();
                board_clamp();
            } else {
                for (sx=[-1,1], sy=[-1,1])
                    translate([sx*board_hole_dx/2, sy*board_hole_dy/2, plate_t-EPS])
                        cylinder(d1=7, d2=6, h=board_soff_h+EPS);
            }
        }
        if (board_mount == "screws")
            for (sx=[-1,1], sy=[-1,1]) translate([sx*board_hole_dx/2, sy*board_hole_dy/2, -EPS])
                cylinder(d=board_pilot, h=plate_t+board_soff_h+1);
        if (board_mount == "rails")   // clamp screw pilot, right through
            translate([board_clamp_cx, 0, -EPS])
                cylinder(d=board_clamp_pl, h=plate_t+board_soff_h+1);
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
if (part == "obstruction") {   // MUST render empty - see fit_check.sh
    intersection() { body(); panel_sweep(); }
}
if (part == "board_clash") {   // MUST render empty: the carrier may touch the
    intersection() { plate(); board_solid(); }   // board, never occupy it
}
if (part == "board_sweep") {   // MUST render empty: the slide-in path is clear
    intersection() { plate(); board_sweep(); }
}
if (part == "section") {   // vertical slice through the DevKitC USB axis
    intersection() {
        union() {
            body();
            lid_asm();
            translate([board_cx, board_cy, lid_iz-plate_boss_h]) rotate([180,0,0]) plate();
            translate([0,0,panel_seat]) rbox(panel_w, panel_h, panel_t, 0.6);
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
    color("WhiteSmoke") translate([0,0,panel_seat+30]) rbox(panel_w, panel_h, panel_t, 0.6);
    color("Green")      translate([board_cx, board_cy, 12.2+42]) rbox(board_l, board_w, 1.6, 1);
    color("Orange")     translate([board_cx, board_cy, lid_iz-plate_boss_h+31]) rotate([180,0,0]) plate();
    color("SteelBlue")  translate([0,0,72]) lid_asm();
}
if (part == "assembly") {
    color("DimGray")    body();
    color("WhiteSmoke") translate([0,0,panel_seat]) rbox(panel_w, panel_h, panel_t, 0.6);
    color("SteelBlue")  lid_asm();
    color("Orange")     translate([board_cx, board_cy, lid_iz-plate_boss_h]) rotate([180,0,0]) plate();
    color("Green")      translate([board_cx, board_cy, lid_iz-plate_boss_h-plate_t-board_soff_h-board_pcb_t]) rbox(board_l, board_w, board_pcb_t, 1); // board mock
}
if (part == "clash") {   // body and lid must never want the same space
    intersection() { body(); lid_asm(); }
}
