#!/usr/bin/perl

# OBJ -> .h converter. Ignores specified normals and recalculates them as
# as face normals.

use warnings;
use strict;

my $scale = 3.7;
my $offset = 0;
my $name = "core";

my @vertices;
my @texcoords;
my @faces;

my $material = 0;
my $lastmaterial = 0;
my %materials = ();

my $texcoord_min_u = 0;
my $texcoord_min_v = 0;

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
        $texcoord_min_u = $1 < $texcoord_min_u ? $1 : $texcoord_min_u;
        $texcoord_min_v = $2 < $texcoord_min_v ? $2 : $texcoord_min_v;
        push @texcoords, [$1, $2];
    }

    if( $_ =~ /f ([0-9]+)\/([0-9]*)\/([0-9]+) ([0-9]+)\/([0-9]*)\/([0-9]+) ([0-9]+)\/([0-9]*)\/([0-9]+)/ ) {
        push @faces, [$1, $4, $7, 0, $2, $5, $8, $material]; 
    }
}

# Make texcoords positive (they wrap on 1)
$texcoord_min_u = int($texcoord_min_u) + 1.0;
$texcoord_min_v = int($texcoord_min_v) + 1.0;

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

print "static vertex_t vertices[] = {\n";
foreach(@vertices) {
    my @vertex = @{$_};
    print "    {FLOAT_FIXED(" . $vertex[0]*$scale .
        "), FLOAT_FIXED(" . $vertex[1]*$scale .
        "), FLOAT_FIXED(" . $vertex[2]*$scale . ")}, \n";
}
print "};\n\n";

print "static vertex_t normals[] = {\n";
foreach(@normalsarr) {
        my @normal = @{$_};
        print "    {FLOAT_FIXED(" . $normal[0]*1.0 .
                "), FLOAT_FIXED(" . $normal[1]*1.0 .
                "), FLOAT_FIXED(" . $normal[2]*1.0 . ")}, \n";
}
print "};\n\n";

print "static texcoord_t texcoords[] = {\n";
foreach(@texcoords) {
        my @texcoord = @{$_};
        print "    {FLOAT_FIXED(" . ($texcoord[0] + $texcoord_min_u) .
                "), FLOAT_FIXED(" . ($texcoord[1] + $texcoord_min_v) . ")}, \n";
}
print "};\n\n";

print "static triangle_t faces[] = {\n";
foreach(@faces) {
    my @face = @{$_};
    print "    {" . ($face[0] - 1 + $offset) . ", " .
                ($face[1] - 1 + $offset) . ", " .
                ($face[2] - 1 + $offset) . ", " .
                ($face[3] - 1 + $offset) . ", " .
                ($face[4] - 1 + $offset) . ", " .
                ($face[5] - 1 + $offset) . ", " .
                ($face[6] - 1 + $offset) . ", " .				
                ($face[7]) . "},\n";
}
print "};\n\n";

print "model_t get_model_$name() {\n";
print <<"RETURN_FUNC";
    model_t model;

    model.vertices = vertices;
    model.normals = normals;
    model.texcoords = texcoords;
    model.faces = faces;

    model.num_vertices = NUM_VERTICES;
    model.num_normals = NUM_NORMALS;
    model.num_texcoords = NUM_TEXCOORDS;
    model.num_faces = NUM_FACES;

    model.modelview = imat4x4(
        INT_FIXED(1), 0, 0, 0,
        0, INT_FIXED(1), 0, 0,
        0, 0, INT_FIXED(1), 0,
        0, 0, 0, INT_FIXED(1)
    );

    return model;
}

RETURN_FUNC
