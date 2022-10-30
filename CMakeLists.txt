cmake_minimum_required(VERSION 3.1)
project(gen_arena)
set(CMAKE_CXX_STANDARD 11)

add_library(gen_arena INTERFACE)
target_include_directories(gen_arena INTERFACE src)

add_executable(gen_arena_test test/main.cpp)
target_include_directories(gen_arena_test PRIVATE test)
target_link_libraries(gen_arena_test PRIVATE gen_arena)

# Windows-specific code for enabling ASAN.
if (USE_ASAN_WINDOWS)
    set(LLVM_DYNLIB_PATH "$ENV{ProgramFiles}/LLVM/lib/clang/15.0.2/lib/windows")
    target_compile_options(gen_arena_test PRIVATE -fsanitize=address)
    target_link_directories(gen_arena_test PRIVATE ${LLVM_DYNLIB_PATH})
    target_link_libraries(gen_arena_test PRIVATE clang_rt.asan_dynamic-x86_64 clang_rt.asan_dynamic_runtime_thunk-x86_64)
    target_link_options(gen_arena_test PRIVATE /wholearchive:clang_rt.asan_dynamic_runtime_thunk-x86_64.lib)
    target_compile_definitions(gen_arena_test PRIVATE _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
    add_custom_command(
            TARGET gen_arena_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${LLVM_DYNLIB_PATH}/clang_rt.asan_dynamic-x86_64.dll
                    ${CMAKE_CURRENT_BINARY_DIR}/clang_rt.asan_dynamic-x86_64.dll)
endif()