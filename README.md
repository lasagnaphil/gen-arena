# gen-arena: C++ Generational Arena library

> "Generational Indices for the Masses!"

This is an implementation of a generational arena (or a slot map, sparse set, etc. depending on the terminology), implemented in C++11 with minimal dependencies.

## Using the library

This is a header-only library, so just grab the header files in `src/` and you're good to go. (All three header files are needed!)

- `gen_arena_config.h` contains various functions (logging, malloc implementation, etc.) that you can override.
- `gen_arena.h` contains a fully templated C++11 implementation of a generational arena. Most users will use this directly. 
- `gen_arena_raw.h` contains a low-level C++11 implementation of a generational arena, without any dependency on the STL.
  Note that it is not templated for the item type, and therefore uses type-erased `void*` pointers in the API.
  This is intended as a base class to create customized high-level containers (like `gen_arena.h`), so most users will probably not use this directly.

## Configuration

There are lots of things you can configure, but the library comes with sane defaults that will be fine in most scenarios.

### Change the amount of bits used for the various index types

Note that both `GenArena` and `GenArenaRaw` classes are templated with a `Config` struct that the user can override.
These contain the settings for how many bits should be used for the index, typeid, and generation indices. (The maximum is 32 bits for each type.)
The user should specify this using the `GenArenaConfig` struct.

The default is `using GenArenaDefaultConfig = GenArenaConfig<32, 8, 24>`, which means 32 bits for the indices, 8 bits for the type id, and 24 bits for the generational index.

### Change the basic functions used in the library (malloc, logging, ..)

In `gen_arena_config.h` you can see the default implementations of various utility functions:

- `gen_arena_aligned_alloc` / `gen_arena_aligned_free`
- `gen_arena_assert`
- `gen_arena_clz`
- `gen_arena_log`

More details for these functions are written in the comments. 
You might not be happy with the default implementation, depending on the circumstances (conventions of the project, performance constraints, weird platforms or compilers, etc.)
Feel free to swap out these functions on your own!

## Building the tests

Do the typical steps `mkdir build && cd build && cmake ..`. If you want to enable ASan on Windows you can add `-DUSE_ASAN`.

## License (MIT)

Copyright 2022-2022 Phil Chang

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.