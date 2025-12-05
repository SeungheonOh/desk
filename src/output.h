#pragma once
#include "imports.h"
#include "server.h"
#include "events.h"
#include "config.h"
#include "view.h"
#include <time.h>
#include <math.h>

typedef struct Output {
  struct DeskServer *server;
  struct wl_list link;
  struct wlr_output *wlr_output;
  struct wl_listener frame;
  struct wl_listener present;
  struct wl_listener requestState;
  struct wl_listener destroy;

  struct shader *windowShader;
  struct shader *windowShaderExternal;
  struct shader *cursorShader;
  struct wlr_render_pass *pass;
  bool shader_initialized;
  bool frame_pending;

  GLuint uiTexture;
  GLuint screenTexture;
} Output;

struct Output *mkOutput(struct DeskServer *,struct wlr_output*);
void destroyOutput(struct Output *);

LISTNER(frame, void, Output);
LISTNER(present, struct wlr_output_event_present, Output);
LISTNER(requestState, struct wlr_output_event_request_state, Output);
LISTNER(destroy, struct wlr_output, Output);

struct RenderContext {
  struct Output *output;
  struct View *view;
  int width, height, offsetX, offsetY;
  float depth;
  cairo_t *uiCtx;
};

void renderSurfaceIter(struct wlr_surface *, int, int, void *);
