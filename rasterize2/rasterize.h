#ifndef __RASTERIZE_H__
#define __RASTERIZE_H__

#include "fixedmath.h"

// Hardcoded configuration for rasterizer
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define ZNEAR FLOAT_FIXED(0.1)
#define ZFAR FLOAT_FIXED(200.0)

// 0-255 R G B to packed RGB332
#define RGB332(r, g, b) ((((r) >> 5) & 0x07) << 5 | (((g) >> 5 ) & 0x07) << 2 | (((b) >> 6) & 0x03))

// Texture transform for power-of-two textures
#define TEX_SIZE 128
#define TEX_SIZE_LOG2 7
#define TEX_SCALE(x) ((x) >> (12 - TEX_SIZE_LOG2))
#define TEX_TRANSFORM(u, v) (((TEX_SCALE(v) & (TEX_SIZE - 1)) << TEX_SIZE_LOG2) + (TEX_SCALE(u) & (TEX_SIZE - 1)))

// Vertex / Triangle as stored by model (per-face normals)
typedef ivec3_t vertex_t;

typedef struct {
    int32_t u;
    int32_t v;
} texcoord_t;

typedef struct {
    int16_t v[8]; // p0, p1, p2, n, t1, t2, t3, texid
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
    ivec3_t p;
    uint8_t clip;
    int32_t uw;
    int32_t vw;
} transformed_vertex_t;

// A single post-transform triangle
typedef struct {
    transformed_vertex_t v[3];
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

    imat4x4_t modelview;
} model_t;

// Actual model drawer
void rasterize(uint8_t* framebuffer, model_t* models, int32_t num_models, imat4x4_t camera, imat4x4_t projection);

#endif
