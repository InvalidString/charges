#include <assert.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../snippets/allocator.h"

typedef float ℝ2 __attribute__((__vector_size__(sizeof(float[2]))));

Vector2 toV(ℝ2 v){ return (Vector2){v[0], v[1]}; };
float magSqr(ℝ2 v){ return v[0]*v[0] + v[1]*v[1]; };
float mag(ℝ2 v){ return sqrt(magSqr(v)); };

#define BLUE_CHARGE -1
#define RED_CHARGE 4

#define isRed(i) (i%2)

typedef struct{
    Camera2D cam;

    int dragging;

    ℝ2 ps[0x40];
    ℝ2 vs[0x40];

    float dt;
} State;
State *state;

bool should_log;

State* before_reload(size_t *size){


    *size = sizeof(State);
    return state;
}
void after_reload(State* old_state, size_t old_size){
    size_t size = sizeof(State);
    state = old_state;
    if(old_size < size){
        state = realloc(state, size);
        if(!state && size) puts("buy more ram"), exit(1);
        memset((char*)state + old_size, 0, size - old_size);
    }


}




void draw_grid(){

    int count = 100;

    for (int i = -count; i < count; i++) {
        Color color;
        if(i==0){
            color = RED;
        }else{
            color = GRAY;
        }
        {
            Vector2 start = {i, -count};
            Vector2 end = {i,count};
            DrawLineV(start, end, color);
        }
        {
            Vector2 start = {-count, i};
            Vector2 end = {count, i};
            DrawLineV(start, end, color);
        }
    }
}

void update_cam(Camera2D *cam){
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)){
        Vector2 delta = GetMouseDelta();
        delta = Vector2Scale(delta, -1.0f/cam->zoom);

        cam->target = Vector2Add(cam->target, delta);
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0){
        Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), *cam);

        cam->offset = GetMousePosition();

        cam->target = mouseWorldPos;

        const float zoomIncrement = 5;

        cam->zoom += (wheel*zoomIncrement);
        if (cam->zoom < 10) cam->zoom = 10;
    }
}

typedef float ℝ;



// Tablelau
// cs left bs bottom


#if 0
// Heun
#define S 2
ℝ a[S][S] = {
         {},
         {1},
};
ℝ b[S] = {0.5, 0.5};
ℝ c[S] = {0,   1};
#endif

#if 0
// RK4
#define S 4
ℝ a[S][S] = {
         {},
         {0.5},
         {0,  0.5},
         {0,  0,  1},
};
ℝ b[S] = {1/6.0, 2/6.0, 2/6.0, 1/6.0};
ℝ c[S] = {0,     0.5,   0.5,   1};
#endif


#if 1
// doPri45
#define S 7
#define Order 5
ℝ a[S][S] = {
         {                                                                             },
         {1/5.0                                                                        },
         {3/40.0,       9/40.0,                                                        },
         {44/45.0,      -56/15.0,      32/9.0,                                         },
         {19372/6561.0, -25360/2187.0, 64448/6561.0, -212/729.0,                       },
         {9017/3168.0,  -355/33.0,     46732/5247.0, 49/176.0,  -5103/18656.0,         },
         {35/384.0,     0,             500/1113.0,   125/192.0, -2187/6784.0, 11/84.0, },
};
ℝ b[S] = {35/384.0,     0,             500/1113.0,   125/192.0, -2187/6784.0, 11/84.0, 0};
ℝ b2[S]= {5179/57600.0, 0,             7571/16695.0, 393/640.0, -92097/339200.0, 187/2100.0, 1/40.0};
ℝ c[S] = {0,            1/5.0,       3/10.0,       4/5.0,     8/9.0,        1,       1};
#endif



void derivative(ℝ2 p, ℝ2 v, ℝ2 *dp, ℝ2 *dv, int idx, int n, ℝ2 ps[n], ℝ2 vs[n]){
    ℝ2 f = {};
    for (int i = 0; i < n; i++) {
        if(idx == i) continue;
        ℝ2 d = p - ps[i];
        float m = mag(d);
        d /= m;

        // collision
        float r = 0.1 * 1.5 * 2;
        if(m < r){
            f += 100.0f * d / (m*m) * ((m-r)*(m-r));
        }


        //coulomb
        float ch1 = isRed(idx) ? RED_CHARGE : BLUE_CHARGE;
        float ch2 = isRed(i)   ? RED_CHARGE : BLUE_CHARGE;

        f += 0.4f / (m*m) * ch1 * ch2 * d;
    }
    // friction
    f += -v*0.2f;
    *dp = v;
    *dv = f;
}

ℝ2 k[S][arrlen(state->ps)*2];
ℝ2 tmp[arrlen(state->ps)*2];
ℝ2 yBackup[arrlen(state->ps)*2];
ℝ2   y[arrlen(state->ps)*2];

