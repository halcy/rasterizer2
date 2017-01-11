/**
 * Entry point. Rasterizes a simple model and displays it via a GLUT window.
 */
#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glut.h>

#ifdef _WIN32
#include "glext.h"
#endif

#define max(a, b) ((a)>(b)?(a):(b))
#define min(a, b) ((a)<(b)?(a):(b))

#define ZOOM_LEVEL 4

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "rasterize.h"
#include "models.h"
#include "bmp_handler.h"

// Keyboard state
int keys[256];

// Frame buffer, frame counter, rendering start time
uint8_t* framebuffer;
int framecount;
float starttime;
float lasttime;

// List of models and projection matrix
#define NUM_MODELS 4
model_t models[NUM_MODELS];
imat4x4_t projection;
uint8_t* textures[20];
    
// Time in seconds to nanosecond accuracy. Too lazy to do it right on win32
#ifdef _WIN32
float qpcFreq = 0.0;
float nanotime() {
    LARGE_INTEGER li;

    if (qpcFreq == 0.0) {
        QueryPerformanceFrequency(&li);
        qpcFreq = (float)li.QuadPart;
    }
    
    QueryPerformanceCounter(&li);
    return (float)li.QuadPart / qpcFreq;
}
#else
float nanotime() {
    struct timespec curtime;
    clock_gettime(CLOCK_MONOTONIC, &curtime);
    return((float)curtime.tv_sec + 1.0e-9 * curtime.tv_nsec);
}
#endif

// Moeller-Trumbore ray triangle intersection, using fixed point vector math
// https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection
int32_t ray_tri_intersect(ivec3_t orig, ivec3_t dir, ivec3_t v0, ivec3_t v1, ivec3_t v2, int32_t* t) {
    ivec3_t v0v1 = ivec3sub(v1, v0);
    ivec3_t v0v2 = ivec3sub(v2, v0);
    ivec3_t pvec = ivec3cross(dir, v0v2);
    
    int32_t det = ivec3dot(v0v1, pvec);

    if(iabs(det) < FLOAT_FIXED(0.001)) {
        return 0;
    }

    ivec3_t tvec = ivec3sub(orig, v0);
    int32_t u = idiv(ivec3dot(tvec, pvec), det);
    if(u < 0 || u > INT_FIXED(1)) {
        return 0;
    }
    
    ivec3_t qvec = ivec3cross(tvec, v0v1);
    int32_t v = idiv(ivec3dot(dir, qvec), det);
    if(v < 0 || u + v > INT_FIXED(1)) {
        return 0;
    }

    *t = idiv(ivec3dot(v0v2, qvec), det);
    return 1;
} 

// Glut display function.
float xpos = 10;
float ypos = 10;
float zpos = 10;

float anglex = 0;
float angley = 0;

float xpower = 0;
float ypower = 0;
float speed = 1.0;

