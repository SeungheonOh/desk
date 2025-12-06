#pragma once
#include "output.h"
#include "keyboard.h"
#include "events.h"
#include "shader.h"

struct SurfaceTracker {
  struct wl_listener commit;
  struct wl_listener destroy;
  struct DeskServer *server;
};

typedef struct DeskServer {
  struct wl_display *display;
  struct wlr_backend *backend;
  struct wlr_renderer *renderer;
  struct wlr_allocator *allocator;
  struct wlr_compositor *compositor;
  const char *socket;

  // xdg shell
  struct wlr_xdg_shell *xdgShell;
  struct wl_listener newXdgSurface;
  struct wl_listener newXdgToplevel;
  struct wl_listener newXdgPopup;
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

  // Surface commit tracking (for damage control)
  struct wl_listener newSurface;

  // test
  struct wl_signal resize;
  struct wl_listener resizeHandler;

  float foo;
  float bar;

  int initialized;

  float x,y;
  float sx, sy;

  int rotationMode;
  
  struct View *focused_view;

  // Window move state
  bool superPressed;
  bool moveMode;
  struct View *grabbed_view;
  double grab_x, grab_y;  // cursor position at grab start
  int grab_view_x, grab_view_y;  // view position at grab start
  
  // Animation loop
  struct wl_event_source *animation_timer;
  
  // Debug mode
  bool debugDamage;
} DeskServer;

struct DeskServer *newServer();
void startServer(struct DeskServer*);
void destroyServer(struct DeskServer*);
void scheduleRedraw(struct DeskServer*);
void damageWholeServer(struct DeskServer*);

LISTNER(newXdgSurface, struct wlr_xdg_surface, DeskServer);
LISTNER(newXdgToplevel, struct wlr_xdg_toplevel, DeskServer);
LISTNER(newXdgPopup, struct wlr_xdg_popup, DeskServer);
LISTNER(newInput, struct wlr_input_device, DeskServer);
LISTNER(requestCursor, struct wlr_seat_pointer_request_set_cursor_event, DeskServer);
LISTNER(requestSetSelection, struct wlr_seat_request_set_selection_event, DeskServer);
LISTNER(cursorMotion, struct wlr_pointer_motion_event, DeskServer);
LISTNER(cursorMotionAbsolute, struct wlr_pointer_motion_absolute_event, DeskServer);
LISTNER(cursorButton, struct wlr_pointer_button_event, DeskServer);
LISTNER(cursorAxis, struct wlr_pointer_axis_event, DeskServer);
LISTNER(cursorFrame, void, DeskServer);
LISTNER(newOutput, struct wlr_output, DeskServer);
LISTNER(newSurface, struct wlr_surface, DeskServer);

LISTNER(resizeHandler, int, DeskServer);

/* LISTNER(modifiers, void, struct Keyboard); */
/* LISTNER(key, void, struct Keyboard); */
/* LISTNER(destroy, struct wlr_keyboard_key_event, struct Keyboard); */
