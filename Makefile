.PHONY: all trigger_reload

all: main live.so

live.so: live.c
	clang -O3 -Wall -o live.so $^ -fPIC -shared -lraylib -g

main: main.c
	clang -O3 -g -o main main.c "-Wl,-rpath=." -lraylib -ldl -lm

trigger_reload:
	pkill --signal SIGUSR1 -f ./main