void display(void) {
    // Timing
    float thistime = nanotime();
    float elapsed = thistime - lasttime;
    lasttime = thistime;

    
    // Input handling
    float inpscale = elapsed * 100.0;
    if(keys['s']) {
       ypower += inpscale * 0.02 / 100.0;
    } 
    else if(keys['w']) {
        ypower -= inpscale * 0.02 / 100.0;
    }
    else {
        if (ypower < 0) {
            ypower += inpscale * 0.02 / 50.0;
        }

        if (ypower > 0) {
            ypower -= inpscale * 0.02 / 50.0;
        }
    }

    if(keys['a']) {
        xpower += inpscale * 0.02 / 150.0;
    } 
    else if(keys['d']) {
        xpower -= inpscale * 0.02 / 150.0;
    }
    else {
        if (xpower < 0) {
            xpower += inpscale * 0.02 / 150.0;
        }

        if (xpower > 0) {
            xpower -= inpscale * 0.02 / 150.0;
        }
    }
    
    xpower = xpower > 0.01 ? 0.01 : xpower;
    xpower = xpower < -0.01 ? -0.01 : xpower;

    ypower = ypower > 0.01 ? 0.01 : ypower;
    ypower = ypower < -0.01 ? -0.01 : ypower;
    
    anglex += inpscale * xpower;
    angley += inpscale * ypower;

    angley = angley > 1.0 ? 1.0 : angley;
    angley = angley < -1.0 ? -1.0 : angley;

    if(angley == 1.0 || angley == -1.0) {
        ypower = 0.0;
    }
    
    if(keys[' ']) {
        speed += 0.03 * inpscale;
    }
    else {
        speed -= 0.03 * inpscale;
    }
    speed = speed < 1.0 ? 1.0 : speed;
    speed = speed > 5.0 ? 5.0 : speed;
    
    // Recalculate projection
    projection = imat4x4perspective(FLOAT_FIXED(45), idiv(INT_FIXED(SCREEN_WIDTH), INT_FIXED(SCREEN_HEIGHT)), ZNEAR, ZFAR);
    
    // Movement
    xpos += speed * sin(anglex) * elapsed * 5.0;
    zpos += speed * cos(anglex) * elapsed * 5.0;
    ypos += speed * angley * elapsed * 5.0;

    // Update camera matrix
    ivec3_t eye = ivec3(FLOAT_FIXED(xpos), FLOAT_FIXED(ypos), FLOAT_FIXED(zpos));
    ivec3_t lookat = ivec3(FLOAT_FIXED(xpos + sin(anglex)), FLOAT_FIXED(ypos + angley), FLOAT_FIXED(zpos + cos(anglex)));
    ivec3_t up =  ivec3(FLOAT_FIXED(xpower * 70.0 * cos(anglex)), FLOAT_FIXED(1), FLOAT_FIXED(-xpower * 70.0 * sin(anglex)));
    
    imat4x4_t camera = imat4x4lookat(eye, lookat, up);

    // Draw models to screen buffer
    rasterize(framebuffer, models, NUM_MODELS, camera, projection, textures[13]);

    // Collide ship
    int32_t best_dot = INT_FIXED(2000);
    for(int m = 0; m < NUM_MODELS; m++) {
        // Inverse translate position
        ivec4_t pos_transformed = imat4x4transform(
            imat4x4affineinverse(models[m].modelview), 
            ivec4(FLOAT_FIXED(xpos), FLOAT_FIXED(ypos), FLOAT_FIXED(zpos), INT_FIXED(1))
        );
        
        // AABB collide position and vertices
        ivec3_t pos = ivec3(pos_transformed.x, pos_transformed.y, pos_transformed.z);
        for(int i = 0; i < models[m].num_vertices; i++) {
            ivec3_t diff = ivec3sub(pos, models[m].vertices[i]);
            int32_t dot = iabs(diff.x);
            dot = max(dot, iabs(diff.y));
            dot = max(dot, iabs(diff.z));
            best_dot = min(dot, best_dot);
        }
    }

    // Are we colliding?
    if(best_dot < FLOAT_FIXED(1.5)) {
        //framebuffer[0] = 0xF0; // TODO consequences
    }
    
    // Ray test
    int32_t t = 0;
    int32_t best_t = INT_FIXED(2000);
    ivec3_t dir_local = ivec3norm(ivec3sub(lookat, eye));
    int hit = 0;
    for(int m = 0; m < NUM_MODELS; m++) {
        ivec4_t pos_transformed = imat4x4transform(
            imat4x4affineinverse(models[m].modelview), 
            ivec4(FLOAT_FIXED(xpos), FLOAT_FIXED(ypos), FLOAT_FIXED(zpos), INT_FIXED(1))
        );
        ivec3_t pos = ivec3(pos_transformed.x, pos_transformed.y, pos_transformed.z);
        
        ivec4_t dir_transformed = imat4x4transform(
            imat4x4affineinverse(models[m].modelview), 
            ivec4(dir_local.x, dir_local.y, dir_local.z, INT_FIXED(0))
        );
        ivec3_t dir = ivec3norm(ivec3(dir_transformed.x, dir_transformed.y, dir_transformed.z));
        
        for(int i = 0; i < models[m].num_faces; i++) {
            ivec3_t v0 = models[m].vertices[models[m].faces[i].v[0]];
            ivec3_t v1 = models[m].vertices[models[m].faces[i].v[1]];
            ivec3_t v2 = models[m].vertices[models[m].faces[i].v[2]];
            if(ray_tri_intersect(pos, dir, v0, v1, v2, &t) != 0) {
                if(t > 0) {
                    best_t = min(t, best_t);
                    hit = 1;
                }
            }
        }
    }
    
    if(hit == 1) {
        imat4x4_t mvp = imat4x4mul(projection, camera);
        ivec4_t hitpos = ivec4(FLOAT_FIXED(xpos) + imul(t, dir_local.x), FLOAT_FIXED(ypos) + imul(t, dir_local.y), FLOAT_FIXED(zpos) + imul(t, dir_local.z), INT_FIXED(1));
        hitpos = imat4x4transform(mvp, hitpos);
        int32_t px = FIXED_INT_ROUND(VIEWPORT(hitpos.x, hitpos.w, SCREEN_WIDTH));
        int32_t py = FIXED_INT_ROUND(VIEWPORT(hitpos.y, hitpos.w, SCREEN_HEIGHT));
        framebuffer[px + SCREEN_WIDTH * py] = 0xF0; // TODO consequences
        
    }
    
    // Overlay
    for(int y = 0; y < SCREEN_HEIGHT; y++) {
        for(int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t pixel = textures[14][x + y * SCREEN_WIDTH];
            if(pixel != RGB332(0, 255, 0)) {
                framebuffer[x + y * SCREEN_WIDTH] = pixel;
            }
        }
    }

    // Buffer to screen
    glPixelZoom(ZOOM_LEVEL, ZOOM_LEVEL);
    glDrawPixels(SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE_3_3_2, framebuffer);
    glutSwapBuffers();

    // Calculate fps and print
    framecount++;
    if(framecount % 1000 == 0) {
        float fps = (float)framecount / (nanotime() - starttime);
        printf("FPS: %f\n", fps);
    }
}

