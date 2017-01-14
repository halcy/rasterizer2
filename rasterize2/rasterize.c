/**
* Basic rasterizer / affine texture mapper
*/

#include <stdlib.h>
#include <string.h>

#include "rasterize.h"

#define RGBCOMPSCALE(col, shift, mask, s) ((FIXED_INT_ROUND(imul(INT_FIXED(((col) >> (shift)) & (mask)), (s)))) << (shift))
#define RGB322SCALE(col, s) (RGBCOMPSCALE(col, 5, 0x07, s) + RGBCOMPSCALE(col, 2, 0x07, s) + RGBCOMPSCALE(col, 0, 0x03, s))
//#define RGB322SCALE(col, s) (col)

// Storage for post-transform vertices / texcoords / triangles
static int32_t num_vertices_total = 0;
static transformed_vertex_t* transformed_vertices = 0;

static int32_t num_faces_total = 0;
static triangle_t* sorted_triangles = 0;

// Triangle drawer
static inline void rasterize_triangle(uint8_t* image, transformed_triangle_t* tri, uint8_t* shadetex) {
    // Local vertex sorting
    transformed_vertex_t upperVertex;
    transformed_vertex_t centerVertex;
    transformed_vertex_t lowerVertex;

    if(tri->v[0].p.y < tri->v[1].p.y) {
        upperVertex = tri->v[0];
        lowerVertex = tri->v[1];
    }
    else {
        upperVertex = tri->v[1];
        lowerVertex = tri->v[0];
    }

    if(tri->v[2].p.y < upperVertex.p.y) {
        centerVertex = upperVertex;
        upperVertex = tri->v[2];
    }
    else {
        if(tri->v[2].p.y > lowerVertex.p.y) {
            centerVertex = lowerVertex;
            lowerVertex = tri->v[2];
        }
        else {
            centerVertex = tri->v[2];
        }
    }

    // Scanline counters
    int32_t scanline;
    int32_t scanlineMax;

    // Left / right x and deltas
    int32_t leftX;
    int32_t leftXd;
    int32_t rightX;
    int32_t rightXd;

    // Left texcoords and texcoord delta
    int32_t leftU;
    int32_t leftV;
    int32_t leftUd;
    int32_t leftVd;

    // Texcoords and texcoord x deltas
    int32_t U;
    int32_t V;
    int32_t UdX;
    int32_t VdX;

    // Calculate y differences
    int32_t upperDiff = upperVertex.p.y - centerVertex.p.y;
    int32_t lowerDiff = upperVertex.p.y - lowerVertex.p.y;

    // Deltas
    int32_t upperCenter;
    int32_t upperLower;

    // Guard against special case A: No triangle
    if(lowerDiff == 0 && upperDiff == 0) {
        return;
    }
    
    // Calculate whole-triangle deltas
    int32_t temp = idiv(centerVertex.p.y - upperVertex.p.y,lowerVertex.p.y - upperVertex.p.y);
    int32_t width = imul(temp, (lowerVertex.p.x - upperVertex.p.x)) + (upperVertex.p.x - centerVertex.p.x);
    if(width == 0) {
        return;
    }
    UdX = idiv(imul(temp, lowerVertex.uw - upperVertex.uw) + upperVertex.uw - centerVertex.uw, width);
    VdX = idiv(imul(temp, lowerVertex.vw - upperVertex.vw) + upperVertex.vw - centerVertex.vw, width);
    
    // Guard against special case B: Flat upper edge
    if(upperDiff == 0 ) {
        if(upperVertex.p.x < centerVertex.p.x) {
            leftX = upperVertex.p.x;
            leftU = upperVertex.uw;
            leftV = upperVertex.vw;
            rightX = centerVertex.p.x;

            leftXd = idiv(upperVertex.p.x - lowerVertex.p.x, lowerDiff);
            rightXd = idiv(centerVertex.p.x - lowerVertex.p.x, lowerDiff);
        }
        else {
            leftX = centerVertex.p.x;
            leftU = centerVertex.uw;
            leftV = centerVertex.vw;
            rightX = upperVertex.p.x;

            leftXd = idiv(centerVertex.p.x - lowerVertex.p.x, lowerDiff);
            rightXd = idiv(upperVertex.p.x - lowerVertex.p.x, lowerDiff);
        }

        leftUd = idiv(leftU - lowerVertex.uw, lowerDiff);
        leftVd = idiv(leftV - lowerVertex.vw, lowerDiff);

        goto lower_half_render;
    }

    // Calculate upper triangle half deltas
    upperCenter = idiv(upperVertex.p.x - centerVertex.p.x, upperDiff);
    upperLower = idiv(upperVertex.p.x - lowerVertex.p.x, lowerDiff);

    // Upper triangle half
    leftX = rightX = upperVertex.p.x;

    leftU = upperVertex.uw;
    leftV = upperVertex.vw;
    
    if(upperCenter < upperLower) {
        leftXd = upperCenter;
        rightXd = upperLower;

        leftUd = idiv(leftU - centerVertex.uw, upperDiff);
        leftVd = idiv(leftV - centerVertex.vw, upperDiff);
    }
    else {
        leftXd = upperLower;
        rightXd = upperCenter;

        leftUd = idiv(leftU - lowerVertex.uw, lowerDiff);
        leftVd = idiv(leftV - lowerVertex.vw, lowerDiff);
    }

    U = leftU;
    V = leftV;
    
    scanlineMax = imin(FIXED_INT_ROUND(centerVertex.p.y), SCREEN_HEIGHT - 1);
    for(scanline = FIXED_INT_ROUND(upperVertex.p.y); scanline < scanlineMax; scanline++ ) {
        if(scanline >= 0) {
            int32_t xMax = imin(FIXED_INT_ROUND(rightX), SCREEN_WIDTH - 1);
            if(xMax >= 0) {
                int32_t offset = scanline * SCREEN_WIDTH;
                int32_t x = FIXED_INT_ROUND(leftX);

                while(x <= -1) {
                    U += UdX;
                    V += VdX;
                    x++;
                }
                
                while(x <= xMax) {
                    image[x+offset] = RGB322SCALE(shadetex[TEX_TRANSFORM(U, V)], tri->shade);
                    x++;
                    U += UdX;
                    V += VdX;
                }
            }
        }

        leftX += leftXd;
        rightX += rightXd;
        leftU += leftUd;
        leftV += leftVd;

        U = leftU;
        V = leftV;
    }
        
    // Guard against special case C: flat lower edge
    int32_t centerDiff = centerVertex.p.y - lowerVertex.p.y;
    if(centerDiff == 0) {
        return;
    }

    // Calculate lower triangle half deltas
    if(upperCenter < upperLower) {
        leftX = centerVertex.p.x;
        leftXd = idiv(centerVertex.p.x - lowerVertex.p.x, centerDiff);

        leftU = centerVertex.uw;
        leftV = centerVertex.vw;

        leftUd = idiv(leftU - lowerVertex.uw, centerDiff);
        leftVd = idiv(leftV - lowerVertex.vw, centerDiff);
    }
    else {
        rightX = centerVertex.p.x;
        rightXd = idiv(centerVertex.p.x - lowerVertex.p.x, centerDiff);
    }

lower_half_render:

    // Lower triangle half
    scanlineMax = imin(FIXED_INT_ROUND(lowerVertex.p.y), SCREEN_HEIGHT - 1);
        
    U = leftU;
    V = leftV;

    for(scanline = FIXED_INT_ROUND(centerVertex.p.y); scanline < scanlineMax; scanline++ ) {
        if(scanline >= 0) {
            int32_t xMax = imin(FIXED_INT_ROUND(rightX), SCREEN_WIDTH - 1);
            if(xMax >= 0) {
                int32_t offset = scanline * SCREEN_WIDTH;
                int32_t x = FIXED_INT_ROUND(leftX);

                while(x <= -1) {
                    U += UdX;
                    V += VdX;
                    x++;
                }
                while(x <= xMax) {
                    image[x+offset] = RGB322SCALE(shadetex[TEX_TRANSFORM(U, V)], tri->shade);
                    x++;
                    U += UdX;
                    V += VdX;
                }
            }
        }
                
        leftX += leftXd;
        rightX += rightXd;
        leftU += leftUd;
        U = leftU;
        leftV += leftVd;
        V = leftV;
    }
}

