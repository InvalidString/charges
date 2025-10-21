#ifndef ALLOCATOR_H
#define ALLOCATOR_H




#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// TODO
// fallback allocator
// respect alignment
// respect flags


#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))
#define arrlen(arr) (sizeof (arr) / sizeof (arr)[0])
#define swap(a, b)      \
    do {                \
        typeof(a) TMP;  \
        TMP = (a);      \
        (a) = (b);      \
        (b) = TMP;      \
    } while (0)
#define cast(x, T) ((union{typeof(x) a; T b;}){.a = (x)}.b)
#define alignof(x) __alignof(x)
#define dbg(fmt, val) fprintf(stderr, #val": " fmt "\n", (val))

#ifndef ALLOC_DYNARR_ALIGN
    #define ALLOC_DYNARR_ALIGN sizeof(void*)
#endif


typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int16_t i16;
typedef unsigned char u8;
typedef char i8;
typedef size_t usz;
typedef ssize_t isz;

#define s8(literal) (s8){literal, sizeof(literal)-1}
typedef struct{i8* ptr; isz len;} s8;
#define String(literal) (String){literal, sizeof(literal)-1, 0}
typedef struct{i8* ptr; isz len; isz cap;} String;


#define s8FMT "%.*s"
#define s8FMT_(x) (int)(x).len, (x).ptr



typedef struct Allocator Allocator;


struct Allocator{
    void* (*reallocate)(
        Allocator *a,
        void* ptr,
        isz size,
        isz align,
        isz oldCount,
        isz newCount,
        usz flags
    );
};

typedef struct {
    Allocator as_alloc[1];
} AnyAllocator;

typedef enum{
    AF_SOFTFAIL = 0x1,
    AF_DONT_ZERO = 0x2,
} AllocatorFlags;



#define as_alloc(x) ((x)->as_alloc)

#define new(...) newx(__VA_ARGS__,new4,new3,new2)(__VA_ARGS__)
#define newx(a,b,c,d,e,...) e
#define new2(a, t) (t *)alloc_alloc(as_alloc(a), sizeof(t), alignof(t), 1, 0)
#define new3(a, t, n) (t *)alloc_alloc(as_alloc(a), sizeof(t), alignof(t), n, 0)
#define new4(a, t, n, f) \
    (t *)alloc_alloc(as_alloc(a), sizeof(t), alignof(t), n, f)

#define DecDynArr(T)  \
    typedef struct{   \
        T *ptr;       \
        isz len;    \
        isz cap;    \
    }ArrOf ## T;

DecDynArr(u8);
DecDynArr(i32);

#define ainit(T, ...)                                          \
    ((T){                                                      \
        .ptr = (typeof(*(T){}.ptr)[]){__VA_ARGS__},            \
        .len = arrlen(((typeof(*(T){}.ptr)[]){__VA_ARGS__})),  \
        .cap = 0,                                              \
    })

#define apush(arr, a)                                   \
    (*(alloc_dynarr_grow(                               \
        as_alloc(a),                                    \
        (void**)&(arr)->ptr, &(arr)->len, &(arr)->cap,  \
        sizeof(*(arr)->ptr),        1                   \
    ), &(arr)->ptr[(arr)->len - 1]))

#define anext(arr, n, a)                                \
    (alloc_dynarr_grow(                                 \
        as_alloc(a),                                    \
        (void**)&(arr)->ptr, &(arr)->len, &(arr)->cap,  \
        sizeof(*(arr)->ptr), (n)                        \
    ), &(arr)->ptr[(arr)->len - (n)])

#define aget(arr, i)                                 \
    (*(assert((i) < (arr)->len && "out of bounds"),  \
     &(arr)->ptr[i]))

#define aget_or(arr, i, or)                  \
    ((i) < (arr)->len ? (arr)->ptr[i] : or)

#define aget_grow(arr, i, a)                                \
    (*((i) >= (arr)->len                                    \
        ? (alloc_dynarr_grow(                               \
            as_alloc(a),                                    \
            (void**)&(arr)->ptr, &(arr)->len, &(arr)->cap,  \
            sizeof(*(arr)->ptr), (i)-(arr)->len+1           \
        ),0)                                                \
        : 0,                                                \
     &(arr)->ptr[i]))

#define alast(arr) aget((arr), (arr)->len - 1)

