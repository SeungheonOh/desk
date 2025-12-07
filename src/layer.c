#include "layer.h"
#include "server.h"
#include "output.h"

static void damageLayerSurface(struct LayerSurface *ls) {
  if (!ls || !ls->output || !ls->layer_surface->surface) {
    return;
  }

  struct wlr_box box;
  wlr_surface_get_extents(ls->layer_surface->surface, &box);
  box.x += ls->x;
  box.y += ls->y;

  damageOutputBox(ls->output, &box);
}

struct LayerSurface *mkLayerSurface(struct DeskServer *server,
                                     struct wlr_layer_surface_v1 *wlr_layer_surface) {
  struct LayerSurface *layer_surface = calloc(1, sizeof(struct LayerSurface));
  ASSERTN(layer_surface);

  layer_surface->server = server;
  layer_surface->layer_surface = wlr_layer_surface;
  layer_surface->mapped = false;
  layer_surface->layer = wlr_layer_surface->pending.layer;

  struct Output *output = NULL;
  if (wlr_layer_surface->output) {
    output = wlr_layer_surface->output->data;
  } else {
    struct Output *o;
    wl_list_for_each(o, &server->outputs, link) {
      output = o;
      break;
    }
    if (output) {
      wlr_layer_surface->output = output->wlr_output;
    }
  }

  if (!output) {
    wlr_layer_surface_v1_destroy(wlr_layer_surface);
    free(layer_surface);
    return NULL;
  }

  layer_surface->output = output;
  wlr_layer_surface->data = layer_surface;

  ATTACH(LayerSurface, layer_surface, wlr_layer_surface->surface->events.map, map);
  ATTACH(LayerSurface, layer_surface, wlr_layer_surface->surface->events.unmap, unmap);
  ATTACH(LayerSurface, layer_surface, wlr_layer_surface->surface->events.commit, commit);
  ATTACH(LayerSurface, layer_surface, wlr_layer_surface->events.destroy, destroy);
  ATTACH(LayerSurface, layer_surface, wlr_layer_surface->events.new_popup, new_popup);

  wl_list_insert(&output->layers[layer_surface->layer], &layer_surface->link);

  return layer_surface;
}

void destroyLayerSurface(struct LayerSurface *layer_surface) {
  if (!layer_surface) return;

  wl_list_remove(&layer_surface->map.link);
  wl_list_remove(&layer_surface->unmap.link);
  wl_list_remove(&layer_surface->destroy.link);
  wl_list_remove(&layer_surface->commit.link);
  wl_list_remove(&layer_surface->new_popup.link);
  wl_list_remove(&layer_surface->link);

  free(layer_surface);
}

void arrangeLayerSurfaces(struct Output *output) {
  if (!output || !output->wlr_output) return;

  int output_width = output->wlr_output->width;
  int output_height = output->wlr_output->height;

  struct wlr_box usable_area = {
    .x = 0,
    .y = 0,
    .width = output_width,
    .height = output_height,
  };

  for (int layer_idx = 0; layer_idx < 4; layer_idx++) {
    struct LayerSurface *ls;
    wl_list_for_each(ls, &output->layers[layer_idx], link) {
      struct wlr_layer_surface_v1 *wlr_ls = ls->layer_surface;
      struct wlr_layer_surface_v1_state *state = &wlr_ls->current;

      struct wlr_box bounds = usable_area;
      if (state->exclusive_zone > 0) {
        bounds = usable_area;
      }

      int width = state->desired_width;
      int height = state->desired_height;

      if (width == 0) {
        width = bounds.width;
        if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT &&
            state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
          width = bounds.width - state->margin.left - state->margin.right;
        }
      }
      if (height == 0) {
        height = bounds.height;
        if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP &&
            state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
          height = bounds.height - state->margin.top - state->margin.bottom;
        }
      }

      int x = bounds.x;
      int y = bounds.y;

      if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT &&
          state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
        x = bounds.x + (bounds.width - width) / 2;
      } else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
        x = bounds.x + state->margin.left;
      } else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
        x = bounds.x + bounds.width - width - state->margin.right;
      } else {
        x = bounds.x + (bounds.width - width) / 2;
      }

      if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP &&
          state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
        y = bounds.y + (bounds.height - height) / 2;
      } else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
        y = bounds.y + state->margin.top;
      } else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
        y = bounds.y + bounds.height - height - state->margin.bottom;
      } else {
        y = bounds.y + (bounds.height - height) / 2;
      }

      ls->x = x;
      ls->y = y;

      if (state->exclusive_zone > 0) {
        if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP &&
            !(state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
          usable_area.y += state->exclusive_zone + state->margin.top;
          usable_area.height -= state->exclusive_zone + state->margin.top;
        } else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM &&
                   !(state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
          usable_area.height -= state->exclusive_zone + state->margin.bottom;
        } else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT &&
                   !(state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
          usable_area.x += state->exclusive_zone + state->margin.left;
          usable_area.width -= state->exclusive_zone + state->margin.left;
        } else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT &&
                   !(state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
          usable_area.width -= state->exclusive_zone + state->margin.right;
        }
      }

      if (width > 0 && height > 0) {
        wlr_layer_surface_v1_configure(wlr_ls, width, height);
      }
    }
  }
}

