#pragma once
#include "imports.h"
#include "events.h"

struct DeskServer;
struct Output;

typedef struct LayerSurface {
  struct DeskServer *server;
  struct Output *output;
  struct wlr_layer_surface_v1 *layer_surface;
  struct wl_list link;

  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener destroy;
  struct wl_listener commit;
  struct wl_listener new_popup;

  bool mapped;
  int x, y;
  uint32_t layer;
} LayerSurface;

struct LayerSurface *mkLayerSurface(struct DeskServer *server, 
                                     struct wlr_layer_surface_v1 *wlr_layer_surface);
void destroyLayerSurface(struct LayerSurface *layer_surface);
void arrangeLayerSurfaces(struct Output *output);
void focusLayerSurface(struct LayerSurface *layer_surface);
struct LayerSurface *layerSurfaceAt(struct DeskServer *server, double lx, double ly,
                                     struct wlr_surface **surface, double *sx, double *sy);

LISTNER(map, void, LayerSurface)
LISTNER(unmap, void, LayerSurface)
LISTNER(destroy, void, LayerSurface)
LISTNER(commit, struct wlr_surface, LayerSurface)
LISTNER(new_popup, struct wlr_xdg_popup, LayerSurface)
