#include "view.h"

struct View *mkView(struct DeskServer *container, struct wlr_xdg_surface *data){
  if(data->role == WLR_XDG_SURFACE_ROLE_POPUP) return NULL;

  struct View *view = calloc(1, sizeof(struct View));
  view->server = container;
  view->xdgToplevel = data->toplevel;

  ATTACH(View, view, data->surface->events.map, map);
  ATTACH(View, view, data->surface->events.unmap, unmap);
  ATTACH(View, view, data->surface->events.destroy, destroy);
  ATTACH(View, view, data->surface->events.map, map);

  struct wlr_seat *seat = container->seat;
  struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
  wlr_xdg_toplevel_set_activated(view->xdgToplevel, true);

  wlr_seat_keyboard_notify_enter(seat, view->xdgToplevel->base->surface,
				 keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
  wlr_seat_pointer_notify_enter(seat, view->xdgToplevel->base->surface, 25, 25);

  view->fadeIn = 1;

  srand(time(NULL));
  view->rot = (float)rand()/(float)(RAND_MAX/(2*PI));

  LOG("ROT:%f", view->rot);

  return view;
}
void destroyView(struct View *container){
}

HANDLE(map, void, View) {
  LOG("MAPMAPMAMPM");
  wl_list_insert(&container->server->views, &container->link);
  //wlr_xdg_toplevel_set_size(container->xdgToplevel, 720, 480);
}
HANDLE(unmap, void, View) {
  LOG("UNMAMAMMANNNNNNN");
  wl_list_remove(&container->link);
  destroyView(container);
}
HANDLE(destroy, void, View) {
}
HANDLE(requestMove, void, View) {
}
HANDLE(requestResize, void, View) {
}
HANDLE(requestMaximize, void, View) {
}
HANDLE(requestFullscreen, void, View) {
}
