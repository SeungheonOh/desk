#include "shader.h"

struct shader* newShader(const char* vertFilePath, const char* fragFilePath) {
  LOG("Loading shader from \"%s\" and \"%s\"", vertFilePath, fragFilePath);
  FILE *vertFile, *fragFile;
  long length;
  char *buffer;
  int success;
  char infoLog[512];  

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
  ASSERT(vertex, "Failed to create vertex shader");
  glShaderSource(vertex, 1, (void*)&buffer, NULL);
  glCompileShader(vertex);

  glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
  if (!success) {    
    glGetShaderInfoLog(vertex, 512, NULL, infoLog);

    EXPLODE("vertex shader failed to compile: %s", infoLog);
  }

  // Fragment
  free(buffer);
  fseek (fragFile, 0, SEEK_END);
  length = ftell (fragFile);
  fseek (fragFile, 0, SEEK_SET);
  buffer = malloc(length);
  ASSERTN(buffer);
  ASSERTN(fread (buffer, 1, length, fragFile));

  unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
  ASSERT(fragment, "Failed to create fragment shader");  
  glShaderSource(fragment, 1, (void*)&buffer, NULL);
  glCompileShader(fragment);
  
  glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragment, 512, NULL, infoLog);

    EXPLODE("fragment shader failed to compile: %s", infoLog);
  }  

  // Program
  struct shader *shader = malloc(sizeof(shader));
  shader->ID = glCreateProgram();
  glAttachShader(shader->ID, vertex);
  glAttachShader(shader->ID, fragment);
  glLinkProgram(shader->ID);

  glGetProgramiv(shader->ID, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shader->ID, 512, NULL, infoLog);
    wlr_log(WLR_ERROR, "shaders failed to link: %s", infoLog);
  }  

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