// Resizable window (does not affect actual drawing)
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

// Basic glut event loop
#define MOVEINC 0.4f
void keyboard(unsigned char key, int x, int y) {
    keys[key] = 1;
    /*switch(key) {
        case 'w':
            xpos += MOVEINC * sin(anglex);
            zpos += MOVEINC * cos(anglex);
        break;

        case 's':
            xpos -= MOVEINC * sin(anglex);
            zpos -= MOVEINC * cos(anglex);
        break;

        case 'a':
            xpos += MOVEINC * sin(anglex + 3.14159 / 2.0);
            zpos += MOVEINC * cos(anglex + 3.14159 / 2.0);
            break;

        case 'd':
            xpos -= MOVEINC * sin(anglex + 3.14159 / 2.0);
            zpos -= MOVEINC * cos(anglex + 3.14159 / 2.0);
        break;

        case 'q':
            ypos -= MOVEINC;
        break;

        case 'e':
            ypos += MOVEINC;
        break;

        case 27:
            exit(0);
        break;

        default:
        break;
    }
    glutPostRedisplay();*/

    switch(key) {
    case 27:
        exit(0);
        break;

    default:
        break;
    }
}

void keyboardup(unsigned char key, int x, int y) {
    keys[key] = 0;
}

#define SENS 0.001f
void mouse(int x, int y) {
    int xc = SCREEN_WIDTH * ZOOM_LEVEL / 2;
    int yc = SCREEN_HEIGHT * ZOOM_LEVEL / 2;
    if(x != xc || y != yc) {
        anglex -= (x - xc) * SENS;

        angley -= (y - yc) * SENS;
        angley = angley > 3.0 ? 3.0 : angley;
        angley = angley < -3.0 ? -3.0 : angley;

        glutWarpPointer(xc, yc);
    }
    glutPostRedisplay();
}

