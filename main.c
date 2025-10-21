#include <raylib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

void* (*init)(void) = 0;
void (*update)(void) = 0;
void* (*before_reload)(size_t *size_out) = 0;
void (*after_reload)(void* old_state, size_t old_size) = 0;
void* live_lib = 0;

void reload(){

    if(live_lib){
        dlclose(live_lib);
        live_lib = 0;
    }
    live_lib = dlopen("live.so", RTLD_NOW);
    if(!live_lib) printf("libload fail %s\n", dlerror()), exit(1);

    init = dlsym(live_lib, "init");
    if(!init) printf("loading init symbol fail %s\n", dlerror()), exit(1);
    update = dlsym(live_lib, "update");
    if(!update) printf("loading update symbol fail %s\n", dlerror()), exit(1);
    after_reload = dlsym(live_lib, "after_reload");
    if(!after_reload) printf("loading after_reload symbol fail %s\n", dlerror()), exit(1);
    before_reload = dlsym(live_lib, "before_reload");
    if(!before_reload) printf("loading before_reload symbol fail %s\n", dlerror()), exit(1);
}

int should_reload = 0;
void on_siguser1(int sig){ should_reload = 1; }

int main(){
    struct sigaction act = {
        .sa_handler = on_siguser1,
        .sa_flags = SA_RESTART,
    };
    if(sigaction(SIGUSR1, &act, NULL)) perror("sigaction"),exit(1);

    reload();
    void* state = init();
    while(!WindowShouldClose()){
        update();
        if(should_reload){
            should_reload = 0;
            if(!system("make live.so")){
                size_t state_size = 0;
                state = before_reload(&state_size);
                reload();
                after_reload(state, state_size);
                puts("reloaded!");
            }
        }
    }
    CloseWindow();
}