// Depth sorting comparator for comparing by average (sum) depth
static int triAvgDepthCompare(const void *p1, const void *p2) {
    triangle_t* t1 = (triangle_t*)p1;
    triangle_t* t2 = (triangle_t*)p2;
    return(
        transformed_vertices[t2->v[0]].p.z +
        transformed_vertices[t2->v[1]].p.z +
        transformed_vertices[t2->v[2]].p.z -
        transformed_vertices[t1->v[0]].p.z -
        transformed_vertices[t1->v[1]].p.z -
        transformed_vertices[t1->v[2]].p.z
    );
}

// Depth sorting comparator for comparing by miminal depth
static int triClosestDepthCompare(const void *p1, const void *p2) {
    triangle_t* t1 = (triangle_t*)p1;
    triangle_t* t2 = (triangle_t*)p2;
    int d1 = imin(imin(
            transformed_vertices[t1->v[0]].p.z,
            transformed_vertices[t1->v[1]].p.z
        ),
        transformed_vertices[t1->v[2]].p.z
    );
    int d2 = imin(imin(
            transformed_vertices[t2->v[0]].p.z,
            transformed_vertices[t2->v[1]].p.z
        ),
        transformed_vertices[t2->v[2]].p.z
    );
    return(d1 - d2);
}

// Set up storage for geometry and copy face data
void prepare_geometry_storage(model_t* models, int32_t num_models) {
    // Count vertices / faces
    int32_t vert_count = 0;
    int32_t face_count = 0;
    for(int32_t m = 0; m < num_models; m++) {
        vert_count += models[m].num_vertices;
        face_count += models[m].num_faces;
    }

    // (Re)alloc storage
    if(vert_count > num_vertices_total || transformed_vertices == 0) {
        num_vertices_total = vert_count;
        transformed_vertices = (transformed_vertex_t*)realloc(transformed_vertices, sizeof(transformed_vertex_t) * num_vertices_total);
    }

    if (face_count > num_faces_total || sorted_triangles == 0) {
        num_faces_total = face_count;
        sorted_triangles = (triangle_t*)realloc(sorted_triangles, sizeof(triangle_t) * num_faces_total);
    }

    // Copy face data
    int32_t face_offset = 0;
    int32_t vert_offset = 0;
    for(int32_t m = 0; m < num_models; m++) {
        memcpy(&sorted_triangles[face_offset], models[m].faces, sizeof(triangle_t) * models[m].num_faces);
        for(int i = face_offset; i < face_offset + models[m].num_faces; i++) {
            sorted_triangles[i].model_id = m;
            for(int j = 0; j < 3; j++) {
                sorted_triangles[i].v[j] += vert_offset;
            }
        }
        vert_offset +=  models[m].num_vertices;
        face_offset += models[m].num_faces;
    }

    num_faces_total = face_count;
    num_vertices_total = vert_count;
}

