/*
    m_arena is a simple arena (linear) allocator for C.

    Memory comes from a single contiguous block obtained.
    Allocations are a pointer bump: no per-allocation free(). Many
    allocations can happen, and everything is released at once, either
    with arena_clear / arena_pop_to (reuse the block) or arena_destroy.
    Currently Linux only.

    source material:
    https://www.dgtlgrove.com/p/untangling-lifetimes-the-arena-allocator
    https://gist.github.com/Magicalbat/4e085cadeed46c7b6f917ea9e9220d6a
*/
#define _DEFAULT_SORCE

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

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

//Align to pointer size
#define ALIGN_UP(n) (((n) + sizeof(void*) - 1) & ~(sizeof(void*) - 1))

//Align to any power of two
#define ALIGN_UP_POW2(n, p) (((n) + ((p) - 1)) & ~((u64)(p) - 1))

#define MIN(a, b) ((a) < (b) ? : (b))

#define ARENA_COMMIT_SIZE KiB(64)
#define ARENA_PUSH_STRUCT(arena, type) (type*)arena_push((arena), sizeof(type))
#define ARENA_PUSH_ARRAY(arena, type, n) (type*)arena_push((arena), sizeof(type) * (n))

typedef struct Arena {
    u64 reserve_size;
    u64 commit_size;
    u64 pos;
    u64 commit_pos;
} Arena;

#define ARENA_HEADER ALIGN_UP(sizeof(Arena))
#define ARENA_BASE_POS ARENA_HEADER

//Platform specific functions
u32 get_pagesize(void);
void* reserve_memory(u64 size);
b32 commit_memory(void *ptr, u64 size);
b32 decommit_memory(void *ptr, u64 size);
b32 release_memory(void *ptr, u64 size);

//General functions
Arena *arena_init(u64 reserve_size);
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
 
    //Crosses the initial commit chunk, so arena_push has to commit more
    u8 *big = arena_push(arena, MiB(1));
    if (big) big[MiB(1) - 1] = 1;
    printf("commit_pos after big push: %llu\n", (unsigned long long)arena->commit_pos);
 
    //Larger than what is left, must fail cleanly
    printf("oversized push: %s\n", arena_push(arena, MiB(12)) ? "ok (bug)" : "NULL");
 
    arena_clear(arena);
    printf("pos after clear: %llu\n", (unsigned long long)arena->pos);
 
    arena_destroy(arena);
    return 0;
}

Arena *arena_init(u64 reserve_size) {
    u64 page_size = get_pagesize();
    u64 commit_size = ALIGN_UP_POW2(ARENA_COMMIT_SIZE, page_size);
    
    reserve_size = ALIGN_UP_POW2(reserve_size, page_size);
    if (reserve_size <= ARENA_HEADER) return NULL;

    void *mem = reserve_memory(reserve_size);
    if (!mem) return NULL;

    u64 initial_commit = MIN(ALIGN_UP_POW2(ARENA_HEADER, commit_size), reserve_size);
    if (!commit_memory(mem, initial_commit)) {
        release_memory(mem, reserve_size);
        return NULL;
    }
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
    Arena *arena = (Arena*)mem;
    arena->reserve_size = reserve_size;
    arena->commit_size = commit_size;
    arena->pos = ARENA_BASE_POS;
    arena->commit_pos = initial_commit;

    return arena;
}

void arena_destroy(Arena *arena) {
    if (!arena) return;

    u64 size = arena->reserve_size;
    release_memory(arena, size);
}

void *arena_push(Arena *arena, u64 size) {
    u64 aligned_pos = ALIGN_UP(arena->pos);

    //Checks if there is enoguh reserved space
    if (aligned_pos > arena->reserve_size) return NULL;
    if (size > arena->reserve_size - aligned_pos) return NULL;

    u64 curr_pos = aligned_pos + size;

    //Commits more space when the push goes past the committed region
    if (curr_pos > arena->commit_pos) {
        u64 new_commit = ALIGN_UP_POW2(curr_pos, arena->commit_size);
        new_commit = MIN(new_commit, arena->reserve_size);

        u8 *commit_start = (u8*)arena + arena->commit_pos;
        if (!commit_memory(commit_start, new_commit - arena->commit_pos)) return NULL;

        arena->commit_pos = new_commit;
    }

    arena->pos = curr_pos;

    return (u8*)arena + aligned_pos;
}

void arena_pop(Arena *arena, u64 size) {
    u64 used = arena->pos - ARENA_BASE_POS;

    if (size > used) size = used;
    arena->pos -= size;
}

void arena_pop_to(Arena *arena, u64 pos) {
    //If the positon is less than the start of the chunk, pop to the start of the chunk
    if (pos < ARENA_BASE_POS) pos = ARENA_BASE_POS;

    //If the pos is less then the arena position
    //Pops to the desired position, else 0
    u64 size = pos < arena->pos ? arena->pos - pos : 0;
    arena_pop(arena, size);
}

void arena_clear(Arena *arena) {
    arena_pop_to(arena, ARENA_BASE_POS);
}

#ifdef __linux__
#include <unistd.h>
#include <sys/mman.h>

//Returns the page size of the OS
u32 get_pagesize(void) {
    u32 page_size = sysconf(_SC_PAGESIZE);
    return page_size;
}   

//Reserves a memory region
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
    i32 decommit = madvise(ptr, size, MADV_DONTNEED);
    if (decommit != 0) return 0;
 
    decommit = mprotect(ptr, size, PROT_NONE);
    return decommit == 0;
}

//Releases the memory region
b32 release_memory(void *ptr, u64 size) {
    if (!ptr) return 0;

    i32 release = munmap(ptr, size);
    return release == 0;
}

#endif