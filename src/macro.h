#pragma once
#include "imports.h"
#include <signal.h>

#define ASSERT(expr, reason, ...)		\
  if(!(expr)) { \
    printf("ASSERTION FAILED!\n  at %s:%d: %s\n  Reason: " reason "\n", __FILE__, __LINE__, #expr, ##__VA_ARGS__); \
    raise(SIGSEGV); \
  }

#define ASSERTN(expr) ASSERT(expr, "?")

const char* eglErrVerbose(EGLint err);

#define GL_CHECK(stmt) \
  do { \
  stmt; \
  GLenum err; \
  ASSERT((err = glGetError()) == GL_NO_ERROR, "OpenGL Error: %s", eglErrVerbose(err)); \
  } while (0)

#define LOG(...) wlr_log(WLR_INFO, __VA_ARGS__)
#define DEBUG(...) wlr_log(WLR_DEBUG, __VA_ARGS__)
