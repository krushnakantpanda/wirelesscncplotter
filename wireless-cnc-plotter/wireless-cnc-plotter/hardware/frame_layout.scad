// Top-down envelope model only; not a complete machine or fabrication-ready drawing.
// Open in OpenSCAD. Units: mm.
$fn = 32;
frame_x = 300;
frame_y = 240;
rail = 20;
beam_x = 260;
beam_y = 20;
beam_position_y = 70; // gantry location in frame
work_x = 180;
work_y = 120;
module extrusion(x,y,z,dx,dy,dz) { translate([x,y,z]) cube([dx,dy,dz]); }
color("#475569") {
  extrusion(0,0,0,frame_x,rail,rail);
  extrusion(0,frame_y-rail,0,frame_x,rail,rail);
  extrusion(0,rail,0,rail,frame_y-2*rail,rail);
  extrusion(frame_x-rail,rail,0,rail,frame_y-2*rail,rail);
}
color("#2563eb") extrusion((frame_x-beam_x)/2,beam_position_y,22,beam_x,beam_y,20);
color("#db2777") translate([frame_x/2-12,beam_position_y-20,42]) cube([24,60,12]);
color([0.7,0.9,0.4,0.45]) translate([(frame_x-work_x)/2,(frame_y-work_y)/2,21]) cube([work_x,work_y,1]);
// Axes shown in plan view; physical belts, wheels, mounts, motor, idlers and brackets omitted.
