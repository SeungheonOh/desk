#include "server.h"
#include <math.h>

static void getViewDamageBox(struct View *view, struct wlr_box *box);
static void damageView(struct DeskServer *server, struct View *view);

/* Animation frame callback - updates smooth movement and rotation */
static int animationFrame(void *data) {
  struct DeskServer *server = (struct DeskServer *)data;
  
  struct View *view;
  wl_list_for_each(view, &server->views, link) {
    if (!view->xdg || !view->xdg->surface) continue;
    
    /* Update position with velocity - spring physics */
    float dx = view->target_x - view->x;
    float dy = view->target_y - view->y;
    
    /* Accelerate towards target */
    float stiffness = 0.3f;
    view->vel_x += dx * stiffness;
    view->vel_y += dy * stiffness;
    
    /* Dampen velocity (friction) */
    view->vel_x *= view->dampening;
    view->vel_y *= view->dampening;
    
    /* Apply velocity */
    if (fabs(view->vel_x) > 0.1f || fabs(view->vel_y) > 0.1f) {
      damageView(server, view);
      view->x += view->vel_x;
      view->y += view->vel_y;
      damageView(server, view);
    } else if (fabs(dx) > 0.5f || fabs(dy) > 0.5f) {
      damageView(server, view);
      view->x = view->target_x;
      view->y = view->target_y;
      view->vel_x = view->vel_y = 0;
      damageView(server, view);
    }
    
    /* Update rotation with velocity - spring physics */
    float d_rot = view->target_rot - view->rot;
    while (d_rot > M_PI) d_rot -= 2 * M_PI;
    while (d_rot < -M_PI) d_rot += 2 * M_PI;
    
    view->rot_vel += d_rot * stiffness;
    view->rot_vel *= view->dampening;
    
    if (fabs(view->rot_vel) > 0.001f) {
      damageView(server, view);
      view->rot += view->rot_vel;
      damageView(server, view);
    } else if (fabs(d_rot) > 0.01f) {
      damageView(server, view);
      view->rot = view->target_rot;
      view->rot_vel = 0;
      damageView(server, view);
    }
  }
  
  /* Reschedule timer for next frame */
  wl_event_source_timer_update(server->animation_timer, 16);
  
  return 0;
}

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

  server->compositor = wlr_compositor_create(server->display, 5, server->renderer);
  wlr_subcompositor_create(server->display);
  wlr_data_device_manager_create(server->display);

  ATTACH(DeskServer, server, server->compositor->events.new_surface, newSurface);

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
  server->animation_timer = NULL;

  return server;
}

void startServer(struct DeskServer *server) {
  wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", server->socket);
  setenv("WAYLAND_DISPLAY", server->socket, true);

  /* Start animation timer (~60 FPS) */
  struct wl_event_loop *loop = wl_display_get_event_loop(server->display);
  server->animation_timer = wl_event_loop_add_timer(loop, animationFrame, server);
  wl_event_source_timer_update(server->animation_timer, 16);  // 16ms = ~60 FPS

  ASSERTN(wlr_backend_start(server->backend));
  wl_display_run(server->display);
}

void destroyServer(struct DeskServer *server) {
  ASSERTN(server);

  wlr_backend_destroy(server->backend);
  wl_display_destroy(server->display);
}

void scheduleRedraw(struct DeskServer *server) {
  struct Output *output;
  wl_list_for_each(output, &server->outputs, link) {
    wlr_output_schedule_frame(output->wlr_output);
  }
}

void damageWholeServer(struct DeskServer *server) {
  struct Output *output;
  wl_list_for_each(output, &server->outputs, link) {
    damageOutputWhole(output);
  }
}

