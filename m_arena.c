/*
    m_arena is a simple arena (linear) allocator for C.

    Memory comes from a single contiguous block obtained with mmap.
    Allocations are a pointer bump: no per-allocation free(). Many
    allocations can happen, and everything is released at once, either
    with arena_clear / arena_pop_to (reuse the block) or arena_destroy
    (unmap it).
    The Arena header is stored at the start of the same block, so
    arena_init returns a single handle with no separate storage needed.
    The block has a fixed capacity. Usable size is the requested size
    minus the header. arena_push returns NULL when it runs out of space.
    Currently Linux only (uses mmap with MAP_ANONYMOUS).

    source material:
    https://www.dgtlgrove.com/p/untangling-lifetimes-the-arena-allocator
    https://gist.github.com/Magicalbat/4e085cadeed46c7b6f917ea9e9220d6a
*/

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;
typedef i32       b32;
typedef float     f32;
typedef double    f64;

#define KiB(x) (u64)((u64)(x) << 10)
#define MiB(x) (u64)((u64)(x) << 20)
#define GiB(x) (u64)((u64)(x) << 30)

#define ALIGN_UP(n) (((n) + sizeof(void*) - 1) & ~(sizeof(void*) - 1))

#define ARENA_PUSH_STRUCT(arena, type) (type*)arena_push((arena), sizeof(type))
#define ARENA_PUSH_ARRAY(arena, type, n) (type*)arena_push((arena), sizeof(type) * (n))

//Platform specific functions
u32 get_pagesize(void);
void* reserve_memory(u64 size);
b32 commit_memory(void *ptr, u64 size);
b32 decommit_memory(void *ptr, u64 size);
b32 release_memory(Arena *arena);


//Need refactoring
typedef struct Arena {
    u8 *buffer;
    u64 buffer_size;
    u64 pos;
} Arena;

#define ARENA_HEADER ALIGN_UP(sizeof(Arena))

Arena *arena_init(u64 size);
void arena_destroy(Arena *arena);
void *arena_push(Arena *arena, u64 size);
void arena_pop(Arena *arena, u64 size);
void arena_pop_to(Arena *arena, u64 pos);
void arena_clear(Arena *arena);

int main(void) {        
    Arena *arena = arena_init(MiB(12));

    if (!arena) {
        perror("mmap");
        return 1;
    }

    int *nums = ARENA_PUSH_ARRAY(arena, int, 100);
    if (nums) nums[0] = 42;
    printf("pos after push: %llu\n", (unsigned long long)arena->pos);
 
    arena_clear(arena);
    printf("pos after clear: %llu\n", (unsigned long long)arena->pos);
 
    arena_destroy(arena);
    return 0;

}

//Need refactoring
Arena *arena_init(u64 size) {
    if (size <= ARENA_HEADER) return NULL;

    void *mem = reserve_memory(size);

    //Creates arena and set the usable memory after its header
    Arena *arena = (Arena*)mem;
    arena->buffer = (u8*)mem + ARENA_HEADER;
    arena->buffer_size = size - ARENA_HEADER;
    arena->pos = 0;
    return arena;
}

void arena_destroy(Arena *arena) {
    release_memory(arena);
}

//Need refactoring
void *arena_push(Arena *arena, u64 size) {
    u64 aligned_size = ALIGN_UP(size);
    u64 aligned_pos = ALIGN_UP(arena->pos); 
    u64 curr_pos = aligned_pos + aligned_size;

    //Checks if there is enough space to push
    if (curr_pos > arena->buffer_size) return NULL;

    arena->pos = curr_pos;
    
    return (u8*)arena->buffer + aligned_pos;
}

//Need refactoring
void arena_pop(Arena *arena, u64 size) {
    if (size > arena->pos) size = arena->pos;
    arena->pos -= size;
}

//Need refactoring
void arena_pop_to(Arena *arena, u64 pos) {
    //If the pos is less then the arena position
    //Pops to the desired position, else 0
    u64 size = pos < arena->pos ? arena->pos - pos : 0;
    arena_pop(arena, size);
}

//Need refactoring
void arena_clear(Arena *arena) {
    arena_pop_to(arena, 0);
}

#ifdef __linux__
#define _DEFAULT_SOURCE

#include <unistd.h>
#include <sys/mman.h>

//Returns the page size of the OS
u32 get_pagesize(void) {
    u32 page_size = sysconf(_SC_PAGESIZE);
    return page_size;
}   

//Reseres a memory region
void* reserve_memory(u64 size) {
    void *mem = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return NULL;

    return mem;
}

//Commits memory in the given memory region
b32 commit_memory(void *ptr, u64 size) {
    i32 commit = mprotect(ptr, size, PROT_READ | PROT_WRITE);
    return commit == 0;
}

//Decommits memory in the given memory region
b32 decommit_memory(void *ptr, u64 size) {
    i32 decommit = (ptr, size, PROT_NONE);
    if (decommit != 0) return false;
    
    decommit = madvise(ptr, size, MADV_DONTNEED);
    return decommit == 0;
}

//Releases the memory region
b32 release_memory(Arena *arena) {
    if (!arena) return;

    i32 release = munmap(arena, ARENA_HEADER + arena->buffer_size);
    return release == 0;
}

#endif