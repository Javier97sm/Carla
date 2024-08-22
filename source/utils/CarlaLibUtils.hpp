/*
 * Carla library utils
 * Copyright (C) 2011-2022 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the doc/GPL.txt file.
 */

#ifndef CARLA_LIB_UTILS_HPP_INCLUDED
#define CARLA_LIB_UTILS_HPP_INCLUDED

#include "CarlaUtils.hpp"

#ifdef CARLA_OS_WIN
typedef HMODULE lib_t;
#else
# include <dlfcn.h>
typedef void* lib_t;
#endif

// -----------------------------------------------------------------------
// library related calls

typedef lib_t (*RunFuncWithEmulatorFunction)(const void *, const char *);
static RunFuncWithEmulatorFunction RunFuncWithEmulator = NULL;

typedef lib_t (*LoadLibraryWithEmulatorFunction)(const char*);
static LoadLibraryWithEmulatorFunction LoadLibraryWithEmulator = NULL;

typedef int (*InitializeFunction)();

static void InitBox64() {
    char box64_lib_path[] = "/home/javier/Documents/Github/box64/build/libbox64.so";
    char box64_ld_library_path[] = "/home/javier/Documents/Github/box64/x64lib";

    setenv("BOX64_LD_LIBRARY_PATH", box64_ld_library_path, 1);

    void* box64_lib_handle = dlopen(box64_lib_path, RTLD_GLOBAL | RTLD_NOW);
    if (!box64_lib_handle) {
        fprintf(stderr, "Error loading box64 library: %s\n", dlerror());
        abort();
    }

    void* box64_init_func = dlsym(box64_lib_handle, "Initialize");
    if (!box64_init_func) {
        fprintf(stderr, "Error getting symbol \"Initialize\" from box64 library: %s\n", dlerror());
        abort();
    }
    int (*Initialize)() = reinterpret_cast<InitializeFunction>(box64_init_func);
    if (Initialize() != 0) {
        fprintf(stderr, "Error initializing box64 library\n");
        abort();
    }

    LoadLibraryWithEmulator = reinterpret_cast<LoadLibraryWithEmulatorFunction>(dlsym(box64_lib_handle, "LoadX64Library"));
    if (!LoadLibraryWithEmulator) {
        fprintf(stderr, "Error getting symbol \"LoadX64Library\" from box64 library: %s\n", dlerror());
        abort();
    }

    RunFuncWithEmulator = reinterpret_cast<RunFuncWithEmulatorFunction>(dlsym(box64_lib_handle, "RunX64Function"));
    if (!RunFuncWithEmulator) {
        fprintf(stderr, "Error getting symbol \"RunX64Function\" from box64 library: %s\n", dlerror());
        abort();
    }

    printf("box64 library initialized.\n");
}

/*
 * Open 'filename' library (must not be null).
 * May return null, in which case "lib_error" has the error.
 */
static inline
lib_t lib_open(const char* const filename, const bool global = false, const bool use_libbox64 = false) noexcept
{
    CARLA_SAFE_ASSERT_RETURN(filename != nullptr && filename[0] != '\0', nullptr);

    try {
#ifdef CARLA_OS_WIN
        return ::LoadLibraryA(filename);
        // unused
        (void)global;
#else
	if (use_libbox64) {
            InitBox64();
	    return LoadLibraryWithEmulator(filename);
	} else {			
            return ::dlopen(filename, RTLD_NOW|(global ? RTLD_GLOBAL : RTLD_LOCAL));
	}
#endif
    } CARLA_SAFE_EXCEPTION_RETURN("lib_open", nullptr);
}

/*
 * Close a previously opened library (must not be null).
 * If false is returned, "lib_error" has the error.
 */
static inline
bool lib_close(const lib_t lib) noexcept
{
    CARLA_SAFE_ASSERT_RETURN(lib != nullptr, false);

    try {
#ifdef CARLA_OS_WIN
        return ::FreeLibrary(lib);
#else
        return (::dlclose(lib) == 0);
#endif
    } CARLA_SAFE_EXCEPTION_RETURN("lib_close", false);
}

/*
 * Get a library symbol (must not be null) as a function.
 * Returns null if the symbol is not found.
 */
template<typename Func>
static inline
Func lib_symbol(const lib_t lib, const char* const symbol, const bool use_libbox64 = false) noexcept
{
    CARLA_SAFE_ASSERT_RETURN(lib != nullptr, nullptr);
    CARLA_SAFE_ASSERT_RETURN(symbol != nullptr && symbol[0] != '\0', nullptr);

    try {
#ifdef CARLA_OS_WIN
# if defined(__GNUC__) && (__GNUC__ >= 9)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
# endif
        return reinterpret_cast<Func>(::GetProcAddress(lib, symbol));
# if defined(__GNUC__) && (__GNUC__ >= 9)
#  pragma GCC diagnostic pop
# endif
#else
	if (use_libbox64) {
	    //InitBox64();
            //lib_t paco = LoadLibraryWithEmulator("/home/javier/.vst/yabridge/Toneforge-MishaMansoorAdvanced.so");
            return reinterpret_cast<Func>(RunFuncWithEmulator(lib, symbol));
	} else {
	    return reinterpret_cast<Func>(::dlsym(lib, symbol));
	}
#endif
    } CARLA_SAFE_EXCEPTION_RETURN("lib_symbol", nullptr);
}

/*
 * Return the last operation error ('filename' must not be null).
 * May return null.
 */
static inline
const char* lib_error(const char* const filename, const bool use_libbox64 = false) noexcept
{
    CARLA_SAFE_ASSERT_RETURN(filename != nullptr && filename[0] != '\0', nullptr);

#ifdef CARLA_OS_WIN
    static char libError[2048+1];
    carla_zeroChars(libError, 2048+1);

    try {
        const DWORD winErrorCode  = ::GetLastError();
        const int   winErrorFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS;
        LPVOID      winErrorString;

        ::FormatMessage(winErrorFlags, nullptr, winErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&winErrorString, 0, nullptr);

        std::snprintf(libError, 2048, "%s: error code %li: %s", filename, winErrorCode, (const char*)winErrorString);
        ::LocalFree(winErrorString);
    } CARLA_SAFE_EXCEPTION("lib_error");

    return (libError[0] != '\0') ? libError : nullptr;
#else
    return ::dlerror();
#endif
}

// -----------------------------------------------------------------------

#endif // CARLA_LIB_UTILS_HPP_INCLUDED