static void getViewDamageBox(struct View *view, struct wlr_box *box) {
  if (!view->xdg || !view->xdg->surface) {
    *box = (struct wlr_box){0, 0, 0, 0};
    return;
  }

  struct wlr_box surface_box;
  wlr_surface_get_extents(view->xdg->surface, &surface_box);

  float cx = view->x + surface_box.width / 2.0f;
  float cy = view->y + surface_box.height / 2.0f;
  float hw = surface_box.width / 2.0f;
  float hh = surface_box.height / 2.0f;

  float cos_r = cosf(view->rot);
  float sin_r = sinf(view->rot);

  float corners[4][2] = {
    {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
  };

  float min_x = cx, max_x = cx, min_y = cy, max_y = cy;
  for (int i = 0; i < 4; i++) {
    float rx = corners[i][0] * cos_r - corners[i][1] * sin_r + cx;
    float ry = corners[i][0] * sin_r + corners[i][1] * cos_r + cy;
    if (rx < min_x) min_x = rx;
    if (rx > max_x) max_x = rx;
    if (ry < min_y) min_y = ry;
    if (ry > max_y) max_y = ry;
  }

  box->x = (int)floorf(min_x) - 1;
  box->y = (int)floorf(min_y) - 1;
  box->width = (int)ceilf(max_x - min_x) + 2;
  box->height = (int)ceilf(max_y - min_y) + 2;
}

static void damageView(struct DeskServer *server, struct View *view) {
  struct wlr_box box;
  getViewDamageBox(view, &box);
  
  struct Output *output;
  wl_list_for_each(output, &server->outputs, link) {
    damageOutputBox(output, &box);
  }
}

static void damageAllOutputs(struct DeskServer *server) {
  struct Output *output;
  wl_list_for_each(output, &server->outputs, link) {
    damageOutputWhole(output);
  }
}

static void damageCursor(struct DeskServer *server, double x, double y) {
  int radius = 20;
  struct wlr_box box = {
    .x = (int)x - radius,
    .y = (int)y - radius,
    .width = radius * 2,
    .height = radius * 2,
  };
  struct Output *output;
  wl_list_for_each(output, &server->outputs, link) {
    damageOutputBox(output, &box);
  }
}

static void surfaceCommitHandler(struct wl_listener *listener, void *data) {
  struct SurfaceTracker *tracker = wl_container_of(listener, tracker, commit);
  struct wlr_surface *surface = data;
  
  struct wlr_surface *root = wlr_surface_get_root_surface(surface);
  
  struct View *view;
  wl_list_for_each(view, &tracker->server->views, link) {
    if (view->xdg && view->xdg->surface == root) {
      damageView(tracker->server, view);
      return;
    }
  }
  
  damageAllOutputs(tracker->server);
}

static void surfaceDestroyHandler(struct wl_listener *listener, void *data) {
  struct SurfaceTracker *tracker = wl_container_of(listener, tracker, destroy);
  wl_list_remove(&tracker->commit.link);
  wl_list_remove(&tracker->destroy.link);
  free(tracker);
}

HANDLE(newSurface, struct wlr_surface, DeskServer) {
  struct SurfaceTracker *tracker = calloc(1, sizeof(struct SurfaceTracker));
  tracker->server = container;
  
  tracker->commit.notify = surfaceCommitHandler;
  wl_signal_add(&data->events.commit, &tracker->commit);
  
  tracker->destroy.notify = surfaceDestroyHandler;
  wl_signal_add(&data->events.destroy, &tracker->destroy);
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
      view->target_x = 100;
      view->target_y = 100;
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
    struct xkb_rule_names rules = {
      .rules = NULL,
      .model = "pc105",
      .layout = "us",
      .variant = "colemak_dh",
      .options = NULL,
    };
    struct xkb_keymap *keymap =
      xkb_keymap_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

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
  damageCursor(container, container->cursor->x, container->cursor->y);
  wlr_cursor_move(container->cursor, &data->pointer->base,
		  data->delta_x, data->delta_y);
  damageCursor(container, container->cursor->x, container->cursor->y);
  
  /* Update window position if in move mode */
  if (container->moveMode && container->grabbed_view) {
    double dx = container->cursor->x - container->grab_x;
    double dy = container->cursor->y - container->grab_y;
    /* Set target position for smooth movement */
    container->grabbed_view->target_x = container->grab_view_x + (int)dx;
    container->grabbed_view->target_y = container->grab_view_y + (int)dy;
  } else {
    processCursorMotion(container, data->time_msec);
  }
  wlr_seat_pointer_notify_frame(container->seat);
}
HANDLE(cursorMotionAbsolute, struct wlr_pointer_motion_absolute_event, DeskServer){
  damageCursor(container, container->cursor->x, container->cursor->y);
  wlr_cursor_warp_absolute(container->cursor, &data->pointer->base, data->x, data->y);
  damageCursor(container, container->cursor->x, container->cursor->y);
  
  /* Update window position if in move mode */
  if (container->moveMode && container->grabbed_view) {
    double dx = container->cursor->x - container->grab_x;
    double dy = container->cursor->y - container->grab_y;
    /* Set target position for smooth movement */
    container->grabbed_view->target_x = container->grab_view_x + (int)dx;
    container->grabbed_view->target_y = container->grab_view_y + (int)dy;
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
  wlr_output_schedule_frame(data);
}


HANDLE(resizeHandler, int, DeskServer) {
  if(container->rotationMode) {
    /* Find view under cursor (rotation-aware) and rotate it */
    struct View *view = viewAt(container, container->cursor->x, 
                                container->cursor->y, NULL, NULL, NULL);
    if (view) {
      /* Set target rotation for smooth rotation */
      view->target_rot += *data * (PI / 1000);
      
      /* Re-send motion event with updated surface-local coords after rotation */
      processCursorMotion(container, 0);
      wlr_seat_pointer_notify_frame(container->seat);
    }
  } else {
    container->foo += *data * 0.01;
  }
}
