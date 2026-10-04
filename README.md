# m_arena

A simple arena (linear) memory allocator for C.

## Overview

`m_arena` hands out memory from a single contiguous block obtained with `mmap`. Allocation is a pointer bump, so there is no need for symmetrical `malloc()`/`free()` pairs: many allocations can happen without freeing any of them, and the memory is released in bulk, which keeps the free overhead low.

- **Contiguous block:** one `mmap` call, one `munmap` call.
- **No per-allocation free:** release everything at once with `arena_clear` / `arena_pop_to` (reuse the block) or `arena_destroy` (unmap it).
- **Embedded header:** the `Arena` header is stored at the start of the same block, so `arena_init` returns a single handle and no separate storage is needed.
- **Fixed capacity:** usable size is the requested size minus the header. `arena_push` returns `NULL` when the arena runs out of space.
- **Linux only** for now (uses `mmap` with `MAP_ANONYMOUS`).

## API

| Function | Description |
|---|---|
| `Arena *arena_init(u64 size)` | Maps a block of `size` bytes and places the header at its start. Returns `NULL` on failure. |
| `void arena_destroy(Arena *arena)` | Unmaps the whole block. The pointer is invalid afterwards. |
| `void *arena_push(Arena *arena, u64 size)` | Allocates `size` bytes (aligned to pointer size). Returns `NULL` if there is not enough space. |
| `void arena_pop(Arena *arena, u64 size)` | Releases the last `size` bytes (clamped to the current position). |
| `void arena_pop_to(Arena *arena, u64 pos)` | Releases everything above position `pos`. |
| `void arena_clear(Arena *arena)` | Resets the arena to empty. |

## Notes

- Memory is never freed individually; the lifetime of an allocation is the lifetime of the arena (or until it is popped).
- Allocations are aligned to `sizeof(void*)`.
- After `arena_destroy`, set your pointer to `NULL` to catch use-after-destroy.
