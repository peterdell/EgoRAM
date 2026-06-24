$fn = 16;

difference(){
    shape();

    translate([5,0,0])
      cube([55, 10, 1.5]);

    translate([4,38,0])
      cube([57, 15, 1.5]);

    translate([10.25, 31, 7])
        cylinder(h=8, r=1.4, center=true);
    
    translate([65-10.25, 31, 7])
        cylinder(h=8, r=1.4, center=true);
}

module shape() {
    cube([65, 48, 1.5]);
    
    translate([0,18,0])
      cube([65, 1.5, 8]);
    translate([0, 35, 0])
      cube([65, 1.5, 8]);
    
    cube([1.5, 48, 9.6]);
      translate([65-1.5, 0, 0])
    cube([1.5, 48, 9.6]);
    
    translate([10.25, 31, 4.8])
        cylinder(h=7, r1=6, r2=5.5/2, center=true);
    translate([65-10.25, 31, 4.8])
        cylinder(h=7, r1=6, r2=5.5/2, center=true);

    translate([10.25, 31, 4.8])
        cylinder(h=9.6, r=5.5/2, center=true);
    translate([65-10.25, 31, 4.8])
        cylinder(h=9.6, r=5.5/2, center=true);
}