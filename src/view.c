#include "view.h"

struct View *mkView(struct DeskServer *container, struct wlr_xdg_surface *data){
  if (!data || !data->surface) {
    return NULL;
  }

  struct View *view = calloc(1, sizeof(struct View));
  view->server = container;
  view->xdg = data;

  ATTACH(View, view, data->surface->events.map, map);
  ATTACH(View, view, data->surface->events.unmap, unmap);
  ATTACH(View, view, data->surface->events.destroy, destroy);
  ATTACH(View, view, data->surface->events.commit, commit);

  if (data->toplevel) {
    ATTACH(View, view, data->toplevel->events.request_move, requestMove);
  }

  view->fadeIn = 1;
  view->xdg->data = view;
  view->needs_configure = true;
  
  // Initialize smooth movement and rotation
  view->vel_x = 0.0f;
  view->vel_y = 0.0f;
  view->target_x = 0.0f;
  view->target_y = 0.0f;
  view->rot_vel = 0.0f;
  view->target_rot = 0.0f;
  view->dampening = 0.35f;  // Friction: higher = smoother/laggier

  return view;
}
void destroyView(struct View *view){
  if (!view) return;
  
  /* Clear focus if this view was focused */
  if (view->server->focused_view == view) {
    view->server->focused_view = NULL;
  }
  
  /* Remove listeners */
  wl_list_remove(&view->map.link);
  wl_list_remove(&view->unmap.link);
  wl_list_remove(&view->destroy.link);
  wl_list_remove(&view->commit.link);
  if (view->xdg && view->xdg->toplevel) {
    wl_list_remove(&view->requestMove.link);
  }
  
  free(view);
}

void focusView(struct View *view, struct wlr_surface *surface) {
  if (!view) return;
  
  struct DeskServer *server = view->server;
  struct wlr_seat *seat = server->seat;
  struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
  
  if (prev_surface == surface) {
    return; /* Already focused */
  }
  
  /* Deactivate previous toplevel */
  if (prev_surface) {
    struct wlr_xdg_toplevel *prev_toplevel = 
      wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
    if (prev_toplevel) {
      wlr_xdg_toplevel_set_activated(prev_toplevel, false);
    }
  }
  
  /* Move view to front of list (top of stack) */
  wl_list_remove(&view->link);
  wl_list_insert(&server->views, &view->link);
  
  /* Activate new toplevel */
  if (view->xdg->toplevel) {
    wlr_xdg_toplevel_set_activated(view->xdg->toplevel, true);
  }
  
  /* Send keyboard focus */
  struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
  if (keyboard) {
    wlr_seat_keyboard_notify_enter(seat, view->xdg->surface,
      keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
  }
  
  server->focused_view = view;
}

struct View *viewAt(struct DeskServer *server, double lx, double ly,
                    struct wlr_surface **surface, double *sx, double *sy) {
  struct View *view;
  wl_list_for_each(view, &server->views, link) {
    if (!view->xdg || !view->xdg->surface || !view->xdg->surface->mapped) {
      continue;
    }
    
    /* Get surface dimensions for center calculation */
    struct wlr_box box = {0};
    wlr_surface_get_extents(view->xdg->surface, &box);
    
    /* Calculate view center in layout coordinates */
    double cx = view->x + box.width / 2.0;
    double cy = view->y + box.height / 2.0;
    
    /* Transform cursor position by INVERSE rotation around view center.
     * 
     * Rendering applies rotation R(θ) to surface points.
     * To find surface coords from screen coords, apply R(-θ).
     * 
     * R(-θ) = [cos(θ)   sin(θ)]
     *         [-sin(θ)  cos(θ)]
     */
    double dx = lx - cx;
    double dy = ly - cy;
    
    double cos_r = cos(view->rot);
    double sin_r = sin(view->rot);
    
    /* Inverse rotation: R(-θ) * (dx, dy) */
    double view_sx = (dx * cos_r + dy * sin_r) + box.width / 2.0;
    double view_sy = (-dx * sin_r + dy * cos_r) + box.height / 2.0;
    
    double _sx, _sy;
    struct wlr_surface *_surface = 
      wlr_xdg_surface_surface_at(view->xdg, view_sx, view_sy, &_sx, &_sy);
    
    if (_surface) {
      if (sx) *sx = _sx;
      if (sy) *sy = _sy;
      if (surface) *surface = _surface;
      return view;
    }
  }
  return NULL;
}

struct point centerPoint(struct View v) {
  struct point center = {0, 0};
  if (!v.xdg || !v.xdg->surface) {
    return center;
  }

  struct wlr_box surfaceBox = {0};
  wlr_surface_get_extents(v.xdg->surface, &surfaceBox);

  center.x = v.x + surfaceBox.width / 2.0f;
  center.y = v.y + surfaceBox.height / 2.0f;

  return center;
}

HANDLE(map, void, View) {
  LOG("View mapped");
  wl_list_insert(&container->server->views, &container->link);
  
  /* Focus the new view */
  focusView(container, container->xdg->surface);
}
HANDLE(unmap, void, View) {
  LOG("UNMAMAMMANNNNNNN");
  wl_list_remove(&container->link);
  destroyView(container);
}
HANDLE(destroy, void, View) {
}
HANDLE(requestMove, void, View) {
  struct DeskServer *server = container->server;
  
  server->moveMode = true;
  server->grabbed_view = container;
  server->grab_x = server->cursor->x;
  server->grab_y = server->cursor->y;
  server->grab_view_x = container->x;
  server->grab_view_y = container->y;
  
  wl_list_remove(&container->link);
  wl_list_insert(&server->views, &container->link);
}
HANDLE(requestResize, void, View) {
}
HANDLE(requestMaximize, void, View) {
}
HANDLE(requestFullscreen, void, View) {
}
HANDLE(commit, struct wlr_surface, View) {
  if (!container->xdg) return;

  if (!container->needs_configure || !container->xdg->initialized) {
    return;
  }

  if (container->xdg->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL &&
      container->xdg->toplevel) {
    wlr_xdg_toplevel_set_size(container->xdg->toplevel, 0, 0);
    container->needs_configure = false;
    return;
  }

  if (container->xdg->role == WLR_XDG_SURFACE_ROLE_POPUP) {
    wlr_xdg_surface_schedule_configure(container->xdg);
    container->needs_configure = false;
    return;
  }
}
