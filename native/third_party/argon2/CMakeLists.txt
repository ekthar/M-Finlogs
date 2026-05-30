# Vendored phc-winner-argon2 (reference implementation)
#
# Upstream: https://github.com/P-H-C/phc-winner-argon2
# Commit:   f57e61e19229e23c4445b85494dbf7c07de721cb (2021-06-25)
# License:  CC0 1.0 Universal / Apache-2.0 (dual licensed, see LICENSE)
#
# This builds the REFERENCE (non-SIMD) Argon2 implementation so that it
# compiles on any platform without requiring CPU intrinsics. It uses
# src/ref.c (NOT src/opt.c) and src/blake2/blamka-round-ref.h.
#
# The resulting static library target `argon2` exposes argon2id_verify(),
# argon2_verify() and the $argon2id$... encoder/decoder, which the
# M-Finlogs native client uses to verify password hashes produced by the
# Electron/Python backend (argon2-cffi).

cmake_minimum_required(VERSION 3.16)

project(argon2 LANGUAGES C)

add_library(argon2 STATIC
    src/argon2.c
    src/core.c
    src/encoding.c
    src/blake2/blake2b.c
    src/thread.c
    src/ref.c
)

# Public API header (argon2.h) lives in include/.
# Internal headers (core.h, encoding.h, blake2/*.h, thread.h) live in src/
# and are only needed while compiling this library, so src/ is PRIVATE.
target_include_directories(argon2
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# thread.c is compiled normally (threading enabled). On Windows it uses the
# Win32 threading API (no extra linking required); elsewhere it uses pthreads.
if(NOT WIN32)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(argon2 PUBLIC Threads::Threads)
endif()
