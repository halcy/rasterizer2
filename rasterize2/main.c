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

#define ZOOM_LEVEL 4

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "rasterize.h"
#include "models.h"
#include "bmp_handler.h"

// Frame buffer, frame counter, rendering start time
uint8_t* framebuffer;
int framecount;
float starttime;

// List of models and projection matrix
#define NUM_MODELS 1
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

// Glut display function.
float xpos = 1;
float ypos = 0;
float zpos = 1;
float anglex = 0;
float angley = 0;
void display(void) {
    // Update modelview matrices
    int32_t time_val = FLOAT_FIXED(sin(nanotime() * 0.1f) * 0.3f);

    ivec3_t eye = ivec3(FLOAT_FIXED(xpos), FLOAT_FIXED(ypos + 1.0), FLOAT_FIXED(zpos));
    ivec3_t lookat = ivec3(FLOAT_FIXED(xpos + sin(anglex)), FLOAT_FIXED(ypos + 1.0 + angley), FLOAT_FIXED(zpos + cos(anglex)));
    ivec3_t up =  ivec3(FLOAT_FIXED(0), FLOAT_FIXED(1), FLOAT_FIXED(0));
    
    imat4x4_t camera = imat4x4lookat(eye, lookat, up);

    // Draw model to screen buffer
    rasterize(framebuffer, models, NUM_MODELS, camera, projection, textures[4]);
    
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
    switch(key) {
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
    glutPostRedisplay();
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
    models[0] = get_model_cityscape3();
    
    // Set up projection
    projection = imat4x4perspective(INT_FIXED(45), idiv(INT_FIXED(SCREEN_WIDTH), INT_FIXED(SCREEN_HEIGHT)), ZNEAR, ZFAR);

    // Screen buffer
    framebuffer = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint8_t));

    /*textures[0] = load_texture("baked.bmp");
    /or (int i = 1; i < 20; i++) {
        textures[i] = textures[0];
    }*/
    textures[0] = load_texture("windows.bmp");
    textures[1] = load_texture("roof_sharp.bmp");
    textures[2] = load_texture("roof_flat.bmp");
    textures[3] = load_texture("windows.bmp");
    textures[4] = load_texture("floor.bmp");
    
    for (int i = 0; i < models[0].num_faces; i++) {
        models[0].faces[i].texture = textures[models[0].faces[i].v[7]];
    }

    // Set up storage required
    prepare_geometry_storage(models, NUM_MODELS);
    
    // Create a window
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(SCREEN_WIDTH * ZOOM_LEVEL, SCREEN_HEIGHT * ZOOM_LEVEL);
    glutCreateWindow("rasterizer");
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(mouse);
    glutSetCursor(GLUT_CURSOR_NONE); 
    //glutIdleFunc(display);
    
    // Run render loop
    starttime = nanotime();
    glutMainLoop();

return 0;
}
