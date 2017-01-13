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
#define MAX_CHARGE (FLOAT_FIXED(0.1))

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "rasterize.h"
#include "models.h"
#include "bmp_handler.h"

#include "text/font8x8_basic.h"

#include <bass.h>

uint8_t cyber_cols[8] = {
    RGB332(255, 71,  254),
    RGB332(255, 116, 254),
    RGB332(255, 155, 254),
    RGB332(255, 190, 254),
    RGB332(255, 255, 255),
    RGB332(190, 255, 253),
    RGB332(156, 255, 253),
    RGB332(0  , 255, 253) 
};

// The Enemy
typedef struct enemy {
    ivec3_t pos;
    ivec3_t goal;
    ivec3_t dir;
    int32_t charge;
    int32_t charging;
    int32_t model;
    int32_t active;
    int32_t scale;
} enemy;

#define ENEMY_MAX 16
enemy enemies[ENEMY_MAX];
int32_t enemy_count;
int32_t enemies_alive;
int32_t player_charge;
int32_t player_health;
int32_t player_shake;
int32_t stage_enemies_max;

int32_t wave_show;
int32_t wave_nb;

float xpos;
float ypos;
float zpos;

float anglex;
float angley;

float xpower;
float ypower;
float speed;

int32_t paused;

HSTREAM music;
HSTREAM sounds[10];

// Keyboard state
int keys[256];

// Frame buffer, frame counter, rendering start time
uint8_t* framebuffer;
int framecount;
float starttime;
float lasttime;
float alltime;

// List of models and projection matrix
#define NUM_MODELS 20
model_t models[NUM_MODELS];
imat4x4_t projection;
uint8_t* textures[64];
uint8_t* texture_floor;
uint8_t* texture_overlay[3];
uint8_t* texture_shot;
uint8_t* texture_menuimages[10];

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

// Draw a single character of text at a given position
void draw_char(char character, int px, int py, int cyber) {
    char* bitmap = font8x8_basic[character];
    uint8_t col = 0xFF;
    for(int x = 0; x < 8; x++) {
        for(int y = 0; y < 8; y++) {
            if(cyber == 1) {
                col = cyber_cols[y];
            }
            if(bitmap[y] & 1 << x) {
                framebuffer[px + x + (SCREEN_HEIGHT - (py + y)) * SCREEN_WIDTH] = col;
            }
            
        }
    }
}

// Draw a string. No line breaking. _ toggles CYBER MODE
void draw_string(char* string, int px, int py) {
    int cybermode = 0;
    int pos = 0;
    while(string[pos] != 0) {
        if(string[pos] == '_') {
            cybermode = cybermode == 1 ? 0 : 1;
            pos++;            
            continue;
        }
        draw_char(string[pos], px, py, cybermode);
        px += 8;
        pos++;
    }
}

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

// Traces a ray against geometry
int raytrace(ivec3_t origin_local, ivec3_t dir_local, ivec3_t* hit_pos, int32_t* hit_model, int32_t ignore_model) {
    int32_t t = 0;
    int32_t best_t = INT_FIXED(2000);
    int32_t hit = 0;
    if(hit_model != 0) {
        *hit_model = -1;
    }
    dir_local = ivec3norm(dir_local);
    for(int m = 0; m < NUM_MODELS; m++) {
        if(m == ignore_model) {
            continue;
        }
        if(models[m].draw == 0) {
            continue;
        }

        ivec4_t pos_transformed = imat4x4transform(
            imat4x4affineinverse(models[m].modelview), 
            ivec4(origin_local.x, origin_local.y, origin_local.z, INT_FIXED(1))
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
                    if (t < best_t) {
                        best_t = t;
                        if(hit_model != 0) {
                            *hit_model = m;
                        }
                    }
                    hit = 1;
                }
            }
        }
    }

    if(hit == 1 && hit_pos != 0) {
        *hit_pos = ivec3(
            origin_local.x + imul(t, dir_local.x), 
            origin_local.y + imul(t, dir_local.y), 
            origin_local.z + imul(t, dir_local.z)
        );
    }

    return hit;
}