void update(void){
    uint32_t screen_width = GetScreenWidth();
    uint32_t screen_height = GetScreenHeight();

    Camera2D *cam = &state->cam;
    update_cam(cam);

    BeginDrawing();
    ClearBackground(WHITE);

    BeginMode2D(state->cam);
    draw_grid();

    ℝ2 mp = (ℝ2){
        GetScreenToWorld2D(GetMousePosition(), *cam).x,
        GetScreenToWorld2D(GetMousePosition(), *cam).y,
    };
    if(state->dragging == -1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
        for (isz i = 0; i < arrlen(state->ps); i++) {
            if(mag(mp - state->ps[i]) < 0.2){
                state->dragging = i;
            }
        }
    }
    if(state->dragging != -1) {
        state->ps[state->dragging] = mp;
        state->vs[state->dragging] = (ℝ2){};
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) state->dragging = -1;

    float ε = 0.02;
    float αMin = 0.5; // 0.2 .. 0.5
    float αMax = 2;   // 1.5 .. 2.0
    float β = 0.9;    // 0.8 .. 0.95
    float dtMax = 1/30.f;
    float dtMin = 1/6000.f;

    float dt = state->dt;
    for (float dtAcc = 0; dtAcc < 1/60.0f; ) {
        memcpy(y, state->ps, sizeof(state->ps));
        memcpy(y + arrlen(state->ps), state->vs, sizeof(state->vs));
        //if(_k >= 2452) should_log = 1;

        //for (isz i = 0; i < arrlen(y)*2; i++) {
        //    printf("%.2f ", ((float*)y)[i]);
        //}
        //puts("");

        for (isz i = 0; i < S; i++) {
            memcpy(tmp, y, sizeof(tmp));
            for (isz j = 0; j < i; j++) for (isz l = 0; l < arrlen(tmp); l++)
                tmp[l] += dt * a[i][j] * k[j][l];

            ℝ2 *ps = tmp;
            ℝ2 *vs = tmp + arrlen(state->ps);
            for (isz j = 0; j < arrlen(state->ps); j++) derivative(
                ps[j],
                vs[j],
                &k[i][j],
                &k[i][j + arrlen(state->ps)],
                j,
                arrlen(state->ps),
                ps,
                vs
            );
        }
        memcpy(yBackup, y, sizeof(yBackup));
        memcpy(tmp, y, sizeof(tmp));
        for (isz i = 0; i < S; i++) for (isz j = 0; j < arrlen(y); j++)
            y[j] += dt * b[i]*k[i][j];

        for (isz i = 0; i < S; i++) for (isz j = 0; j < arrlen(y); j++)
            tmp[j] += dt * b2[i]*k[i][j];

        float sqrErr = 0;
        for (isz i = 0; i < arrlen(y); i++) sqrErr += magSqr(y[i]-tmp[i]);
        //printf("%.6f\n", sqrt(sqrErr));

        float q = sqrtf(sqrErr)/(dt * ε);
        float α = max(αMin, powf(q, -1./Order));
        α = min(αMax, α);

        float dtNew = max(dtMin, β*α*dt);
        dtNew = min(dtNew, dtMax);

        if(q > 1 && dtNew > dtMin){
            dt = dtNew;
            memcpy(y, yBackup, sizeof(y));
            continue;
        }


        memcpy(state->ps, y, sizeof(state->ps));
        memcpy(state->vs, y + arrlen(state->ps), sizeof(state->vs));
        should_log = 0;

        float borderR = 2.5;

        for (isz i = 0; i < arrlen(state->ps); i++) {
            for (int j = 0; j < 2; j++) {
                if(state->ps[i][j] > borderR){
                    //state->ps[i][j] = 2-(state->ps[i][j] - 2);
                    if(state->vs[i][j] > 0) state->vs[i][j] *= -0.9;
                    state->vs[i][j] -= 0.04;
                }
                if(state->ps[i][j] < -borderR){
                    //state->ps[i][j] = -2-(state->ps[i][j] + 2);
                    if(state->vs[i][j] < 0) state->vs[i][j] *= -0.9;
                    state->vs[i][j] += 0.04;
                }
            }
        }



        dtAcc += dt;
        dt = dtNew;
    }

    for (isz i = 0; i < arrlen(state->ps); i++) for (isz j = 0; j < arrlen(state->ps); j++) {
        if(i==j) continue;
        if(magSqr(state->ps[i] - state->ps[j]) > 0.2) continue;
        if(!!isRed(i) == !!isRed(j)) continue;

        DrawLineEx(toV(state->ps[i]), toV(state->ps[j]), 3/cam->zoom, BLACK);
    }

    for (isz i = 0; i < arrlen(state->ps); i++) {
        DrawCircleV(toV(state->ps[i]), 0.1, isRed(i) ? RED : BLUE);
    }
    state->dt = dt;


    EndMode2D();

    DrawText(TextFormat("dt: %.5f", dt), 10, 30, 20, LIME);

    DrawFPS(10, 10);
    EndDrawing();

}


State* init() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);
    InitWindow(600, 400, "Voll Grafik");
    SetTargetFPS(60);

    size_t size = sizeof(State);
    if(size == 0) size = 1;
    state = calloc(size, 1);
    if(!state && size) puts("buy more ram"), exit(1);


    state->cam.zoom = 100;
    state->cam.offset = (Vector2){
        GetScreenWidth() * 0.5,
        GetScreenHeight()* 0.5,
    };
    state->dt = 1/60.;

    //SetRandomSeed(2);

    static_assert(arrlen(state->ps) == arrlen(state->vs), "vector length");
    for (isz i = 0; i < arrlen(state->ps); i++) {
        state->ps[i] = (ℝ2){
            GetRandomValue(-1000, 1000)/1000.0,
            GetRandomValue(-1000, 1000)/1000.0,
        } * 2;
        state->vs[i] = (ℝ2){
            GetRandomValue(-1000, 1000)/1000.0,
            GetRandomValue(-1000, 1000)/1000.0,
        };

        //Vector2 mp = GetScreenToWorld2D(GetMousePosition(), *cam);
        //state->ps[i] = (ℝ2){mp.x, mp.y};
    }
    state->dragging = -1;

    return state;
}
