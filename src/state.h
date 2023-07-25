#pragma once
#include "imports.h"

struct DeskServer {
  struct wl_display *display;
  struct wlr_backend *backend;
  struct wlr_renderer *renderer;
  struct wlr_allocator *allocator;

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

  // Render
  unsigned int shaderProgram;
  GLint samplerLoc;
  GLint modelLoc;
  GLint viewLoc;
  GLint projectionLoc;

  int initialized;
};

struct Keyboard {
  struct DeskServer *server;
  struct wl_list link;

  struct wlr_keyboard *wlr_keyboard;

  struct wl_listener modifiers;
  struct wl_listener key;
  struct wl_listener destroy;
};