// Checks if a point is in the arena cylinder
int point_in_arena(ivec3_t point) {
    if(point.y < 0 || point.y > INT_FIXED(200)) {
        return 0;
    }

    int32_t dist = FIXED_INT_ROUND(point.x) * FIXED_INT_ROUND(point.x) + FIXED_INT_ROUND(point.z) * FIXED_INT_ROUND(point.z);
    if(dist > 256 * 256) {
        return 0;
    }

    return 1;
}

// Returns a random point in the arena cylinder
ivec3_t random_arena_point() {
    ivec3_t point = ivec3(-1, -1, -1);
    while(!point_in_arena(point)) {
        int32_t x = idiv(INT_FIXED((rand() % (512 * 8)) - 256 * 8), INT_FIXED(8));
        int32_t y = idiv(INT_FIXED(rand() % (200 * 8)), INT_FIXED(8));
        int32_t z = idiv(INT_FIXED((rand() % (512 * 8)) - 256 * 8), INT_FIXED(8));
        point = ivec3(x, y, z);
    }
    return point;
}

// Returns a random point that can be reached from another random point without crashing
ivec3_t random_reachable_point(ivec3_t origin, int32_t ignore_model) {
    ivec3_t point = ivec3(-1, -1, -1);
    int32_t hit = 1;
    ivec3_t hit_pos;
    int32_t hit_model;
    while(hit == 1) {
        point = random_arena_point();

        ivec3_t diff = ivec3sub(origin, point);
        if(ivec3dot(diff, diff) > FLOAT_FIXED(0.1)) {
            hit = raytrace(origin, ivec3sub(point, origin), &hit_pos, &hit_model, ignore_model);
        }
    }
    return point;
}

// Draw a line towards an enemy
void enemy_line(ivec3_t enemy, ivec3_t pos, imat4x4_t mvp, int32_t len, uint8_t* framebuffer, uint8_t color) {
    ivec3_t enemy_dir = ivec3sub(enemy, pos);
    ivec4_t enemy_dir_transformed = imat4x4transform(mvp, ivec4(enemy_dir.x, enemy_dir.y, enemy_dir.z, INT_FIXED(0)));
    ivec3_t dir_norm = ivec3norm(ivec3(
        idiv(enemy_dir_transformed.x, enemy_dir_transformed.w),
        idiv(enemy_dir_transformed.y, enemy_dir_transformed.w),
        FLOAT_FIXED(0.0)
    ));

    // too lazy for bresenham
    int32_t aspect = idiv(INT_FIXED(SCREEN_WIDTH), INT_FIXED(SCREEN_HEIGHT));
    for(int i = FLOAT_FIXED(0.04); i < len + FLOAT_FIXED(0.04); i += FLOAT_FIXED(0.005)) {
        int32_t px = FIXED_INT_ROUND(VIEWPORT_NO_PERSPECTIVE(imul(dir_norm.x, i), SCREEN_WIDTH));
        int32_t py = FIXED_INT_ROUND(VIEWPORT_NO_PERSPECTIVE(imul(imul(dir_norm.y, i), aspect), SCREEN_HEIGHT));
        framebuffer[px + py * SCREEN_WIDTH] = color;
    }

    // fillup end
    for(int i = FLOAT_FIXED(-0.02); i < FLOAT_FIXED(0.02); i += FLOAT_FIXED(0.005)) {
        int32_t px = imul(dir_norm.x, MAX_CHARGE + FLOAT_FIXED(0.04));
        int32_t py = imul(dir_norm.y, MAX_CHARGE + FLOAT_FIXED(0.04));

        ivec3_t dir_norm_ortho = ivec3(dir_norm.y, -dir_norm.x, 0.0);
        px = FIXED_INT_ROUND(VIEWPORT_NO_PERSPECTIVE(px + imul(dir_norm_ortho.x, i), SCREEN_WIDTH));
        py = FIXED_INT_ROUND(VIEWPORT_NO_PERSPECTIVE(imul(py + imul(dir_norm_ortho.y, i), aspect), SCREEN_HEIGHT));
        framebuffer[px + py * SCREEN_WIDTH] = color;
    }
}

