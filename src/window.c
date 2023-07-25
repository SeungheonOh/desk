struct desk_window {
  struct wlr_xdg_toplevel *xdg_toplevel;
  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener destroy;
  struct wl_listener request_move;
  struct wl_listener request_resize;
  struct wl_listener request_maximize;
  struct wl_listener request_fullscreen;

  int x, y;
  float theta; // in radian
};
