$fn = 16;


difference(){
    shape();
    
    
    //translate([4,0,0])
    //  cube([57, 10, 1.5]);
    
    translate([4,38,0])
      cube([57, 15, 1.5]);

    translate([10.25, 31, 6])
        cylinder(h=9, r=1.55, center=true);
    translate([65-10.25, 31, 6])
        cylinder(h=9, r=1.55, center=true);

    translate([10.25, 31, 1])
        cylinder(h=2, r=2.6, center=true);
    translate([65-10.25, 31, 1])
        cylinder(h=2, r=2.6, center=true);

}

module shape() {
    cube([65, 48, 1.5]);
    
    translate([0, 18, 0])
      cube([65, 1.5, 8]);
    translate([0, 35, 0])
      cube([65, 1.5, 8]);
    
    cube([3, 48, 8]);
      translate([65-3, 0, 0])
    cube([3, 48, 8]);

    translate([1.5, 0, 0])
      cube([1.5, 48, 9.6]);
    
    translate([65-3, 0, 0])
      cube([1.5, 48, 9.6]);
    
    translate([10.25, 31, 4])
        cylinder(h=8, r1=6, r2=3 , center=true);
    
    translate([65-10.25, 31, 4])
        cylinder(h=8, r1=6, r2=3, center=true);
}