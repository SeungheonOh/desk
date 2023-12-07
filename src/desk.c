#include "server.h"
#include "shader.h"
#include "events.h"

int main() {
  struct DeskServer *server = newServer();
  startServer(server);

  newShader("./src/shader/vert.glsl", "./src/shader/frag.glsl");

  return 0;
};