#define asplice(a, i, n, src, alloc)              \
    alloc_dynarr_splice(                          \
        as_alloc(alloc),                          \
        (void**)&(a)->ptr, &(a)->len, &(a)->cap,  \
        (src).ptr, (src).len,                     \
        i, n,                                     \
        sizeof(*(a)->ptr)                         \
    )


#define aput(arr, a, i, ...) \
    asplice(arr, i, 0, ((typeof(*(arr))){.ptr = (typeof(*(arr)->ptr)[]){(__VA_ARGS__)}, .len=1,}), (a))

#define adel(arr, i) \
    asplice(arr, i, 1, ((typeof(*(arr))){}), noalloc)

void *alloc_reallocate(
    Allocator *a, void* ptr,
    isz size, isz align,
    isz oldCount, isz newCount, usz flags
);
static inline void* alloc_alloc(Allocator *a, isz size, isz align, isz count, usz flags){
    return alloc_reallocate(a, 0, size, align, 0, count, flags);
}

typedef struct {
    Allocator as_alloc[1];
    u8 *beg;
    u8 *end;
}Arena;

void* arena_reallocate(
    Allocator *allocator, void* ptr,
    isz size, isz align,
    isz oldCount, isz newCount, usz flags
);

Arena arena_mmap(isz size);
static inline Arena arena_buf(u8 *beg, u8 *end){
    return (Arena){
        .as_alloc = {{arena_reallocate}},
        .beg = beg,
        .end = end,
    };
}

// allocator that always fails
extern AnyAllocator noalloc[1];
// allocator using malloc and free
extern AnyAllocator aheap[1];

void alloc_dynarr_grow(
    Allocator* a,
    void **arr_ptr, isz *len, isz *cap,
    isz data_size, isz n
);

void alloc_dynarr_splice(
    Allocator* a,
    void **arr_ptr, isz *len, isz *cap,
    void* src_ptr, isz src_len,
    isz replace_start, isz replace_n,
    isz data_size
);


s8 aread_file(Arena *a, FILE* f);
s8 aprintf(Arena *a, char* fmt, ...);
s8 vaprintf(Arena *a, char* fmt, va_list va);

#endif /* ifndef ALLOCATOR_H */






#ifdef ALLOCATOR_IMPL

static void* heap_reallocate(
    Allocator *allocator, void* ptr,
    isz size, isz align,
    isz oldCount, isz newCount, usz flags
){
    if(newCount == 0){
        free(ptr);
        return 0;
    }
    void* res;
    if(!ptr){
        res = calloc(newCount, size);
        if(!res) goto oom;
    }else{

        #ifdef _WIN32
        res = realloc(ptr, newCount * size);
        #else
        res = reallocarray(ptr, newCount, size);
        #endif
        if(!res) goto oom;
    }
    return res;

    oom:
    if(flags & AF_SOFTFAIL) return 0;
    fprintf(stderr, "arena_alloc: Out of Memory\n");
    abort();
}

void *alloc_reallocate(
    Allocator *a, void* ptr,
    isz size, isz align,
    isz oldCount, isz newCount, usz flags
){
    if(a && a->reallocate){
        return a->reallocate(a,ptr,size,align,oldCount,newCount,flags);
    }
    return heap_reallocate(a, ptr, size, align, oldCount, newCount, flags);
}


void* arena_reallocate(
    Allocator *allocator, void* ptr,
    isz size, isz align,
    isz oldCount, isz newCount, usz flags
){
    Arena *a = (Arena*)allocator;

    isz padding = -(isz)a->beg & (align - 1);
    void* out = a->beg + padding;
    if(ptr && a->beg == ptr + oldCount*size){
        //extend
        newCount -= oldCount;
        out = ptr;
    }

    isz available = a->end - a->beg - padding;

    if (available < 0 || newCount > available/size) {
        if(flags & AF_SOFTFAIL) return 0;
        fprintf(stderr, "arena_reallocate: Out of Memory\n");
        abort();
    }
    a->beg += padding + newCount*size;

    if(out != ptr) memcpy(out, ptr, oldCount*size);

    if(!(flags & AF_DONT_ZERO))
        memset((u8*)out + oldCount*size, 0, (newCount-oldCount)*size);
    return out;
}

#ifndef _WIN32

#include <sys/mman.h>

Arena arena_mmap(isz size){
    u8* buf = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    if(buf==MAP_FAILED) perror("mmap"), exit(1);
    return arena_buf(buf, buf+size);
}