// Cleanup
void free_geometry_storage() {
    free(transformed_vertices);
    free(sorted_triangles);
}

// Clip a line against znear
ivec4_t clip_line(ivec4_t a, ivec4_t b) {
    int32_t dist = idiv(a.z, (a.z - b.z));

    a.x = a.x + imul(dist, b.x - a.x);
    a.y = a.y + imul(dist, b.y - a.y);
    a.z = a.z + imul(dist, b.z - a.z);
    a.w = a.w + imul(dist, b.w - a.w);

    return a;
}

// Set up shading information for a normal textured shaded triangle
// from model info and index
void set_shading(uint8_t* framebuffer, model_t* models, int32_t tri_idx, transformed_triangle_t* tri) {
    // Set up tex coords
    for(int ver = 0; ver < 3; ver++) {        
        tri->v[ver].uw = models[sorted_triangles[tri_idx].model_id].texcoords[sorted_triangles[tri_idx].v[ver + 4]].u;
        tri->v[ver].vw = models[sorted_triangles[tri_idx].model_id].texcoords[sorted_triangles[tri_idx].v[ver + 4]].v;           
    }

    // Shade (Hemi lighting, per face)
    ivec3_t light_dir = ivec3norm(ivec3(FLOAT_FIXED(0.5), FLOAT_FIXED(1.0), FLOAT_FIXED(0.5)));
    // TODO rotate
    ivec3_t norm = models[sorted_triangles[tri_idx].model_id].normals[sorted_triangles[tri_idx].v[3]];
    ivec4_t norm_tranformed = imat4x4transform( models[sorted_triangles[tri_idx].model_id].modelview, ivec4(norm.x, norm.y, norm.z, 0));
    ivec3_t norm_proper = ivec3norm(ivec3(norm_tranformed.x, norm_tranformed.y, norm_tranformed.z));
    tri->shade = imin(FLOAT_FIXED(1.0), FLOAT_FIXED(0.1) + imax(0, ivec3dot(norm_proper, light_dir)));
}

