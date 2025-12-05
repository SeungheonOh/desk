#include "server.h"

struct DeskServer *newServer() {
  wlr_log_init(WLR_DEBUG, NULL);

  struct DeskServer *server = malloc(sizeof(struct DeskServer));
  ASSERTN(server);

  server->display = wl_display_create();
  ASSERTN(server->backend = wlr_backend_autocreate(
            wl_display_get_event_loop(server->display), NULL));

  ASSERTN(server->renderer = wlr_renderer_autocreate(server->backend));

  wlr_renderer_init_wl_display(server->renderer, server->display);
  wlr_renderer_init_wl_display(server->renderer, server->display);

  ASSERTN(server->allocator = wlr_allocator_autocreate(server->backend, server->renderer));

  wlr_compositor_create(server->display, 5, server->renderer);
  wlr_subcompositor_create(server->display);
  wlr_data_device_manager_create(server->display);

  server->outputLayout = wlr_output_layout_create(server->display);

  wl_list_init(&server->outputs);
  ATTACH(DeskServer, server, server->backend->events.new_output, newOutput);

  wl_list_init(&server->views);
  server->xdgShell = wlr_xdg_shell_create(server->display, 3);
  ATTACH(DeskServer, server, server->xdgShell->events.new_surface, newXdgSurface);
  ATTACH(DeskServer, server, server->xdgShell->events.new_toplevel, newXdgToplevel);
  ATTACH(DeskServer, server, server->xdgShell->events.new_popup, newXdgPopup);

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
  server->bar = 0;  
  server->x = 0;
  server->y = 0;
  server->sx = -1;
  server->sy = -1;
  server->rotationMode = 0;
  server->focused_view = NULL;
  server->superPressed = false;
  server->moveMode = false;
  server->grabbed_view = NULL;

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
}

HANDLE(newXdgToplevel, struct wlr_xdg_toplevel, DeskServer){
  struct wlr_xdg_surface *xdg_surface = data->base;
  struct View *view = xdg_surface ? xdg_surface->data : NULL;
  if (!view && xdg_surface) {
    view = mkView(container, xdg_surface);
    if (view) {
      view->x = 100;
      view->y = 100;
      view->scale = 1;
    } else {
      LOG("Ignoring xdg surface without a wl_surface");
    }
  }
}

HANDLE(newXdgPopup, struct wlr_xdg_popup, DeskServer){
}
HANDLE(newInput, struct wlr_input_device, DeskServer){
  switch (data->type) {
  case WLR_INPUT_DEVICE_KEYBOARD:
    struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(data);
    struct Keyboard *keyboard =
      malloc(sizeof(struct Keyboard));

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

  uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
  if (!wl_list_empty(&container->keyboards)) {
    caps |= WL_SEAT_CAPABILITY_KEYBOARD;
  }
  wlr_seat_set_capabilities(container->seat, caps);
}
HANDLE(requestCursor, struct wlr_seat_pointer_request_set_cursor_event, DeskServer){
}
HANDLE(requestSetSelection, struct wlr_seat_request_set_selection_event, DeskServer){
  LOG("asrta");
}
static void processCursorMotion(struct DeskServer *server, uint32_t time) {
  double sx, sy;
  struct wlr_surface *surface = NULL;
  struct View *view = viewAt(server, server->cursor->x, server->cursor->y, 
                              &surface, &sx, &sy);
  
  if (!view) {
    /* No view under cursor, clear pointer focus */
    wlr_seat_pointer_clear_focus(server->seat);
  } else {
    /* Send pointer enter/motion to the surface */
    wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);
  }
}

