#pragma once
#include <stdlib.h>
#include "imports.h"

struct shader {
  unsigned int ID;
  const char *vertFile;
  const char *fragFile;  
};

struct shader* newShader(const char*, const char*);
void reloadShader(struct shader *);
void destroyShader(struct shader *);
void useShader(struct shader *);
void setInt(struct shader *, const char*, int);
void setFloat(struct shader *, const char*, float);
void set4fv(struct shader *, const char* , GLsizei, GLboolean, float *);