// Draw a single triangle, view clipping against near/far if need be
void clip_rasterize(uint8_t* framebuffer, model_t* models, int32_t tri_idx, transformed_triangle_t tri, uint8_t* texture_override) {
    // Check what needs clipping
    uint32_t clip = 0;

    // Clip A, B, C are the vertices with clipping verts first
    int clip_a = -1;
    int clip_b = -1;
    int clip_c = -1;

    for(int ver = 0; ver < 3; ver++) {
        clip += tri.v[ver].clip;
        
        // Vertex clips
        if(tri.v[ver].clip == 1) {
            if(clip_a == -1) {
                clip_a = ver;
            }
            else {
                clip_b = ver;
            }
        }
        else {
            if(clip_c == -1) {
                clip_c = ver;
            }
            else {
                clip_b = ver;
            }
        }
    }

    // All vertices clip
    if(clip + ((clip & 0xFF) << 8) >= 0x300) {
        return;
    }
    clip = clip & 0xFF;

    // One vertex out -> quad, so copy tri 
    if(clip == 1) {
        ivec4_t transform_pos = clip_line(tri.v[clip_a].cp, tri.v[clip_b].cp);
        if(transform_pos.w == 0) {
            return;
        }

        tri.v[clip_a].p = ivec3(
            VIEWPORT(transform_pos.x, transform_pos.w, SCREEN_WIDTH),
            VIEWPORT(transform_pos.y, transform_pos.w, SCREEN_HEIGHT),
            transform_pos.z
        );

        // Additional draw for the bonus triangle
        if(texture_override == 0) {
            set_shading(framebuffer, models, tri_idx, &tri);
            rasterize_triangle(framebuffer, &tri, sorted_triangles[tri_idx].texture); 
        }
        else {
            rasterize_triangle(framebuffer, &tri, texture_override); 
        }
        
        // Set up final triangle
        tri.v[clip_b] = tri.v[clip_c];
        transform_pos = clip_line(tri.v[clip_a].cp, tri.v[clip_c].cp);
        if(transform_pos.w == 0) {
            return;
        }

        tri.v[clip_c].p = ivec3(
            VIEWPORT(transform_pos.x, transform_pos.w, SCREEN_WIDTH),
            VIEWPORT(transform_pos.y, transform_pos.w, SCREEN_HEIGHT),
            transform_pos.z
        );
    }

    // Two vertices out -> tri again
    if (clip == 2) {   
        ivec4_t transform_pos = clip_line(tri.v[clip_a].cp, tri.v[clip_c].cp);
        transform_pos = clip_line(tri.v[clip_a].cp, tri.v[clip_c].cp);
        if(transform_pos.w == 0) {
            return;
        }

        tri.v[clip_a].p = ivec3(
            VIEWPORT(transform_pos.x, transform_pos.w, SCREEN_WIDTH),
            VIEWPORT(transform_pos.y, transform_pos.w, SCREEN_HEIGHT),
            transform_pos.z
        );

        transform_pos = clip_line(tri.v[clip_b].cp, tri.v[clip_c].cp);
        if(transform_pos.w == 0) {
            return;
        }

        tri.v[clip_b].p = ivec3(
            VIEWPORT(transform_pos.x, transform_pos.w, SCREEN_WIDTH),
            VIEWPORT(transform_pos.y, transform_pos.w, SCREEN_HEIGHT),
            transform_pos.z
        );
    }

    if(texture_override == 0) {
        set_shading(framebuffer, models, tri_idx, &tri);
        rasterize_triangle(framebuffer, &tri, sorted_triangles[tri_idx].texture);
    }
    else {
        rasterize_triangle(framebuffer, &tri, texture_override); 
    }
}

