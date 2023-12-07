#include "output.h"

struct Output *mkOutput(struct DeskServer *container, struct wlr_output* data){
  struct Output *output = malloc(sizeof(struct Output));
  output->server = container;
  output->wlr_output = data;

  ATTACH(Output, output, data->events.frame, frame);
  ATTACH(Output, output, data->events.request_state, requestState);
  ATTACH(Output, output, data->events.destroy, destroy);

  wl_list_insert(&container->outputs, &output->link);

  return output;
}

void destroyOutput(struct Output *container){
  wl_list_remove(&container->frame.link);
  wl_list_remove(&container->requestState.link);
  wl_list_remove(&container->destroy.link);
  wl_list_remove(&container->link);
  free(container);
}

HANDLE(frame, void, Output) {
  LOG ("FRAME!!!");
}

HANDLE(requestState, struct wlr_output, Output) {
  LOG ("FRAME!!!");
}

HANDLE(destroy, struct wlr_output, Output) {
  LOG ("Destroying %s", data->name);

  destroyOutput(container);
}
