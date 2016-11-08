#!/usr/bin/perl

# OBJ -> .h converter. Ignores specified normals and recalculates them as
# as face normals.

use warnings;
use strict;

my $scale = 0.4;
my $offset = 0;
my $name = "cityscape";

my @vertices;
my @texcoords;
my @faces;

my $material = 0;
my $lastmaterial = 0;
my %materials = ();

while(<>) {
    if( $_ =~ /usemtl ([^ ]+)/ ) {
        if(defined $materials{$1}) {
            $material = $materials{$1};
        }
        else {
            $material = $lastmaterial;
            $materials{$1} = $material;
            $lastmaterial++;
        }
    }
        
    if( $_ =~ /v ([^ ]+) ([^ ]+) ([^ ]+)/ ) {
        push @vertices, [$1, $2, $3];
    }
    
    if( $_ =~ /vt ([^ ]+) ([^ ]+)/ ) {
        push @texcoords, [$1, $2];
    }

    if( $_ =~ /f ([0-9]+)\/([0-9]*)\/([0-9]+) ([0-9]+)\/([0-9]*)\/([0-9]+) ([0-9]+)\/([0-9]*)\/([0-9]+)/ ) {
        push @faces, [$1, $4, $7, 0, $2, $5, $8]; 
    }
}

my @faces_proper;
my %normals = ();
my @normalsarr = ();
my $normalidx = 1;
for(my $faceidx = 0; $faceidx < scalar @faces; $faceidx++) {
    my @face = @{$faces[$faceidx]};
    my @a = @{$vertices[$face[0] - 1 + $offset]};
    my @b = @{$vertices[$face[1] - 1 + $offset]};
    my @c = @{$vertices[$face[2] - 1 + $offset]};
    
    my @ab = ($b[0] - $a[0], $b[1] - $a[1], $b[2] - $a[2]);
    my $ablen = sqrt($ab[0] * $ab[0] + $ab[1] * $ab[1] + $ab[2] * $ab[2]);
    
    my @ac = ($c[0] - $a[0], $c[1] - $a[1], $c[2] - $a[2]);
    my $aclen = sqrt($ac[0] * $ac[0] + $ac[1] * $ac[1] + $ac[2] * $ac[2]);
    
    if($aclen == 0 || $ablen == 0) {
        next;
    }
    
    @ab = ($ab[0] / $ablen, $ab[1] / $ablen, $ab[2] / $ablen);
    @ac = ($ac[0] / $aclen, $ac[1] / $aclen, $ac[2] / $aclen);
    
    my @p = (
        $ab[1] * $ac[2] - $ab[2] * $ac[1],
        $ab[2] * $ac[0] - $ab[0] * $ac[2],
        $ab[0] * $ac[1] - $ab[1] * $ac[0]
    );
    my $plen = sqrt($p[0] * $p[0] + $p[1] * $p[1] + $p[2] * $p[2]);
    if($plen == 0) {
        splice(@faces, $faceidx, 1);
        $faceidx--;
        next;
    }
    @p = ($p[0] / $plen, $p[1] / $plen, $p[2] / $plen);
    my $normalid = $p[0] . "---" . $p[1] . "---" . $p[2];
    my $normalex = $normals{$normalid};
    my $normalexidx = 0;
    if(!defined $normalex) {
        $normalexidx = $normalidx;
        $normals{$normalid} = $normalidx;
        push @normalsarr, \@p;
        $normalidx++;
    }
    else {
        $normalexidx = $normalex;
    }
    
    $faces[$faceidx]->[3] = $normalexidx;
    
    push(@faces_proper, $faces[$faceidx]);
}

print "/**\n * Model: $name\n */\n\n";

print "#include \"rasterize.h\"\n\n";

print "#define NUM_VERTICES " . scalar @vertices . "\n";
print "#define NUM_NORMALS " . scalar @normalsarr . "\n";
print "#define NUM_TEXCOORDS " . scalar @texcoords . "\n";
print "#define NUM_FACES " . scalar @faces_proper . "\n\n";

print "static const vertex_t vertices[] = {\n";
foreach(@vertices) {
    my @vertex = @{$_};
    print "\t{FLOAT_FIXED(" . $vertex[0]*$scale .
        "), FLOAT_FIXED(" . $vertex[1]*$scale .
        "), FLOAT_FIXED(" . $vertex[2]*$scale . ")}, \n";
}
print "};\n\n";

print "static const vertex_t normals[] = {\n";
foreach(@normalsarr) {
        my @normal = @{$_};
        print "\t{FLOAT_FIXED(" . $normal[0]*1.0 .
                "), FLOAT_FIXED(" . $normal[1]*1.0 .
                "), FLOAT_FIXED(" . $normal[2]*1.0 . ")}, \n";
}
print "};\n\n";

print "static const texcoord_t texcoords[] = {\n";
foreach(@texcoords) {
        my @texcoord = @{$_};
        print "\t{FLOAT_FIXED(" . $texcoord[0]*1.0 .
                "), FLOAT_FIXED(" . $texcoord[1]*1.0 . ")}, \n";
}
print "};\n\n";

print "static const triangle_t faces[] = {\n";
foreach(@faces) {
    my @face = @{$_};
    print "\t{" . ($face[0] - 1 + $offset) . ", " .
                ($face[1] - 1 + $offset) . ", " .
                ($face[2] - 1 + $offset) . ", " .
                ($face[3] - 1 + $offset) . ", " .
                ($face[4] - 1 + $offset) . ", " .
                ($face[5] - 1 + $offset) . ", " .
                ($face[6] - 1 + $offset) . "},\n";
}
print "};\n";