// Draws an overlay on the screen
void blit_to_screen(uint8_t* blit_texture) {
    for(int y = 0; y < SCREEN_HEIGHT; y++) {
        for(int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t pixel = blit_texture[x + y * SCREEN_WIDTH];
            if(pixel != RGB332(0, 255, 0)) {
                framebuffer[x + y * SCREEN_WIDTH] = pixel;
            }
        }
    }
}

// Sends out a wave of enemies
void start_wave(int count) {
    // Set wave display active
    wave_show = INT_FIXED(2);
    wave_nb += 1;
    
    // Set all enemies inactive
    for(int i = 0; i < ENEMY_MAX; i++) {
        enemies[i].active = 0;
    }
    
    // Set up enemies
    enemy_count = count;
    enemies_alive = enemy_count;
    for(int i = 0; i < enemy_count; i++) {
        ivec3_t start_point = random_arena_point();
        start_point.y = INT_FIXED(192); // Upper 8 units are always empty

        enemies[i].pos = start_point;
        enemies[i].goal = random_reachable_point(enemies[i].pos, enemies[i].model);
        enemies[i].dir = ivec3norm(ivec3sub(enemies[i].goal, enemies[i].pos));
        enemies[i].scale = FLOAT_FIXED(0.1);
        enemies[i].active = 1;
    }
}

// Resets game state
void start_game() {
    // Game state
    player_charge = 0;
    player_health = 3;
    player_shake = 0;
    
    xpos = 10;
    ypos = 10;
    zpos = 10;

    anglex = 0;
    angley = 0;

    xpower = 0;
    ypower = 0;
    speed = 1.0;
    
    wave_nb = 0;
    start_wave(2);
}

