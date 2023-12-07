#pragma once
#include <stdlib.h>
#include "imports.h"

struct shader {
  unsigned int ID;
};

struct shader* newShader(const char*, const char*);
void destroyShader(struct shader *);
void useShader(struct shader *);
void setInt(struct shader *, const char*, int);
void setFloat(struct shader *, const char*, float);
