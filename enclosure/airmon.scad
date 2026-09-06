/* AirMon parametric prototype. Units: mm.
   Mechanical source: LCDWIKI E32R28T drawing 2024-08-31.
   Module representations are clearance envelopes, not vendor detailed models.
   part: assembly, exploded, bezel, rear_full, rear_shallow, stand, fit_test, pcb
*/
part = "assembly";
full = true;
$fn = 48;
case_w=100; case_h=82;
wall=2; clearance=.3;
rear_depth=full ? 44 : 30;
pcb_z=6.6; sensor_z=22;
board_offset=[7,10];
carrier_offset=[15,15];
case_screws=[[4,4],[96,4],[4,78],[96,78]];
mount_screws=[[14,6],[86,6],[14,75],[86,75]];
board_holes=[[11,14],[89,14],[11,56],[89,56]];
carrier_holes=[[32,60],[54,61]];
sgp_origin=[60,3]; scd_origin=[60,54];

module rounded_plate(w,h,z,r=4) {
 linear_extrude(z) hull() for(x=[r,w-r],y=[r,h-r]) translate([x,y]) circle(r=r);
}
module ring(w,h,z,r=4,t=2) {
 difference(){rounded_plate(w,h,z,r);translate([t,t,-.01])rounded_plate(w-2*t,h-2*t,z+.02,max(.1,r-t));}
}
module bore(d,h) {cylinder(d=d,h=h);}
module hex_nut(flat,h) {cylinder(d=flat/cos(30),h=h,$fn=6);}
module bezel() {
 difference() {
  union() {
   difference(){rounded_plate(case_w,case_h,2);translate([22.3,11.8,-.1])rounded_plate(61,46.4,2.2,1);}
   ring(case_w,case_h,8);
   translate([2.3,2.3,8])ring(95.4,77.4,2,2,1.2);
   for(p=board_holes)translate([p[0],p[1],2])cylinder(d=8,h=pcb_z-2);
   for(p=case_screws)translate([p[0],p[1],0])cylinder(d=7.6,h=8);
  }
  for(p=board_holes) {
   translate([p[0],p[1],2.15])hex_nut(5.8,2.7);
   translate([p[0],p[1],2])bore(3.3,6);
  }
  for(p=case_screws) {
   translate([p[0],p[1],2.6])hex_nut(5.8,2.7);
   translate([p[0],p[1],2.6])bore(3.3,6);
  }
  // USB plug access continues through the shell seam.
  translate([-.1,30,6.5])cube([8,10,8]);
  // Front room-air vents below the display, separated from its heat sources.
  for(x=[16:4:36])translate([x,68,-.1])rounded_plate(1.8,8,2.2,.8);
  translate([64,75,.49])rotate([180,0,0])linear_extrude(.5)text("AirMon",size=5,halign="center",font="Liberation Sans:style=Bold");
 }
}
module rear_post(p,d=6,flat=4.3,hole=2.3,depth=rear_depth,nut_h=1.9) {
 difference() {
  translate([p[0],p[1],sensor_z+1.6])cylinder(d=d,h=depth-sensor_z-1.6);
  translate([p[0],p[1],sensor_z+1.58])hex_nut(flat,nut_h);
  translate([p[0],p[1],sensor_z+1.5])bore(hole,7);
 }
}
module rear(is_full=full) {
 rear_depth=is_full?44:30;
 difference() {
  union() {
   translate([0,0,8])ring(case_w,case_h,rear_depth-8);
   translate([0,0,rear_depth-wall])rounded_plate(case_w,case_h,wall);
   for(p=case_screws)translate([p[0],p[1],8])cylinder(d=7.6,h=rear_depth-8);
   for(p=mount_screws)difference() {
    translate([p[0],p[1],rear_depth-4.2])cylinder(d=6,h=4.2);
    translate([p[0],p[1],rear_depth-4.21])hex_nut(4.3,1.9);
    translate([p[0],p[1],rear_depth-4.21])bore(2.3,4.3);
   }
   if(is_full) {
    // Independent inlet/outlet ducts, sealed to the module face with thin foam.
    difference() {
     translate([34.5,9,25.6])cube([16,5.7,18.4]);
     translate([36,10.5,25.5])cube([13,2.7,17]);
     translate([35.5,12.9,26.6])cube([13,2,6]);
    }
    difference() {
     translate([27,50.3,25.6])cube([16,5.7,18.4]);
     translate([28.5,51.8,25.5])cube([13,2.7,17]);
     translate([28.5,50.2,26.6])cube([13,2,5]);
    }
   }
  }
  for(p=case_screws) {
   translate([p[0],p[1],7.9])bore(3.4,rear_depth);
   translate([p[0],p[1],rear_depth-2.2])bore(6.2,2.3);
  }
  // Relief around the alignment tongue, including the corner screw bosses.
  translate([2,2,7.9])ring(96,78,2.4,2.3,1.8);
  // Physical BOOT/RESET access from the back; use a nonconductive probe.
  for(y=[23.43,46.57])translate([10.26,y,rear_depth-3])bore(3,4);
  translate([-.1,30,6.5])cube([8,10,8]);
  // SHT40 vents in the lower wall and rear; avoid a copper/metal heat path.
  for(x=[17:4:37])translate([x,66,rear_depth-2.1])rounded_plate(1.8,8,2.2,.8);
  for(x=[17:4:37])translate([x,case_h-2.1,20])cube([1.8,2.2,7]);
  if(is_full) {
   // Opposite PM edges: inlet near y=15, outlet near y=50.
   translate([36.3,10.8,rear_depth-2.1])rounded_plate(12.4,2.1,2.2,.4);
   translate([28.8,52.1,rear_depth-2.1])rounded_plate(12.4,2.1,2.2,.4);
   for(origin=[sgp_origin,scd_origin])for(x=[8:4:16])translate([origin[0]+x,origin[1]+4,rear_depth-2.1])rounded_plate(1.8,12,2.2,.8);
  }
 }
}
module sensor_mount(is_full=full) {
 top=is_full?39.8:25.8;
 difference() {
  union() {
   translate([11,2.4,top-2])ring(78,77,2,3,2.5);
   translate([56,2.4,top-2])cube([4,77,2]);
   translate([11,58,top-2])cube([49,4,2]);
   for(p=mount_screws)translate([p[0],p[1],top-2])cylinder(d=7,h=2);
   for(p=carrier_holes)rear_post(p,depth=top);
   for(x=[13,57])translate([x,15,21.7])cube([2,13,top-21.7]);
   if(is_full) {
    for(y=[16,54])translate([58,y,top-2])cube([31,4,2]);
    for(x=[2.54,22.86],y=[2.54,15.24])rear_post([sgp_origin[0]+x,sgp_origin[1]+y],6.5,5.3,2.8,top,2.3);
    for(x=[2.54,22.86],y=[2.54,20.32])rear_post([scd_origin[0]+x,scd_origin[1]+y],6.5,5.3,2.8,top,2.3);
    // Retaining lips touch thin foam over the PM body, clear of both air ports.
    for(x=[13,54])translate([x,20,37.8])cube([5,4,2]);
   }
  }
  for(p=mount_screws)translate([p[0],p[1],top-2.1])bore(2.3,2.2);
 }
}
module carrier() {
 color("#24795c")translate([carrier_offset[0],carrier_offset[1],sensor_z])linear_extrude(1.6)
 difference() {
  polygon([[0,0],[42,0],[42,48],[10,48],[10,55],[0,55],[0,48],[3,48],[3,43],[0,43]]);
  for(p=[[17,45],[39,46]])translate(p)circle(d=2.2);
 }
 color("#22282c")translate([carrier_offset[0]+4.23,carrier_offset[1]+2,sensor_z+1.6])cube([10.54,7,2]);
 color("#ddd9ca")translate([carrier_offset[0]+4.25,carrier_offset[1]+51.25,sensor_z+1.6])cube([1.5,1.5,.55]);
 // Connector and regulator clearance envelopes on the underside.
 color("#ede8db")for(p=[[26,42],[35,42]])translate([carrier_offset[0]+p[0]-5,carrier_offset[1]+p[1]-3,sensor_z-6])cube([8,6,6]);
 color("#ede8db")for(x=[29,38])translate([carrier_offset[0]+x-3,carrier_offset[1]+32,sensor_z-3])cube([6,4,3]);
 color("#262b32")translate([carrier_offset[0]+15.5,carrier_offset[1]+35.5,sensor_z-1.2])cube([3,3,1.2]);
}
module pm_module() {
 color("#bbc1c7")difference() {
  translate([17,15,25.6])cube([38,35,12]);
  translate([36.5,14.9,27.6])cube([10,2,4]);
  translate([29,48.5,27.6])cube([10.5,2,3]);
 }
 color("#313c44")translate([36.5,15,27.6])cube([10,.2,4]);
 color("#596873")translate([36,35,37.61])linear_extrude(.05)text("PMSA003I",size=3,halign="center");
}
module gas_module(origin,kind="scd") {
 h=kind=="scd"?22.86:17.78;
 color("#27374b")translate([origin[0],origin[1],sensor_z])linear_extrude(1.6)
 difference() {square([25.4,h]);for(x=[2.54,22.86],y=[2.54,h-2.54])translate([x,y])circle(d=2.8);}
 if(kind=="scd")color("#d3c9a7")translate([origin[0]+6,origin[1]+4,sensor_z+1.6])cube([11,11,7]);
 else color("#b6a998")translate([origin[0]+10,origin[1]+6,sensor_z+1.6])cube([3,3,1]);
 color("#ede8db")for(x=[0,21])translate([origin[0]+x,origin[1]+h/2-2,sensor_z+1.6])cube([4,4,3]);
}
module display_board() {
 color("#a8a534")translate([7,10,pcb_z])linear_extrude(1.6)difference(){square([86,50]);for(x=[4,82],y=[4,46])translate([x,y])circle(d=3.2);}
 color("#252c31")translate([15.4,10,2.6])cube([69.2,50,4]);
 color("#0e202c")translate([24.1,13.4,2.54])cube([57.6,43.2,.06]);
 color("#bcc5c8")translate([74,25,pcb_z+1.6])cube([13,18,3]);
 color("#2c3336")translate([87,25,pcb_z+1.6])cube([6,18,1]);
 color("#b7bec6")translate([6.5,31,pcb_z+1.6])cube([7,8,3]);
 // Illustrative LCD values; not a screenshot of the physical firmware.
 for(i=[0:3]) {
  color("#97b7c4")translate([28,20+i*8,2.50])rotate([180,0,0])linear_extrude(.04)text(["AIRMON","22.4 C   45 %","PM2.5   8","CO2   620"][i],size=i==0?3:2.6,font="Liberation Sans");
 }
}
module pose() {translate([4,8,14])rotate([-102,0,0])translate([0,-case_h,0])children();}
module stand() {
 difference() {
  union() {
   rounded_plate(108,65,4,5);
   for(x=[0,101])translate([x,5,0])cube([7,54,20]);
   translate([3,5,0])cube([102,5,16]);
  }
  pose()translate([-.35,-.35,-.35])cube([case_w+.7,case_h+.7,44.7]);
 }
}
module fit_test() {
 difference(){rounded_plate(55,24,5,2);
  for(i=[0:2])translate([9+i*13,8,1.9])hex_nut(5.6+.2*i,3.2);
  for(i=[0:2])translate([9+i*13,8,-.1])bore(3.1+.2*i,5.2);
  translate([7,16,-.1])cube([36,2.4,5.2]);
  translate([48,7,3.1])hex_nut(4.3,2);
  translate([48,7,-.1])bore(2.3,5.2);
  translate([48,18,2.7])hex_nut(5.3,2.4);
  translate([48,18,-.1])bore(2.8,5.2);
 }
 translate([7,28,0])cube([36,1.8,3]);
}
module exploded() {
 color("#344e58")translate([-4,8.5,-50])stand();
 color("#e4e0d5")translate([0,0,-20])bezel();
 display_board();
 translate([0,0,20])carrier();
 if(full){translate([0,0,36])pm_module();translate([0,0,20])gas_module(scd_origin);translate([0,0,20])gas_module(sgp_origin,"sgp");}
 color("#d3ad73")translate([0,0,55])sensor_mount();
 color("#3e5962")translate([20,10,120])rear();
}
if(part=="bezel")bezel();
else if(part=="rear_full")translate([0,case_h,44])rotate([180,0,0])rear(true);
else if(part=="rear_shallow")translate([0,case_h,30])rotate([180,0,0])rear(false);
else if(part=="sensor_mount_full")translate([-10,80,39.8])rotate([180,0,0])sensor_mount(true);
else if(part=="sensor_mount_shallow")translate([-10,80,25.8])rotate([180,0,0])sensor_mount(false);
else if(part=="stand")stand();
else if(part=="fit_test")fit_test();
else if(part=="check_display")display_board();
else if(part=="check_carrier")carrier();
else if(part=="check_pm")pm_module();
else if(part=="check_scd")gas_module(scd_origin);
else if(part=="check_sgp")gas_module(sgp_origin,"sgp");
else if(part=="check_rear")rear();
else if(part=="check_mount")sensor_mount();
else if(part=="pcb") {carrier();if(full)pm_module();}
else if(part=="exploded")exploded();
else {
 color("#344e58")stand();
 pose(){color("#e4e0d5")bezel();display_board();color("#3e5962")rear();}
}
