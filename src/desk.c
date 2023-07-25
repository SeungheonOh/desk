#include "server.h"

#include "events.h"

int main() {
  struct DeskServer server = {0};
  if(initializeServer(&server)) {
    printf("error\n");
    return 1;
  }

  return 0;
};
