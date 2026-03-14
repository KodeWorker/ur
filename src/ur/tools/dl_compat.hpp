#pragma once

// ---------------------------------------------------------------------------
// dl_compat.hpp — thin portability shim over dlopen / LoadLibrary.
//
// Used exclusively by Loader to open and query shared-library plugins.
// Include this header only in loader.cpp — do not expose it further.
// ---------------------------------------------------------------------------

#if defined(_WIN32)
#include <windows.h>
using DlHandle = HMODULE;
inline DlHandle dl_open(const char* path) { return LoadLibraryA(path); }
inline void* dl_sym(DlHandle h, const char* sym) {
  return reinterpret_cast<void*>(GetProcAddress(h, sym));
}
inline void dl_close(DlHandle h) { FreeLibrary(h); }
#else
#include <dlfcn.h>
using DlHandle = void*;
// RTLD_LOCAL: prevent symbol collisions between independently loaded plugins.
// RTLD_LAZY:  resolve symbols on first use — faster load, fine for plugins.
inline DlHandle dl_open(const char* path) {
  return dlopen(path, RTLD_LAZY | RTLD_LOCAL);
}
inline void* dl_sym(DlHandle h, const char* sym) { return dlsym(h, sym); }
inline void dl_close(DlHandle h) { dlclose(h); }
#endif
