#pragma once
#include "output.h"
#include "keyboard.h"
#include "events.h"
#include "shader.h"

typedef struct DeskServer {
  struct wl_display *display;
  struct wlr_backend *backend;
  struct wlr_renderer *renderer;
  struct wlr_allocator *allocator;
  const char *socket;

  // xdg shell
  struct wlr_xdg_shell *xdgShell;
  struct wl_listener newXdgSurface;
  struct wl_list views;

  // Input
  struct wlr_seat *seat;
  struct wl_listener newInput;
  struct wl_listener requestCursor;
  struct wl_listener requestSetSelection;

  // Mouse
  struct wlr_cursor *cursor;
  struct wl_listener cursorMotion;
  struct wl_listener cursorMotionAbsolute;
  struct wl_listener cursorButton;
  struct wl_listener cursorAxis;
  struct wl_listener cursorFrame;

  // Keyboard
  struct wl_list keyboards;

  // Output
  struct wlr_output_layout *outputLayout;
  struct wl_list outputs;
  struct wl_listener newOutput;

  // test
  struct wl_signal resize;
  struct wl_listener resizeHandler;

  float foo;

  int initialized;
} DeskServer;

struct DeskServer *newServer();
void startServer(struct DeskServer*);
void destroyServer(struct DeskServer*);

LISTNER(newXdgSurface, struct wlr_xdg_surface, DeskServer);
LISTNER(newInput, struct wlr_input_device, DeskServer);
LISTNER(requestCursor, struct wlr_seat_pointer_request_set_cursor_event, DeskServer);
LISTNER(requestSetSelection, struct wlr_seat_request_set_selection_event, DeskServer);
LISTNER(cursorMotion, struct wlr_pointer_motion_event, DeskServer);
LISTNER(cursorMotionAbsolute, struct wlr_pointer_motion_event, DeskServer);
LISTNER(cursorButton, struct wlr_pointer_button_event, DeskServer);
LISTNER(cursorAxis, struct wlr_pointer_axis_event, DeskServer);
LISTNER(cursorFrame, void, DeskServer);
LISTNER(newOutput, struct wlr_output, DeskServer);

LISTNER(resizeHandler, int, DeskServer);

/* LISTNER(modifiers, void, struct Keyboard); */
/* LISTNER(key, void, struct Keyboard); */
/* LISTNER(destroy, struct wlr_keyboard_key_event, struct Keyboard); */