// Update function
void main_loop(void) {
    // Restart music
    if(!BASS_ChannelIsActive(music)) {
        BASS_ChannelPlay(music, 1);
    }

    // Timing
    float thistime = nanotime();
    float elapsed = thistime - lasttime;
    lasttime = thistime;

    if(paused == 1) {
        elapsed = 0;
    }
    alltime += elapsed;

    // Enemies
    for(int i = 0; i < ENEMY_MAX; i++) {
        // Is it alive?
        if(enemies[i].active == 0) {
            if(enemies[i].scale > FLOAT_FIXED(0.1)) {
                models[enemies[i].model].modelview = imat4x4mul(
                    imat4x4mul(
                        imat4x4translate(enemies[i].pos),
                        imat4x4rotatey(FLOAT_FIXED(alltime))
                    ),
                    imat4x4scale(enemies[i].scale)
                );
                enemies[i].scale -= FLOAT_FIXED(elapsed * 2.0);
            }
            else {
                models[enemies[i].model].draw = 0;
            }
            continue;
        }
        models[enemies[i].model].draw = 1;
        
        // Make it bigger
        if(enemies[i].scale < INT_FIXED(1)) {
            enemies[i].scale += FLOAT_FIXED(elapsed);
        }
        else {
            enemies[i].scale = INT_FIXED(1);
        }
        
        // Move
        enemies[i].pos = ivec3add(enemies[i].pos, ivec3mul(enemies[i].dir, FLOAT_FIXED(elapsed * 20.0)));
        ivec3_t diff = ivec3sub(enemies[i].pos, enemies[i].goal);

        // Change direction
        if(ivec3dot(diff, enemies[i].dir) > FLOAT_FIXED(0.0)) {
            // Potentially change mode
            if(!enemies[i].charging && ((rand() % 100) > 30)) {
                enemies[i].charging = 1;
            }
            else {
                enemies[i].goal = random_reachable_point(enemies[i].pos, enemies[i].model);
                enemies[i].dir = ivec3norm(ivec3sub(enemies[i].goal, enemies[i].pos));
            }
        }

        // Charge
        if(enemies[i].charging) {
            enemies[i].charge += FLOAT_FIXED(elapsed * 0.016);
        }

        // Detarget when obstructed
        if(enemies[i].charging) {
            ivec3_t player_pos = ivec3(FLOAT_FIXED(xpos), FLOAT_FIXED(ypos), FLOAT_FIXED(zpos));
            ivec3_t dir = ivec3norm(ivec3sub(enemies[i].pos, player_pos));
            int32_t model_hit = -1;
            int32_t hit = raytrace(player_pos, dir, 0, &model_hit, -1);
            if(model_hit != enemies[i].model) {
                enemies[i].charge = 0;
                enemies[i].charging = 0;
            }
        }

        // Charged? Hit player.
        if(enemies[i].charge > MAX_CHARGE) {
            enemies[i].charge = 0;
            enemies[i].charging = 0;
            player_health -= 1;
            BASS_ChannelPlay(sounds[1], 1);
            player_shake = INT_FIXED(5);
        }

        // Update models modelview
        models[enemies[i].model].modelview = imat4x4mul(
            imat4x4mul(
                imat4x4translate(enemies[i].pos),
                imat4x4rotatey(FLOAT_FIXED(alltime))
            ),
            imat4x4scale(enemies[i].scale)
        );
    }
    
    // Player shot charge
    if(player_charge <= FLOAT_FIXED(0.1)) {
        player_charge += FLOAT_FIXED(elapsed * 0.65);
    }

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
    
    // Shooting?
    int32_t player_shot = 0;
    if(keys['m'] && player_charge >= FLOAT_FIXED(0.1)) {
        player_charge = 0;
        player_shot = 1;
        BASS_ChannelPlay(sounds[0], 1);
    }

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
    rasterize(framebuffer, models, NUM_MODELS, camera, projection, texture_floor);

    // Collide ship TODO this is bad
    int32_t best_dot = INT_FIXED(2000);
    for(int m = 0; m < NUM_MODELS; m++) {
        if(models[m].draw == 0) {
            continue;
        }
        
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
    if(best_dot < FLOAT_FIXED(1.5) || !point_in_arena(eye)) {
        player_health -= 1;
        xpos = 50;
        ypos = 50;
        zpos = 50;

        anglex = 0;
        angley = 0;

        xpower = 0;
        ypower = 0;
        speed = 1.0;
        
        BASS_ChannelPlay(sounds[1], 1);
        
        player_shake = INT_FIXED(5);
    }
   
    // Trace shots
    ivec3_t hit_pos;
    int32_t hit_model;
    int hit = raytrace(ivec3(FLOAT_FIXED(xpos), FLOAT_FIXED(ypos), FLOAT_FIXED(zpos)), ivec3sub(lookat, eye), &hit_pos, &hit_model, -1);
    if(hit == 1) {
        imat4x4_t mvp = imat4x4mul(projection, camera);
        ivec4_t hit_pos_tranformed = imat4x4transform(mvp, ivec4(hit_pos.x, hit_pos.y, hit_pos.z, INT_FIXED(1)));
        int32_t px = FIXED_INT_ROUND(VIEWPORT(hit_pos_tranformed.x, hit_pos_tranformed.w, SCREEN_WIDTH));
        int32_t py = FIXED_INT_ROUND(VIEWPORT(hit_pos_tranformed.y, hit_pos_tranformed.w, SCREEN_HEIGHT));

        int32_t hit_enemy = -1;
        for(int i = 0; i < enemy_count; i++) {
            if(hit_model == enemies[i].model) {
                hit_enemy = i;
            }
        }

        if(hit_enemy != -1) {
            framebuffer[px + SCREEN_WIDTH * py] = 0xF0;
        
            // Shooting?
            if(player_shot && enemies[hit_enemy].active == 1) {
                enemies[hit_enemy].active = 0;
                enemies_alive--;
                BASS_ChannelPlay(sounds[1], 1);
            }
        }
    }
    
    // Enemy lines
    for(int i = 0; i < enemy_count; i++) {
        if(enemies[i].charging && enemies[i].active) {
            imat4x4_t mvp = imat4x4mul(projection, camera);
            enemy_line(enemies[i].pos, eye, mvp, enemies[i].charge, framebuffer, RGB332(255, 0, 46));
        }
    }

    // Shot draw
    if(player_charge < FLOAT_FIXED(0.02)) {
        blit_to_screen(texture_shot);
    }

    // Overlay
    int cockpit_img = player_health - 1;
    cockpit_img = cockpit_img < 0 ? 0 : cockpit_img;
    for(int y = 0; y < SCREEN_HEIGHT; y++) {
        for(int x = 0; x < SCREEN_WIDTH; x++) {
            int32_t ssinc = 0;
            int32_t invshake = INT_FIXED(5) - player_shake;
            if(invshake != 0) {
                ssinc = idiv(isin(invshake), invshake);
            }
            
            ssinc = ssinc > INT_FIXED(1) ? INT_FIXED(1) : ssinc;
            
            int px = x + FIXED_INT_ROUND(imul(ssinc, INT_FIXED(10)));
            px = px < 0 ? 0 : px;
            px = px >= SCREEN_WIDTH ? SCREEN_WIDTH - 1 : px;
            
            uint8_t pixel = texture_overlay[cockpit_img][px + y * SCREEN_WIDTH];
            if(pixel != RGB332(0, 255, 0)) {
                framebuffer[x + y * SCREEN_WIDTH] = pixel;
            }
        }
    }
    
    // Display "wave n" text
    if(wave_show > 0) {
        char wavetext[255];
        sprintf(wavetext, "_Wave %d_", wave_nb);
        draw_string(wavetext, 130, 40);
        wave_show -= FLOAT_FIXED(elapsed);        
    }
    
    // Unshake
    if(player_shake > 0) {
        player_shake -= FLOAT_FIXED(elapsed * 6.0);
    }
    else {
        player_shake = 0;
    }
    
    // "Paused"
    if(paused == 1) {
        blit_to_screen(texture_menuimages[0]);
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

    // Win / lose check
    if(player_health == 0) {
        start_game();
        paused = 1;
        printf("You lose\n");
    }
    
    if(enemies_alive == 0) {
        if(enemy_count == stage_enemies_max) {
            paused = 1;
            start_game();
            printf("You win!\n");
        }
        else {
            start_wave(enemy_count * 2);
        }
    }
}

// Resizable window (does not affect actual drawing)
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

// Glut input: key down
#define MOVEINC 0.4f
void keyboard(unsigned char key, int x, int y) {
    keys[key] = 1;

    switch(key) {
    case 27:
        if(paused == 1) {
            paused = 0;
        }
        else {
            paused = 1;
        }
        break;

    default:
        break;
    }
}

// Glut input: key up
void keyboardup(unsigned char key, int x, int y) {
    keys[key] = 0;
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

void load_level_city() {
    // Maximum enemies for this stage
    stage_enemies_max = 4;
    
    // Set all models to no draw
    for(int i = 0; i < NUM_MODELS; i++) {
        models[i].draw = 0;
    }
    
    // Create model
    models[0] = get_model_tower();
    models[0].draw = 1;

    models[1] = get_model_cityscape3();
    models[1].modelview = imat4x4translate(ivec3(INT_FIXED(0), INT_FIXED(0), INT_FIXED(160)));
    models[1].draw = 1;

    models[2] = get_model_cityscape3();
    models[2].modelview = imat4x4translate(ivec3(INT_FIXED(138), INT_FIXED(0), INT_FIXED(-80)));
    models[2].draw = 1;

    models[3] = get_model_cityscape3();
    models[3].modelview = imat4x4translate(ivec3(INT_FIXED(-138), INT_FIXED(0), INT_FIXED(-80)));
    models[3].draw = 1;

    for(int i = 0; i < ENEMY_MAX; i++) {
        models[4  + i] = get_model_enemy();
        models[4 + i].draw = 1;
        enemies[i].model = i + 4;
    }
    
     // Set up projection
    projection = imat4x4perspective(INT_FIXED(45), idiv(INT_FIXED(SCREEN_WIDTH), INT_FIXED(SCREEN_HEIGHT)), ZNEAR, ZFAR);

    // Screen buffer
    framebuffer = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint8_t));

    textures[0] = load_texture("data/tower.bmp");

    textures[1] = load_texture("data/windows.bmp");
    textures[2] = load_texture("data/roof_sharp.bmp");
    textures[3] = load_texture("data/roof_flat.bmp");
    textures[4] = load_texture("data/windows.bmp");

    textures[5] = load_texture("data/windows.bmp");
    textures[6] = load_texture("data/roof_sharp.bmp");
    textures[7] = load_texture("data/roof_flat.bmp");
    textures[8] = load_texture("data/windows.bmp");

    textures[9] = load_texture("data/windows.bmp");
    textures[10] = load_texture("data/roof_sharp.bmp");
    textures[11] = load_texture("data/roof_flat.bmp");
    textures[12] = load_texture("data/windows.bmp");

    for(int i = 0; i < ENEMY_MAX; i++) {
        textures[13 + i] = load_texture("data/enemy.bmp");
    }

    texture_floor = load_texture("data/floor.bmp");
    texture_overlay[0] = load_texture("data/cockpit_low.bmp");
    texture_overlay[1] = load_texture("data/cockpit_med.bmp");
    texture_overlay[2] = load_texture("data/cockpit.bmp");
    texture_shot = load_texture("data/shot.bmp");
    texture_menuimages[0] = load_texture("data/pause.bmp");

    int tex_offset = 0;
    for(int m = 0; m < NUM_MODELS; m++) {
        int tex_max = 0;
        for(int i = 0; i < models[m].num_faces; i++) {
            tex_max = max(models[m].faces[i].v[7], tex_max);
            models[m].faces[i].texture = textures[models[m].faces[i].v[7] + tex_offset];
        }
        // printf("%d -> %d\n", m, tex_offset);
        tex_offset += tex_max + 1;
        tex_max = 0;
    }

    // Set up storage required
    prepare_geometry_storage(models, NUM_MODELS);
}

// Entry point
int main(int argc, char **argv) {

    // Don't lock to 60hz (nvidia specific)
#ifdef _WIN32
    _putenv( (char *) "__GL_SYNC_TO_VBLANK=0" );
#else    
    putenv( (char *) "__GL_SYNC_TO_VBLANK=0" );
#endif

    // Create a window
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(SCREEN_WIDTH * ZOOM_LEVEL, SCREEN_HEIGHT * ZOOM_LEVEL);
    glutCreateWindow("CYBER DEFENSE 2200");
    glutReshapeFunc(reshape);
    glutIgnoreKeyRepeat (1);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardup);
    glutSetCursor(GLUT_CURSOR_NONE); 
    glutIdleFunc(main_loop);
    
    // Sound
    BASS_Init(-1, 44100, 0, 0, 0);
    BASS_Start();

    // Music!
    music = BASS_StreamCreateFile(0, "data/cyber.ogg", 0, 0, BASS_STREAM_PRESCAN);
    BASS_ChannelPlay(music, 0);

    sounds[0] = BASS_StreamCreateFile(0, "data/fwup.ogg", 0, 0, BASS_STREAM_PRESCAN);
    sounds[1] = BASS_StreamCreateFile(0, "data/bwoom.ogg", 0, 0, BASS_STREAM_PRESCAN);

    // Set up game
    load_level_city();
    start_game();

    // Run render loop
    starttime = nanotime();
    lasttime = starttime;
    glutMainLoop();

return 0;
}
