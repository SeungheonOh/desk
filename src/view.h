#pragma once
#include "imports.h"
#include "server.h"
#include "events.h"
#include <time.h>
#include <math.h>

/*
  Basically a window
 */
typedef struct View {
  struct DeskServer *server;
  struct wl_list link;
  struct wlr_xdg_toplevel *xdgToplevel;
  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener destroy;
  struct wl_listener requestMove;
  struct wl_listener requestResize;
  struct wl_listener requestMaximize;
  struct wl_listener requestFullscreen;

  int x, y;
  float fadeIn;
  float rot;
  float scale;
} View;

struct View *mkView(struct DeskServer*, struct wlr_xdg_surface*);
void destroyView(struct View *);

struct point centerPoint(struct View);

LISTNER(map, void, View)
LISTNER(unmap, void, View)
LISTNER(destroy, void, View)
LISTNER(requestMove, void, View)
LISTNER(requestResize, void, View)
LISTNER(requestMaximize, void, View)
LISTNER(requestFullscreen, void, View)

