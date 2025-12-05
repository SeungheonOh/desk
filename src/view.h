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
  struct wlr_xdg_surface *xdg;
  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener destroy;
  struct wl_listener requestMove;
  struct wl_listener requestResize;
  struct wl_listener requestMaximize;
  struct wl_listener requestFullscreen;
  struct wl_listener commit;
  bool needs_configure;

  float x, y;
  float fadeIn;
  float rot;
  float scale;
  
  // Smooth movement with velocity
  float vel_x, vel_y;
  float target_x, target_y;
  
  // Smooth rotation with velocity
  float rot_vel;
  float target_rot;
  
  // Dampening factor (0.0-1.0, lower = smoother)
  float dampening;
} View;

struct View *mkView(struct DeskServer*, struct wlr_xdg_surface*);
void destroyView(struct View *);
void focusView(struct View *, struct wlr_surface *surface);
struct View *viewAt(struct DeskServer *, double lx, double ly, 
                    struct wlr_surface **surface, double *sx, double *sy);

struct point centerPoint(struct View);

LISTNER(map, void, View)
LISTNER(unmap, void, View)
LISTNER(destroy, void, View)
LISTNER(requestMove, void, View)
LISTNER(requestResize, void, View)
LISTNER(requestMaximize, void, View)
LISTNER(requestFullscreen, void, View)
LISTNER(commit, struct wlr_surface, View)
