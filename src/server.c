#include "server.h"

int initializeServer(struct DeskServer *server) {
  wlr_log_init(WLR_DEBUG, NULL);

  if(server == NULL) return 1;
  server->display = wl_display_create();
  server->backend = wlr_backend_autocreate(server->display, NULL);
  if (server->backend == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_backend");
    return 1;
  }
  server->renderer = wlr_renderer_autocreate(server->backend);
  if (server->renderer == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_renderer");
    return 1;
  }

  wlr_renderer_init_wl_display(server->renderer, server->display);
  wlr_renderer_init_wl_display(server->renderer, server->display);

  server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
  if(server->allocator == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_allocator");
    return 1;
  }

  wlr_compositor_create(server->display, 5, server->renderer);
  wlr_subcompositor_create(server->display);
  wlr_data_device_manager_create(server->display);

  server->outputLayout = wlr_output_layout_create();

  wl_list_init(&server->outputs);
  ATTACH(server, server->backend->events.new_output, newOutput);

  wl_list_init(&server->views);
  server->xdgShell = wlr_xdg_shell_create(server->display, 3);
  ATTACH(server, server->xdgShell->events.new_surface, newXdgSurface);

  server->cursor = wlr_cursor_create();
  wlr_cursor_attach_output_layout(server->cursor, server->outputLayout);
  ATTACH(server, server->cursor->events.motion, cursorMotion);
  ATTACH(server, server->cursor->events.motion_absolute, cursorMotionAbsolute);
  ATTACH(server, server->cursor->events.button, cursorButton);
  ATTACH(server, server->cursor->events.axis, cursorAxis);
  ATTACH(server, server->backend->events.new_input, newInput);

  server->seat = wlr_seat_create(server->display, "seat0");

  const char *socket = wl_display_add_socket_auto(server->display);
  if (!socket) {
    wlr_backend_destroy(server->backend);
    return 1;
  }

  if (!wlr_backend_start(server->backend)) {
    wlr_backend_destroy(server->backend);
    wl_display_destroy(server->display);
    return 1;
  }

  setenv("WAYLAND_DISPLAY", socket, true);

  wl_signal_init(&server->resize);

  ATTACH(server, server->resize, resizeHandler);

  wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
  wl_display_run(server->display);

  return 0;
}