HANDLE(cursorMotion, struct wlr_pointer_motion_event, DeskServer){
  wlr_cursor_move(container->cursor, &data->pointer->base,
		  data->delta_x, data->delta_y);
  
  /* Update window position if in move mode */
  if (container->moveMode && container->grabbed_view) {
    double dx = container->cursor->x - container->grab_x;
    double dy = container->cursor->y - container->grab_y;
    container->grabbed_view->x = container->grab_view_x + (int)dx;
    container->grabbed_view->y = container->grab_view_y + (int)dy;
  } else {
    processCursorMotion(container, data->time_msec);
  }
  wlr_seat_pointer_notify_frame(container->seat);
}
HANDLE(cursorMotionAbsolute, struct wlr_pointer_motion_absolute_event, DeskServer){
  wlr_cursor_warp_absolute(container->cursor, &data->pointer->base, data->x, data->y);
  
  /* Update window position if in move mode */
  if (container->moveMode && container->grabbed_view) {
    double dx = container->cursor->x - container->grab_x;
    double dy = container->cursor->y - container->grab_y;
    container->grabbed_view->x = container->grab_view_x + (int)dx;
    container->grabbed_view->y = container->grab_view_y + (int)dy;
  } else {
    processCursorMotion(container, data->time_msec);
  }
  wlr_seat_pointer_notify_frame(container->seat);
}
HANDLE(cursorButton, struct wlr_pointer_button_event, DeskServer){
  /* Update motion before button to ensure coordinates are current */
  processCursorMotion(container, data->time_msec);
  
  if(data->button == BTN_LEFT && data->state == WLR_BUTTON_PRESSED) {
    LOG("SELECT");
    container->sx = container->cursor->x;
    container->sy = container->cursor->y;
    
    /* Focus view under cursor */
    double sx, sy;
    struct wlr_surface *surface = NULL;
    struct View *view = viewAt(container, container->cursor->x, 
                                container->cursor->y, &surface, &sx, &sy);
    if (view) {
      focusView(view, surface);
      
      /* Start move if Super is pressed */
      if (container->superPressed) {
        container->moveMode = true;
        container->grabbed_view = view;
        container->grab_x = container->cursor->x;
        container->grab_y = container->cursor->y;
        container->grab_view_x = view->x;
        container->grab_view_y = view->y;
        return;  // Don't send button to client during move
      }
    }
    
    /* Notify clients of button event (only if not moving) */
    wlr_seat_pointer_notify_button(container->seat, data->time_msec, 
                                    data->button, data->state);
  } else if(data->button == BTN_LEFT && data->state == WLR_BUTTON_RELEASED) {
    LOG("SELECT No");
    container->sx = -1;
    container->sy = -1;
    
    /* End move mode */
    if (container->moveMode) {
      container->moveMode = false;
      container->grabbed_view = NULL;
      return;  // Don't send release to client if we were moving
    }
    
    wlr_seat_pointer_notify_button(container->seat, data->time_msec, 
                                    data->button, data->state);
  } else {
    /* Other buttons - pass through */
    wlr_seat_pointer_notify_button(container->seat, data->time_msec, 
                                    data->button, data->state);
  }
}
HANDLE(cursorAxis, struct wlr_pointer_axis_event, DeskServer){
  /* If in rotation mode, handle internally */
  if (container->rotationMode) {
    int counter = 0;
    if(data->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL)
      counter -= data->delta;
    else
      counter += data->delta;
    
    int *param = malloc(sizeof(int));
    *param = counter;
    wl_signal_emit_mutable(&container->resize, param);
    free(param);
    return;
  }
  
  /* Forward scroll events to client */
  wlr_seat_pointer_notify_axis(container->seat, data->time_msec,
                                data->orientation, data->delta,
                                data->delta_discrete, data->source,
                                data->relative_direction);
}

HANDLE(cursorFrame, void, DeskServer){
  /* Frame events are now sent after each motion/button event */
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
  if(container->rotationMode) {
    /* Find view under cursor (rotation-aware) and rotate it */
    struct View *view = viewAt(container, container->cursor->x, 
                                container->cursor->y, NULL, NULL, NULL);
    if (view) {
      view->rot += *data * (PI / 1000);
      
      /* Re-send motion event with updated surface-local coords after rotation */
      processCursorMotion(container, 0);
      wlr_seat_pointer_notify_frame(container->seat);
    }
  } else {
    container->foo += *data * 0.01;
  }
}