HANDLE(map, void, LayerSurface) {
  LOG("Layer surface mapped");
  container->mapped = true;
  
  struct wlr_layer_surface_v1_state *state = &container->layer_surface->current;
  if (state->keyboard_interactive) {
    focusLayerSurface(container);
  }
  
  damageLayerSurface(container);
}

HANDLE(unmap, void, LayerSurface) {
  LOG("Layer surface unmapped");
  damageLayerSurface(container);
  container->mapped = false;
  
  struct Output *output = container->output;
  if (output) {
    arrangeLayerSurfaces(output);
  }
}

HANDLE(destroy, void, LayerSurface) {
  LOG("Layer surface destroyed");
  damageLayerSurface(container);
  struct Output *output = container->output;
  destroyLayerSurface(container);
  if (output) {
    arrangeLayerSurfaces(output);
  }
}

HANDLE(commit, struct wlr_surface, LayerSurface) {
  struct wlr_layer_surface_v1 *wlr_ls = container->layer_surface;

  if (!wlr_ls->initialized) {
    return;
  }

  uint32_t layer = wlr_ls->current.layer;
  if (layer != container->layer) {
    damageLayerSurface(container);
    wl_list_remove(&container->link);
    container->layer = layer;
    wl_list_insert(&container->output->layers[layer], &container->link);
  }

  arrangeLayerSurfaces(container->output);
  damageLayerSurface(container);
}

HANDLE(new_popup, struct wlr_xdg_popup, LayerSurface) {
  LOG("Layer surface popup created");
}

void focusLayerSurface(struct LayerSurface *layer_surface) {
  if (!layer_surface || !layer_surface->layer_surface->surface) {
    return;
  }

  struct DeskServer *server = layer_surface->server;
  struct wlr_seat *seat = server->seat;
  struct wlr_surface *surface = layer_surface->layer_surface->surface;

  struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
  if (keyboard) {
    wlr_seat_keyboard_notify_enter(seat, surface,
      keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
  }
}

struct LayerSurface *layerSurfaceAt(struct DeskServer *server, double lx, double ly,
                                     struct wlr_surface **surface, double *sx, double *sy) {
  struct Output *output;
  wl_list_for_each(output, &server->outputs, link) {
    for (int layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY; layer >= 0; layer--) {
      struct LayerSurface *ls;
      wl_list_for_each(ls, &output->layers[layer], link) {
        if (!ls->mapped || !ls->layer_surface->surface->mapped) {
          continue;
        }

        double local_x = lx - ls->x;
        double local_y = ly - ls->y;

        double _sx, _sy;
        struct wlr_surface *_surface = wlr_layer_surface_v1_surface_at(
          ls->layer_surface, local_x, local_y, &_sx, &_sy);

        if (_surface) {
          if (sx) *sx = _sx;
          if (sy) *sy = _sy;
          if (surface) *surface = _surface;
          return ls;
        }
      }
    }
  }
  return NULL;
}
