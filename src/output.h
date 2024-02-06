#pragma once
#include "imports.h"
#include "server.h"
#include "events.h"
#include <time.h>
#include <math.h>

typedef struct Output {
  struct DeskServer *server;
  struct wl_list link;
  struct wlr_output *wlr_output;
  struct wl_listener frame;
  struct wl_listener requestState;
  struct wl_listener destroy;

  struct shader *windowShader;
} Output;

struct Output *mkOutput(struct DeskServer *,struct wlr_output*);
void destroyOutput(struct Output *);

LISTNER(frame, void, Output);
LISTNER(requestState, struct wlr_output, Output);
LISTNER(destroy, struct wlr_output, Output);
