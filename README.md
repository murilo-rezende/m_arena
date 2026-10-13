# m_arena

A simple arena (linear) allocator for C, in a single header file.

Allocations just move a pointer forward. You never free them one by one: rewind the arena to reuse the memory, or destroy it to release everything at once.

Linux only.

## Usage

```c
#include "m_arena.h"

Arena *arena = arena_init(MiB(12));          // reserve 12 MiB
int *nums = ARENA_PUSH_ARRAY(arena, int, 100); // allocate
arena_clear(arena);                          // rewind and reuse
arena_destroy(arena);                        // release everything
```

## Functions

| Function | What it does |
|---|---|
| `arena_init(size)` | Create an arena. Returns `NULL` on failure. |
| `arena_push(arena, size)` | Allocate bytes. Returns `NULL` if full. |
| `arena_pop(arena, size)` | Give back the last `size` bytes. |
| `arena_pop_to(arena, pos)` | Rewind to an earlier position. |
| `arena_clear(arena)` | Rewind to empty. |
| `arena_destroy(arena)` | Release the whole arena. |

## Try it

```sh
gcc test.c -o test
./test
```