// Texture loader
uint8_t* load_texture(const char* path) {
    int32_t r;
    int32_t g;
    int32_t b;

    bmp_info* bmp_file = bmp_open_read(path);
    uint8_t* texture = (uint8_t*)malloc(bmp_file->x_size * bmp_file->y_size * sizeof(uint8_t));

    for(int y = 0; y < bmp_file->y_size; y++) {
        for(int x = 0; x < bmp_file->x_size; x++) {
            bmp_read_pixel(bmp_file, &r, &g, &b);
            texture[y * bmp_file->x_size + x] = RGB332(r, g, b);
        }
    }
    bmp_close(bmp_file);

    return texture;
}

// Entry point
int main(int argc, char **argv) {

    // Don't lock to 60hz (nvidia specific)
#ifdef _WIN32
    _putenv( (char *) "__GL_SYNC_TO_VBLANK=0" );
#else    
    putenv( (char *) "__GL_SYNC_TO_VBLANK=0" );
#endif

    // Create model
    models[0] = get_model_tower();

    models[1] = get_model_cityscape3();
    models[1].modelview = imat4x4translate(ivec3(INT_FIXED(0), INT_FIXED(0), INT_FIXED(160)));

    models[2] = get_model_cityscape3();
    models[2].modelview = imat4x4translate(ivec3(INT_FIXED(138), INT_FIXED(0), INT_FIXED(-80)));

    models[3] = get_model_cityscape3();
    models[3].modelview = imat4x4translate(ivec3(INT_FIXED(-138), INT_FIXED(0), INT_FIXED(-80)));

    // Set up projection
    projection = imat4x4perspective(INT_FIXED(45), idiv(INT_FIXED(SCREEN_WIDTH), INT_FIXED(SCREEN_HEIGHT)), ZNEAR, ZFAR);

    // Screen buffer
    framebuffer = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint8_t));

    /*textures[0] = load_texture("baked.bmp");
    /or (int i = 1; i < 20; i++) {
        textures[i] = textures[0];
    }*/

    textures[0] = load_texture("tower.bmp");

    textures[1] = load_texture("windows.bmp");
    textures[2] = load_texture("roof_sharp.bmp");
    textures[3] = load_texture("roof_flat.bmp");
    textures[4] = load_texture("windows.bmp");

    textures[5] = load_texture("windows.bmp");
    textures[6] = load_texture("roof_sharp.bmp");
    textures[7] = load_texture("roof_flat.bmp");
    textures[8] = load_texture("windows.bmp");

    textures[9] = load_texture("windows.bmp");
    textures[10] = load_texture("roof_sharp.bmp");
    textures[11] = load_texture("roof_flat.bmp");
    textures[12] = load_texture("windows.bmp");

    textures[13] = load_texture("floor.bmp");
    textures[14] = load_texture("cockpit.bmp");

    int tex_offset = 0;
    int tex_max = 0;
    for(int m = 0; m < NUM_MODELS; m++) {
        for(int i = 0; i < models[m].num_faces; i++) {
            tex_max = max(models[m].faces[i].v[7], tex_max);
            models[m].faces[i].texture = textures[models[m].faces[i].v[7] + tex_offset];
        }
        tex_offset = tex_max + 2;
        tex_max = 0;
    }

    // Set up storage required
    prepare_geometry_storage(models, NUM_MODELS);
    
    // Create a window
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(SCREEN_WIDTH * ZOOM_LEVEL, SCREEN_HEIGHT * ZOOM_LEVEL);
    glutCreateWindow("CYBER DEFENSE 2200");
    glutReshapeFunc(reshape);
    glutIgnoreKeyRepeat (1);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardup);
    //glutPassiveMotionFunc(mouse);
    glutSetCursor(GLUT_CURSOR_NONE); 
    glutIdleFunc(display);
    
    // Run render loop
    starttime = nanotime();
    lasttime = starttime;
    glutMainLoop();

return 0;
}
