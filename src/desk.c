#include "server.h"
#include "shader.h"
#include "events.h"

int main() {
  struct DeskServer *server = newServer();
  startServer(server);

  return 0;
  
};