// Draw a xz-plane
void draw_floor(uint8_t* framebuffer, imat4x4_t camera, imat4x4_t projection, uint8_t* texture, int32_t height) {
    imat4x4_t mvp = imat4x4mul(projection, camera);

    // Figure out how far above the plane we are so we can clip agressively
    int harsh_clip = 1;
    ivec4_t pos = imat4x4transform(camera, ivec4(0, height, 0, INT_FIXED(1)));
    if(iabs(pos.y) > INT_FIXED(20)) {
        harsh_clip = 3;
    }
    
    for(int x = -32; x <= 32; x++) {
        for(int z = -32; z <= 32; z++) {
            // Skip if outside arena
            if(x * x + z * z > 32 * 32) {
                continue;
            }
            
            // Floor triangle 1
            transformed_triangle_t floor_tri;
            floor_tri.shade = INT_FIXED(1);
            floor_tri.v[0].cp = imat4x4transform(mvp, ivec4(INT_FIXED(8 * x),       height, INT_FIXED(8 * z),       INT_FIXED(1)));
            floor_tri.v[2].cp = imat4x4transform(mvp, ivec4(INT_FIXED(8 * (x + 1)), height, INT_FIXED(8 * z),       INT_FIXED(1)));
            floor_tri.v[1].cp = imat4x4transform(mvp, ivec4(INT_FIXED(8 * x),       height, INT_FIXED(8 * (z + 1)), INT_FIXED(1)));
            
            floor_tri.v[0].uw = INT_FIXED(0);
            floor_tri.v[0].vw = INT_FIXED(0);
            
            floor_tri.v[1].uw = INT_FIXED(1);
            floor_tri.v[1].vw = INT_FIXED(0);
            
            floor_tri.v[2].uw = INT_FIXED(0);
            floor_tri.v[2].vw = INT_FIXED(1);
            
            // Clip?
            for(int i = 0; i < 3; i++) {
                if(floor_tri.v[i].cp.z <= 0) {
                    floor_tri.v[i].clip = harsh_clip;
                    continue;
                }
                else {
                    if(floor_tri.v[i].cp.z >= floor_tri.v[i].cp.w) {
                        floor_tri.v[i].clip = 3;
                        continue;
                    }
                }
                
                // xy clip?
                if(
                    floor_tri.v[i].cp.x >= floor_tri.v[i].cp.w ||
                    floor_tri.v[i].cp.y >= floor_tri.v[i].cp.w ||
                    floor_tri.v[i].cp.x <= -floor_tri.v[i].cp.w ||
                    floor_tri.v[i].cp.y <= -floor_tri.v[i].cp.w 
                ) {
                    floor_tri.v[i].clip = 0x100;
                }

                floor_tri.v[i].p = ivec3(
                    VIEWPORT(floor_tri.v[i].cp.x, floor_tri.v[i].cp.w, SCREEN_WIDTH),
                    VIEWPORT(floor_tri.v[i].cp.y, floor_tri.v[i].cp.w, SCREEN_HEIGHT),
                    floor_tri.v[i].cp.z
                );
                floor_tri.v[i].clip = 0;
            }
            
            // Pass to rasterizer
            clip_rasterize(framebuffer, 0, 0, floor_tri, texture);
            
             // Floor triangle 2
            floor_tri.shade = INT_FIXED(1);
            floor_tri.v[0].cp = imat4x4transform(mvp, ivec4(INT_FIXED(8 * (x + 1)), height, INT_FIXED(8 * (z + 1)), INT_FIXED(1)));
            floor_tri.v[2].cp = imat4x4transform(mvp, ivec4(INT_FIXED(8 * (x + 1)), height, INT_FIXED(8 * z),       INT_FIXED(1)));
            floor_tri.v[1].cp = imat4x4transform(mvp, ivec4(INT_FIXED(8 * x),       height, INT_FIXED(8 * (z + 1)), INT_FIXED(1)));
            
            floor_tri.v[0].uw = INT_FIXED(1);
            floor_tri.v[0].vw = INT_FIXED(1);
            
            floor_tri.v[1].uw = INT_FIXED(1);
            floor_tri.v[1].vw = INT_FIXED(0);
            
            floor_tri.v[2].uw = INT_FIXED(0);
            floor_tri.v[2].vw = INT_FIXED(1);
            
            // Clip?
            for(int i = 0; i < 3; i++) {
                if(floor_tri.v[i].cp.z <= 0) {
                    floor_tri.v[i].clip = harsh_clip;
                    continue;
                }
                else {
                    if(floor_tri.v[i].cp.z >= floor_tri.v[i].cp.w) {
                        floor_tri.v[i].clip = 3;
                        continue;
                    }
                }
                
                floor_tri.v[i].p = ivec3(
                    VIEWPORT(floor_tri.v[i].cp.x, floor_tri.v[i].cp.w, SCREEN_WIDTH),
                    VIEWPORT(floor_tri.v[i].cp.y, floor_tri.v[i].cp.w, SCREEN_HEIGHT),
                    floor_tri.v[i].cp.z
                );
                floor_tri.v[i].clip = 0;                
            }
            
            // Pass to rasterizer
            clip_rasterize(framebuffer, 0, 0, floor_tri, texture);
        }
    }
}

