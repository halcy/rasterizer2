/**
* Basic rasterizer / affine texture mapper
*/

#include <stdlib.h>
#include <string.h>

#include "rasterize.h"

// Viewport transform
#define VIEWPORT(x, w, s) (imul(idiv((x), (w)) + INT_FIXED(1), INT_FIXED((s) / 2)))

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
    return(
        d1 - d2
    );
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
    for(int32_t m = 0; m < num_models; m++) {
        memcpy(&sorted_triangles[face_offset], models[m].faces, sizeof(triangle_t) * models[m].num_faces);
        for(int i = face_offset; i < face_offset + models[m].num_faces; i++) {
            sorted_triangles[i].model_id = m;
        }
        face_offset += models[m].num_faces;
    }
}

// Cleanup
void free_geometry_storage() {
    free(transformed_vertices);
    free(sorted_triangles);
}

// Actual model rasterizer. Prepare model storage before rendering (whenever scene changes)
void rasterize(uint8_t* framebuffer, model_t* models, int32_t num_models, imat4x4_t camera, imat4x4_t projection) {
    int32_t vert_offset = 0;
    for(int32_t m = 0; m < num_models; m++) {
        // Mvp matrix from camera, mv and p
        imat4x4_t mvp = imat4x4mul(models[m].modelview, camera);
        mvp = imat4x4mul(projection, mvp);

        // Transform all vertices
        shade_vertex_t transform_vertex;
        for(int32_t i = 0; i < models[m].num_vertices; i++) {
            transform_vertex.p = imat4x4transform(mvp, ivec4(models[m].vertices[i].x, models[m].vertices[i].y, models[m].vertices[i].z, INT_FIXED(1)));

            if(transform_vertex.p.z <= 0 || transform_vertex.p.z >= transform_vertex.p.w) {
                transformed_vertices[i + vert_offset].clip = 1;
                continue;
            }
            else {
                transformed_vertices[i + vert_offset].clip = 0;
            }

            // Perspective divide and viewport transform
            transformed_vertices[i + vert_offset].p = ivec3(
                VIEWPORT(transform_vertex.p.x, transform_vertex.p.w, SCREEN_WIDTH),
                VIEWPORT(transform_vertex.p.y, transform_vertex.p.w, SCREEN_HEIGHT),
                transform_vertex.p.z
            );
        }

        vert_offset += models[m].num_vertices;
    }

    // Depth sort
    qsort(sorted_triangles, num_faces_total, sizeof(triangle_t), &triAvgDepthCompare);
    
    // Figure out horizon height
    ivec4_t horizon;
    ivec4_t best_horizon = ivec4(0, 0, 0, 0);
    for(int angle = 0; angle < INT_FIXED(1); angle += FLOAT_FIXED(0.05)) {
        horizon = imat4x4transform(camera, ivec4(isin(angle) << 7, 0, icos(angle) << 7, INT_FIXED(0)));
        if(iabs(horizon.z) > iabs(best_horizon.z)) {
            best_horizon = horizon;
        }
    }
    horizon = imat4x4transform(projection, best_horizon);

    // Draw floor and sky
    int32_t horizon_y = FIXED_INT(VIEWPORT(horizon.y, horizon.w, SCREEN_HEIGHT));
    horizon_y = imin(imax(0, horizon_y), SCREEN_HEIGHT - 1);
    memset(&framebuffer[0], RGB332(70, 70, 70), horizon_y * SCREEN_WIDTH);    
    memset(&framebuffer[horizon_y * SCREEN_WIDTH], RGB332(1<<5, 1<<5, 1<<6), (SCREEN_HEIGHT - horizon_y) * SCREEN_WIDTH);
    
    // Rasterize triangle-order
    transformed_triangle_t tri;
    uint32_t skip = 0;
    for(int32_t i = 0; i < num_faces_total; i++ ) {
        skip = 0;

        // Set up triangle
        for(int ver = 0; ver < 3; ver++) {
            tri.v[ver] = transformed_vertices[sorted_triangles[i].v[ver]];
        }
        
        // Clip / Cull
        for(int ver = 0; ver < 3; ver++) {
            // Vertex clips -> triangle is clipped
            if(tri.v[ver].clip == 1) {
                skip = 1;
                break;
            }
            
            // Cull backfaces
            if( imul(tri.v[1].p.x - tri.v[0].p.x, tri.v[2].p.y - tri.v[0].p.y) -
                imul(tri.v[2].p.x - tri.v[0].p.x, tri.v[1].p.y - tri.v[0].p.y) < 0) {
                skip = 1;
                break;
            }
        }

        // Clipped / culled
        if(skip == 1) {
            continue;
        }
        
        // Set up tex coords
        for(int ver = 0; ver < 3; ver++) {        
            tri.v[ver].uw = models[sorted_triangles[i].model_id].texcoords[sorted_triangles[i].v[ver + 4]].u;
            tri.v[ver].vw = models[sorted_triangles[i].model_id].texcoords[sorted_triangles[i].v[ver + 4]].v;           
        }
        
        // Shade (Hemi lighting, per face)
        ivec3_t light_dir = ivec3norm(ivec3(FLOAT_FIXED(0.5), FLOAT_FIXED(1.0), FLOAT_FIXED(0.5)));
        tri.shade = imin(FLOAT_FIXED(1.0), FLOAT_FIXED(0.1) + imax(0, ivec3dot(models[sorted_triangles[i].model_id].normals[sorted_triangles[i].v[3]], light_dir)));
            
        // Draw triangle
        rasterize_triangle(framebuffer, &tri, sorted_triangles[i].texture);
    }
}