#endif /* ifndef _WIN32 */




static void *dont_reallocate(
    Allocator *a, void* ptr,
    isz size, isz align,
    isz oldCount, isz newCount, usz flags
){
    if(newCount <= oldCount) return ptr;
    if(newCount == 0 || (flags & AF_SOFTFAIL)) return 0;
    fprintf(stderr, "dont_reallocate: Out of Memory\n");
    abort();
}

AnyAllocator noalloc[1] = {{{{dont_reallocate}}}};
AnyAllocator aheap[1] = {};







void alloc_dynarr_grow(
    Allocator* a,
    void **arr_ptr, isz *len, isz *cap,
    isz data_size, isz n
){
    isz old_len = *len;
    isz old_cap = *cap;
    void* old_ptr = *arr_ptr;
    *len += n;
    if(*len <= *cap) return;
    if(!*cap) *cap = *len;


    void *new_ptr = alloc_reallocate(
        a, old_cap ? *arr_ptr : 0,
        2*data_size, ALLOC_DYNARR_ALIGN,
        old_cap, *cap * 2,
        0
    );
    if(!old_cap && new_ptr != *arr_ptr){
        assert(new_ptr != *arr_ptr);
        memcpy(new_ptr, *arr_ptr, old_len*data_size);
    }
    *arr_ptr = new_ptr;
    *cap *= 2;
}

void alloc_dynarr_splice(
    Allocator* a,
    void **arr_ptr, isz *len, isz *cap,
    void* src_ptr, isz src_len,
    isz replace_start, isz replace_n,
    isz data_size
){
    isz grow_n = src_len - replace_n;
    isz old_len = *len;
    if(grow_n > 0 || !*cap){
        alloc_dynarr_grow(
            a,
            arr_ptr, len, cap,
            data_size, max(grow_n, 0)
        );
    }
    memmove(
        ((u8*)*arr_ptr) + data_size * (replace_start + replace_n + grow_n),
        ((u8*)*arr_ptr) + data_size * (replace_start + replace_n),
        data_size * (old_len - (replace_start + replace_n))
    );
    memcpy(
        ((u8*)*arr_ptr) + data_size * (replace_start),
        src_ptr,
        src_len * data_size
    );
    *len = old_len + grow_n;
}

s8 aread_file(Arena *a, FILE* f){
    isz read = 0;
    u8 *beg = a->beg;
    for(;;){
        read += fread(a->beg, 1, a->end - a->beg, f);
        int ferr = ferror(f);
        if(ferr) perror("aread_file"), exit(1);
        if(feof(f)) break;
    }
    a->beg += read;
    new(a, char);
    return (s8){(i8*)beg, read};
}


s8 vaprintf(Arena *a, char* fmt, va_list va) {
    char* beg = (char*)a->beg;
    isz space = (char*)a->end - beg;
    isz len = vsnprintf(beg, space, fmt, va);
    va_end(va);
    alloc_alloc(as_alloc(a), len, 1, 1, AF_DONT_ZERO);

    return (s8){beg, len};
}

s8 __attribute__((format(printf, 2, 3)))
aprintf(Arena *a, char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    return vaprintf(a, fmt, va);
}

#endif

#ifdef ALLOCATOR_TEST


int main(void)
{
    Arena a = arena_mmap(0x1000);

    ArrOfi32 lel = ainit(ArrOfi32, 1,2,3,4,5);

    apush(&lel, aheap) = 44;
    apush(&lel, aheap) = 33;
    apush(&lel, aheap) = 55;
    apush(&lel, aheap) = 66;
    apush(&lel, aheap) = 77;
    for (isz i = 0; i < 10; i++) {
        apush(&lel, aheap) = i;
    }


    for (isz i = 0; i < lel.len; i++) {
        printf("%d\n", lel.ptr[i]);
    }
    free(lel.ptr);

    typeof(*(ArrOfi32){}.ptr) α = 9;

    ArrOfi32 lel2 = ainit(ArrOfi32, 1,2,3,4,5);

    int* blub = new(&a, int);

    apush(&lel2, &a) = 44;
    puts("lel2");
    for (isz i = 0; i < lel2.len; i++) {
        printf("%d\n", lel2.ptr[i]);
    }

    return 0;
}

#endif /* ifdef ALLOCATOR_TEST */
