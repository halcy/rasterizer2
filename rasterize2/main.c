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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "rasterize.h"
#include "cityscape.h"

// Frame buffer, frame counter, rendering start time
uint8_t* framebuffer;
int framecount;
float starttime;

// List of models and projection matrix
#define NUM_MODELS 1
model_t models[NUM_MODELS];
imat4x4_t projection;

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
void display(void) {
    // Clear screen
    memset(framebuffer, 0xFF, SCREEN_WIDTH * SCREEN_HEIGHT);
    
    // Update modelview matrices
    int32_t time_val = FLOAT_FIXED(sin(nanotime() * 0.1f) * 0.3f);
    models[0].modelview = imat4x4translate(ivec3(INT_FIXED(0), INT_FIXED(-12), INT_FIXED(-20)));
    models[0].modelview = imat4x4mul(models[0].modelview, imat4x4rotatey(time_val));

    // Draw model to screen buffer
    rasterize(framebuffer, models, NUM_MODELS, projection);
    
    // Buffer to screen
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
void keyboard(unsigned char key, int x, int y) {
    switch(key) {
        case 27:
            exit(0);
        break;

        default:
        break;
    }
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
    models[0] = get_model_cityscape();
    
    // Set up projection
    projection = imat4x4perspective(INT_FIXED(45), idiv(INT_FIXED(SCREEN_WIDTH), INT_FIXED(SCREEN_HEIGHT)), ZNEAR, ZFAR);

    // Screen buffer
    framebuffer = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint8_t));

    // Create a window
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);
    glutCreateWindow("rasterizer");
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(display);
    
    // Run render loop
    starttime = nanotime();
    glutMainLoop();

return 0;
}
