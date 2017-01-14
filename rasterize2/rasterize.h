#ifndef __RASTERIZE_H__
#define __RASTERIZE_H__

#include "fixedmath.h"

// Hardcoded configuration for rasterizer
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
#define ZNEAR FLOAT_FIXED(0.1)
#define ZFAR FLOAT_FIXED(1024.0)

// 0-255 R G B to packed RGB332
#define RGB332(r, g, b) ((((r) >> 5) & 0x07) << 5 | (((g) >> 5 ) & 0x07) << 2 | (((b) >> 6) & 0x03))

// Texture transform for power-of-two textures
//#define TEX_SIZE 1024
//#define TEX_SIZE_LOG2 10
#define TEX_SIZE 128
#define TEX_SIZE_LOG2 7
#define TEX_SCALE(x) ((x) >> (12 - TEX_SIZE_LOG2))
#define TEX_TRANSFORM(u, v) (((TEX_SCALE(v) & (TEX_SIZE - 1)) << TEX_SIZE_LOG2) + (TEX_SCALE(u) & (TEX_SIZE - 1)))

// Viewport transform
#define VIEWPORT(x, w, s) (imul(idiv((x), (w)) + INT_FIXED(1), INT_FIXED((s) / 2)))
#define VIEWPORT_NO_PERSPECTIVE(x, s) (imul((x) + INT_FIXED(1), INT_FIXED((s) / 2)))

// Vertex / Triangle as stored by model (per-face normals)
typedef ivec3_t vertex_t;

typedef struct {
    int32_t u;
    int32_t v;
} texcoord_t;

typedef struct {
    int32_t v[8]; // p0, p1, p2, n, t1, t2, t3, texid
    uint8_t model_id;
    uint8_t* texture;
} triangle_t;

// Vertex during transformation and shading
typedef struct {
    ivec4_t p;
    ivec3_t n;
} shade_vertex_t;

// Vertex in post-transform space
typedef struct {
    ivec4_t cp;
    ivec3_t p;
    uint16_t clip;
    int32_t uw;
    int32_t vw;
} transformed_vertex_t;

// A single post-transform triangle
typedef struct {
    transformed_vertex_t v[3];
    int32_t shade;    
} transformed_triangle_t;

// A model: Backing vertices / normals / texcoords / faces, 
// number of vertices / normals / texcoords / faces, modelview matrix
typedef struct {
    vertex_t* vertices;
    vertex_t* normals;
    texcoord_t* texcoords;
    triangle_t* faces;

    int16_t num_vertices;
    int16_t num_normals;
    int16_t num_texcoords;
    int16_t num_faces;

    int32_t draw;

    imat4x4_t modelview;
} model_t;

// Actual model drawer
void prepare_geometry_storage(model_t* models, int32_t num_models);
void free_geometry_storage();
void rasterize(uint8_t* framebuffer, model_t* models, int32_t num_models, imat4x4_t camera, imat4x4_t projection, uint8_t* floor_tex, uint8_t sky_color);

#endif
