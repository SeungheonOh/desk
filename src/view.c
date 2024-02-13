#include "view.h"

struct View *mkView(struct DeskServer *container, struct wlr_xdg_surface *data){
  if(data->role == WLR_XDG_SURFACE_ROLE_POPUP) return NULL;

  struct View *view = calloc(1, sizeof(struct View));
  view->server = container;
  view->xdgToplevel = data->toplevel;

  ATTACH(View, view, data->surface->events.map, map);
  ATTACH(View, view, data->surface->events.unmap, unmap);
  ATTACH(View, view, data->surface->events.destroy, destroy);
  //  ATTACH(View, view, data->surface->events.map, map);
}
void destroyView(struct View *container){
}

HANDLE(map, void, View) {
  LOG("MAPMAPMAMPM");
  wl_list_insert(&container->server->views, &container->link);
  wlr_xdg_toplevel_set_size(container->xdgToplevel, 720, 720);
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