// Actual model rasterizer. Prepare model storage before rendering (whenever scene changes)
void rasterize(uint8_t* framebuffer, model_t* models, int32_t num_models, imat4x4_t camera, imat4x4_t projection, uint8_t* floor_tex, uint8_t sky_color) {
    int32_t vert_offset = 0;
    for(int32_t m = 0; m < num_models; m++) {
        // Mvp matrix from camera, mv and p
        imat4x4_t mvp = imat4x4mul(camera, models[m].modelview);
        mvp = imat4x4mul(projection, mvp);

        // Transform all vertices
        shade_vertex_t transform_vertex;
        for(int32_t i = 0; i < models[m].num_vertices; i++) {
            transform_vertex.p = imat4x4transform(mvp, ivec4(models[m].vertices[i].x, models[m].vertices[i].y, models[m].vertices[i].z, INT_FIXED(1)));
            transformed_vertices[i + vert_offset].cp = transform_vertex.p;

            transformed_vertices[i + vert_offset].clip = 0;

            // Near clip?
            if(transform_vertex.p.z <= 0) {
                transformed_vertices[i + vert_offset].clip = 1;
                continue;
            }
            else {
                // Far clip?
                if(transform_vertex.p.z >= transform_vertex.p.w) {
                    transformed_vertices[i + vert_offset].clip = 3; // Far clip is THREE TIMES as bad as near clip
                    continue;
                }

                // xy clip?
                if(
                    transform_vertex.p.x >= transform_vertex.p.w ||
                    transform_vertex.p.y >= transform_vertex.p.w ||
                    transform_vertex.p.x <= -transform_vertex.p.w ||
                    transform_vertex.p.y <= -transform_vertex.p.w 
                ) {
                    transformed_vertices[i + vert_offset].clip = 0x100;
                }

                // No clipping? Perspective divide and viewport transform
                transformed_vertices[i + vert_offset].p = ivec3(
                    VIEWPORT(transform_vertex.p.x, transform_vertex.p.w, SCREEN_WIDTH),
                    VIEWPORT(transform_vertex.p.y, transform_vertex.p.w, SCREEN_HEIGHT),
                    transform_vertex.p.z
                );
            }
        }

        vert_offset += models[m].num_vertices;
    }

    // Depth sort
    qsort(sorted_triangles, num_faces_total, sizeof(triangle_t), &triAvgDepthCompare);
    
    // Clear screen
    memset(framebuffer, sky_color, SCREEN_HEIGHT * SCREEN_WIDTH);
    
    // Floor / ceiling
    if(floor_tex != 0) {
        draw_floor(framebuffer, camera, projection, floor_tex, INT_FIXED(0));
        draw_floor(framebuffer, camera, projection, floor_tex, INT_FIXED(200));
    }
    
    
    // Draw border
    for(int i = 0; i < 20; i++) {
        ivec4_t dot;
        imat4x4_t mvp = imat4x4mul(projection, camera);
        for(int angle = 0; angle < INT_FIXED(1) - FLOAT_FIXED(0.0125 / 2.0); angle += FLOAT_FIXED(0.0125)) {
            dot =  ivec4(isin(angle) << 8, INT_FIXED(i * 10), icos(angle) << 8, INT_FIXED(1));
            dot = imat4x4transform(mvp, dot);
            
            // Near clip
            if(dot.z > 0) {
                int32_t dot_x = FIXED_INT_ROUND(VIEWPORT(dot.x, dot.w, SCREEN_WIDTH));
                int32_t dot_y = FIXED_INT_ROUND(VIEWPORT(dot.y, dot.w, SCREEN_HEIGHT));
                
                if(dot_x >= 0 && dot_x < SCREEN_WIDTH && dot_y >= 0 && dot_y < SCREEN_HEIGHT) {
                    framebuffer[dot_x + dot_y * SCREEN_WIDTH] = 0xFF;
                }
            }
        }
    }
    
    // Rasterize triangle-order
    transformed_triangle_t tri;

    for(int32_t i = 0; i < num_faces_total; i++ ) {
        // Inefficient, but urgh too lazy to rewrite: skip triangle if model inactive
        if(models[sorted_triangles[i].model_id].draw == 0) {
            continue;
        }

        // Set up triangle
        for(int ver = 0; ver < 3; ver++) {
            tri.v[ver] = transformed_vertices[sorted_triangles[i].v[ver]];
        }
        
        // Cull backfaces 
        if(imul(tri.v[1].p.x - tri.v[0].p.x, tri.v[2].p.y - tri.v[0].p.y) -
            imul(tri.v[2].p.x - tri.v[0].p.x, tri.v[1].p.y - tri.v[0].p.y) < 0) {
            continue;
        }

        clip_rasterize(framebuffer, models, i, tri, 0);
    }
    
    /*
    // Draw a little RGB332 swatch
    int r = 0;
    int g = 0;
    int b = 0;
    for(int y = 0; y < 16; y++) {
        for(int x = 0; x < 16; x++) {
            framebuffer[x + y * SCREEN_WIDTH] = (r << 5) | (g << 2) | b;
            
            r += 1;
            if(r == 0x8) {
                r = 0;
                g++;
            }
            
            if(g == 0x8) {
                g = 0;
                b++;
            }
        }
    }*/
}


