#include "m_arena.h"
#include <stdio.h>

int main(void) {
    //Reserve 12 MiB of virtual memory. Almost nothing is committed yet.
    Arena *arena = arena_init(MiB(12));
    if (!arena) return 1;
 
    //Allocate an array of 100 ints from the arena
    int *nums = ARENA_PUSH_ARRAY(arena, int, 100);
    nums[0] = 42;
    printf("used after push:  %llu bytes\n", (unsigned long long)(arena->pos - ARENA_BASE_POS));
 
    //Too big for the arena: push fails cleanly and returns NULL
    printf("oversized push:   %s\n", arena_push(arena, MiB(12)) ? "ok (bug)" : "NULL");
 
    //Rewind everything. The memory stays committed and can be reused.
    arena_clear(arena);
    printf("used after clear: %llu bytes\n", (unsigned long long)(arena->pos - ARENA_BASE_POS));
 
    //Give the whole block back to the OS
    arena_destroy(arena);
    return 0;
}
