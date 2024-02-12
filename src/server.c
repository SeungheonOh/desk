#include "server.h"

struct DeskServer *newServer() {
  wlr_log_init(WLR_DEBUG, NULL);

  struct DeskServer *server = malloc(sizeof(struct DeskServer));
  ASSERTN(server);

  server->display = wl_display_create();
  ASSERTN(server->backend = wlr_backend_autocreate(server->display, NULL));

  ASSERTN(server->renderer = wlr_renderer_autocreate(server->backend));

  wlr_renderer_init_wl_display(server->renderer, server->display);
  wlr_renderer_init_wl_display(server->renderer, server->display);

  ASSERTN(server->allocator = wlr_allocator_autocreate(server->backend, server->renderer));

  wlr_compositor_create(server->display, 5, server->renderer);
  wlr_subcompositor_create(server->display);
  wlr_data_device_manager_create(server->display);

  server->outputLayout = wlr_output_layout_create();

  wl_list_init(&server->outputs);
  ATTACH(DeskServer, server, server->backend->events.new_output, newOutput);

  wl_list_init(&server->views);
  server->xdgShell = wlr_xdg_shell_create(server->display, 3);
  ATTACH(DeskServer, server, server->xdgShell->events.new_surface, newXdgSurface);

  wl_list_init(&server->keyboards);

  server->cursor = wlr_cursor_create();
  wlr_cursor_attach_output_layout(server->cursor, server->outputLayout);
  ATTACH(DeskServer, server, server->cursor->events.motion, cursorMotion);
  ATTACH(DeskServer, server, server->cursor->events.motion_absolute, cursorMotionAbsolute);
  ATTACH(DeskServer, server, server->cursor->events.button, cursorButton);
  ATTACH(DeskServer, server, server->cursor->events.axis, cursorAxis);
  ATTACH(DeskServer, server, server->backend->events.new_input, newInput);

  server->seat = wlr_seat_create(server->display, "seat0");

  ASSERTN(server->socket = wl_display_add_socket_auto(server->display));

  wl_signal_init(&server->resize);

  ATTACH(DeskServer, server, server->resize, resizeHandler);

  server->foo = 0;

  return server;
}

void startServer(struct DeskServer *server) {
  wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", server->socket);
  setenv("WAYLAND_DISPLAY", server->socket, true);

  ASSERTN(wlr_backend_start(server->backend));
  wl_display_run(server->display);
}

void destroyServer(struct DeskServer *server) {
  ASSERTN(server);

  wlr_backend_destroy(server->backend);
  wl_display_destroy(server->display);
}

HANDLE(newXdgSurface, struct wlr_xdg_surface, DeskServer){
  LOG("new XDG surface");
  mkView(container, data);
}
HANDLE(newInput, struct wlr_input_device, DeskServer){
  switch (data->type) {
  case WLR_INPUT_DEVICE_KEYBOARD:
    struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(data);
    ASSERTN(wlr_keyboard);
    struct Keyboard *keyboard =
      malloc(sizeof(struct Keyboard));
    ASSERTN(keyboard);

    keyboard->server = container;
    keyboard->wlr_keyboard = wlr_keyboard;

    struct xkb_context *context =
      xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap =
      xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(wlr_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

    ATTACH(Keyboard, keyboard, wlr_keyboard->events.modifiers, modifiers);
    ATTACH(Keyboard, keyboard, wlr_keyboard->events.key, key);
    ATTACH(Keyboard, keyboard, data->events.destroy, destroy);

    wlr_seat_set_keyboard(container->seat, keyboard->wlr_keyboard);

    wl_list_insert(&container->keyboards, &keyboard->link);

    break;
  case WLR_INPUT_DEVICE_POINTER:
    wlr_cursor_attach_input_device(container->cursor, data);
    break;
  default:
    break;
  }
}
HANDLE(requestCursor, struct wlr_seat_pointer_request_set_cursor_event, DeskServer){
}
HANDLE(requestSetSelection, struct wlr_seat_request_set_selection_event, DeskServer){
}
HANDLE(cursorMotion, struct wlr_pointer_motion_event, DeskServer){
}
HANDLE(cursorMotionAbsolute, struct wlr_pointer_motion_event, DeskServer){
}
HANDLE(cursorButton, struct wlr_pointer_button_event, DeskServer){
}
HANDLE(cursorAxis, struct wlr_pointer_axis_event, DeskServer){
  int *param = malloc(sizeof(int));
  static int counter = 0;
  if(data->orientation == WLR_AXIS_ORIENTATION_VERTICAL)
    counter -= data->delta;
  else
    counter += data->delta;  
  
  *param = counter;

  wl_signal_emit_mutable(&container->resize, param);

  free(param);
}

HANDLE(cursorFrame, void, DeskServer){
}

HANDLE(newOutput, struct wlr_output, DeskServer){
  wlr_output_init_render(data, container->allocator, container->renderer);

  /* The output may be disabled, switch it on. */
  struct wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_enabled(&state, true);

  struct wlr_output_mode *mode = wlr_output_preferred_mode(data);
  if (mode != NULL) {
    wlr_output_state_set_mode(&state, mode);
  }

  wlr_output_commit_state(data, &state);
  wlr_output_state_finish(&state);
  wlr_output_layout_add_auto(container->outputLayout, data);
  
  mkOutput(container, data);
}


HANDLE(resizeHandler, int, DeskServer) {
  LOG("a %d", *data);

  container->foo = *data * 0.1;
}
