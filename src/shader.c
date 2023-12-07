#include "shader.h"

struct shader* newShader(const char* vertFilePath, const char* fragFilePath) {
  LOG("Loading shader from \"%s\" and \"%s\"", vertFilePath, fragFilePath);
  FILE *vertFile, *fragFile;
  long length;
  char *buffer;

  vertFile = fopen(vertFilePath, "r");
  ASSERT(vertFile, "Failed to load vertex shader: %s", vertFilePath);
  fragFile = fopen(fragFilePath, "r");
  ASSERT(vertFile, "Failed to load fragment shader: %s", fragFilePath);

  // Vertex
  fseek (vertFile, 0, SEEK_END);
  length = ftell (vertFile);
  fseek (vertFile, 0, SEEK_SET);
  buffer = malloc(length);
  ASSERTN(buffer);
  ASSERTN(fread(buffer, 1, length, vertFile));

  unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
  GL_CHECK(glShaderSource(vertex, 1, (void*)buffer, NULL));
  GL_CHECK(glCompileShader(vertex));

  // Fragment
  free(buffer);
  fseek (fragFile, 0, SEEK_END);
  length = ftell (fragFile);
  fseek (fragFile, 0, SEEK_SET);
  buffer = malloc(length);
  ASSERTN(buffer);
  ASSERTN(fread (buffer, 1, length, fragFile));
  unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
  GL_CHECK(glShaderSource(fragment, 1, (void*)buffer, NULL));
  GL_CHECK(glCompileShader(fragment));

  // Program
  struct shader *shader = malloc(sizeof(shader));
  shader->ID = glCreateProgram();
  glAttachShader(shader->ID, vertex);
  glAttachShader(shader->ID, fragment);
  GL_CHECK(glLinkProgram(shader->ID));

  glDeleteShader(vertex);
  glDeleteShader(fragment);
  free(buffer);
  fclose(vertFile);
  fclose(fragFile);
  return shader;
}

void destroyShader(struct shader *shader) {
  glDeleteProgram(shader->ID);
}

void useShader(struct shader *shader) {
  glUseProgram(shader->ID);
}

void setInt(struct shader *shader, const char* loc, int val) {
  glUniform1i(glGetUniformLocation(shader->ID, loc), val);
}

void setFloat(struct shader *shader, const char* loc, float val) {
  glUniform1f(glGetUniformLocation(shader->ID, loc), val);
}
