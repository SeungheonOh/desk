#include "output.h"

struct Output *mkOutput(struct DeskServer *container, struct wlr_output* data){
  struct Output *output = malloc(sizeof(struct Output));
  output->server = container;
  output->wlr_output = data;

  ATTACH(Output, output, data->events.frame, frame);
  ATTACH(Output, output, data->events.request_state, requestState);
  ATTACH(Output, output, data->events.destroy, destroy);

  output->windowShader = newShader(WINDOW_VERTEX_SHADER, WINDOW_FRAGMENT_SHADER);
  ASSERT(output->windowShader, "Shader failed to load");

  wl_list_insert(&container->outputs, &output->link);

  GL_CHECK(glGenTextures(1, &output->uiTexture));

  return output;
}

void destroyOutput(struct Output *container){
  wl_list_remove(&container->frame.link);
  wl_list_remove(&container->requestState.link);
  wl_list_remove(&container->destroy.link);
  wl_list_remove(&container->link);
  free(container);
}

HANDLE(frame, void, Output) {
  //if(!wl_list_empty(&container->server->views)) LOG("nothing to render");

  wlr_output_attach_render(container->wlr_output, NULL);
  wlr_renderer_begin(container->server->renderer, container->wlr_output->width, container->wlr_output->height);

  glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  useShader(container->windowShader);

  mat4 proj = GLM_MAT4_IDENTITY_INIT;
  mat4 view = GLM_MAT4_IDENTITY_INIT;
  mat4 trans = GLM_MAT4_IDENTITY_INIT;
  glm_ortho(0, container->wlr_output->width, 0, container->wlr_output->height, 0.00001f, 10000.0f, proj);
  //glm_ortho(-1, 1, -1, 1, 0.00001f, 10000.0f, proj);

  set4fv(container->windowShader, "projection", 1, GL_FALSE, (float*)proj);
  set4fv(container->windowShader, "view", 1, GL_FALSE, (float*)view);
  setFloat(container->windowShader, "time", container->server->foo);

  static float foo = 0;
  foo++;
  struct View *e;
  wl_list_for_each(e, &container->server->views, link) {
#define ABS(a) (a>0? a:a*-1)
    //LOG("at: %d %d", e->x, e->y);

    struct wlr_box surfaceBox;
    wlr_surface_get_extends(e->xdgToplevel->base->surface, &surfaceBox);

    LOG("BOUNDS: %d %d %d %d", surfaceBox.x, surfaceBox.y, surfaceBox.width, surfaceBox.height);


    struct RenderContext renderContext = {
      .output = container,
      .view = e,
    };

    e->scale = 1; // sin(foo * 0.03) + 1;
    setFloat(container->windowShader, "time", e->x * e->y * 0.001);

    wlr_surface_for_each_surface(e->xdgToplevel->base->surface, renderSurfaceIter, &renderContext);
  }

  cairo_surface_t *ui =
    cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			       container->wlr_output->width,
			       container->wlr_output->height);
  if(!ui) EXPLODE("Failed to create caior surface");

  cairo_t *uiCtx = cairo_create(ui);
  cairo_text_extents_t extents;

  const char *utf8 = "cairo";

  cairo_set_source_rgba(uiCtx, 1, 1, 1, 0);
  cairo_paint(uiCtx);

  cairo_select_font_face (uiCtx, "Sans",
    CAIRO_FONT_SLANT_NORMAL,
    CAIRO_FONT_WEIGHT_NORMAL);

  cairo_set_source_rgba(uiCtx, 0, 0, 0, 1);
  cairo_set_font_size (uiCtx, 100.0);
  cairo_text_extents (uiCtx, utf8, &extents);

  double x=500;
  double y=600;

  cairo_move_to (uiCtx, x,y);
  cairo_show_text (uiCtx, utf8);


  x=25.6,  y=128.0;
  double x1=102.4, y1=230.4,
    x2=153.6, y2=25.6,
    x3=230.4, y3=128.0;

  cairo_move_to (uiCtx, x, y);
  cairo_curve_to (uiCtx, x1, y1, x2, y2, x3, y3);

  cairo_set_line_width (uiCtx, 10.0);
  cairo_stroke (uiCtx);

  cairo_set_source_rgba (uiCtx, 1, 0.2, 0.2, 0.6);
  cairo_set_line_width (uiCtx, 6.0);
  cairo_move_to (uiCtx,x,y);   cairo_line_to (uiCtx,x1,y1);
  cairo_move_to (uiCtx,x2,y2); cairo_line_to (uiCtx,x3,y3);
  cairo_stroke (uiCtx);

  cairo_set_source_rgba (uiCtx, 0, 0, 0, 1);
  cairo_move_to(uiCtx, 0, 0);
  cairo_line_to (uiCtx,0, container->wlr_output->height);
  cairo_line_to (uiCtx,container->wlr_output->width, container->wlr_output->height);
  cairo_line_to (uiCtx,container->wlr_output->width, 0);
  cairo_line_to (uiCtx,0, 0);
  cairo_set_line_width (uiCtx, 10.0);
  cairo_stroke (uiCtx);

  cairo_new_path(uiCtx);  
  cairo_set_source_rgba (uiCtx, 0, 0, 0, 0.8);    
  cairo_arc (uiCtx, container->server->cursor->x, container->server->cursor->y, 1.0, 0, 2*M_PI);
  cairo_stroke (uiCtx);

  if(container->server->sx != -1 && container->server->sy != -1) {
    cairo_set_source_rgba (uiCtx, 1, 0, 0, 0.3);        
    cairo_line_to(uiCtx, container->server->sx, container->server->sy);
    cairo_line_to(uiCtx, container->server->sx, container->server->cursor->y);
    cairo_line_to(uiCtx, container->server->cursor->x, container->server->cursor->y);
    cairo_line_to(uiCtx, container->server->cursor->x, container->server->sy);
    cairo_fill(uiCtx);
  }


  cairo_surface_flush(ui);

  glBindTexture(GL_TEXTURE_2D, container->uiTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,
	       0,
	       GL_RGBA,
	       container->wlr_output->width,
	       container->wlr_output->height,
	       0,
	       GL_RGBA,
	       GL_UNSIGNED_BYTE,
	       cairo_image_surface_get_data(ui)
	       );

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, container->uiTexture);
  glUniform1i(glGetUniformLocation(container->windowShader->ID, "s_texture"), 0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  /* wlr_renderer_end(container->server->renderer); */
  /* wlr_output_commit(container->wlr_output); */
  /* return; */

  set4fv(container->windowShader, "model", 1, GL_FALSE, (float*)trans);

  float n = -0.5f, p = 0.5f;
  /*
   P0     P3
   *------*
   |      |
   *------*
   P1     P2
   */
  GLfloat vVertices[] = {
    0,  0, 0.0f,  // Position 0
    0.0f,  0.0f,        // TexCoord 0

    0, container->wlr_output->height, 0.0f,  // Position 1
    0.0f,  1.0f,        // TexCoord 1

    container->wlr_output->width, container->wlr_output->height, 0.0f,  // Position 2
    1.0f,  1.0f,        // TexCoord 2

    container->wlr_output->width,  0, 0.0f,  // Position 3
    1.0f,  0.0f         // TexCoord 3
  };
  GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

  glVertexAttribPointer ( 0, 3, GL_FLOAT,
			  GL_FALSE, 5 * sizeof ( GLfloat ), vVertices );

  glVertexAttribPointer ( 1, 2, GL_FLOAT,
			  GL_FALSE, 5 * sizeof ( GLfloat ), &vVertices[3] );

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  wlr_renderer_end(container->server->renderer);
  wlr_output_commit(container->wlr_output);
}

HANDLE(requestState, struct wlr_output_event_request_state, Output) {
  LOG ("State request");
  wlr_output_commit_state(container->wlr_output, data->state);
}

HANDLE(destroy, struct wlr_output, Output) {
  LOG ("Destroying %s", data->name);

  destroyOutput(container);
}

// wlr_surface_iterator_func_t
void renderSurfaceIter(struct wlr_surface *surface, int x, int y, void *data) {
  struct RenderContext *ctx = (struct RenderContext*)data;
  //LOG("%d %d %d %d", x, y, surface->current.width, surface->current.height);

  mat4 trans = GLM_MAT4_IDENTITY_INIT;

  //glm_translate(trans, (float[3]){(float)box.x, (float)box.y, 0.0f});
  //glm_translate(trans, (float[3]){(float)surface->current.width, (float)surface->current.height , 0.0f});
  //glm_translate(trans, (float[3]){(float)box.width/2, (float)box.height/2 , 0.0f});

  //glm_rotate_at(trans, (float[3]){0, 0, 0.0f}, rot, GLM_ZUP);

  //glm_translate(trans, (float[3]){-(float)box.width/2, -(float)box.height/2 , 0.0f});

  //glm_scale(trans, (float[3]) {(float)surface->current.width, (float)surface->current.height, 1.0f});


  float width = (float)surface->current.width * ctx->view->scale;
  float height = (float)surface->current.height * ctx->view->scale;

  float halfWidth = width / 2;
  float halfHeight = height / 2;


  static float rot = 0;
  rot += PI/1000;
  glm_translate(trans, (float[3]){x, y, 0.0f});
  glm_translate(trans, (float[3]){(float)ctx->view->x+halfWidth, (float)ctx->view->y+halfHeight, 0.0f});
  glm_rotate_at(trans, (float[3]){0, 0, 0.0f}, rot, GLM_ZUP);

  set4fv(ctx->output->windowShader, "model", 1, GL_FALSE, trans);


  GLfloat vVertices[] = {
    -halfWidth,  halfHeight, 0.0f,  // Position 0
    0.0f,  1.0f,  // TexCoord 0

    -halfWidth,  -halfHeight, 0.0f,  // Position 1
    0.0f,  0.0f,  // TexCoord 1

    halfWidth,  -halfHeight, 0.0f,  // Position 2
    1.0f,  0.0f,  // TexCoord 2

    halfWidth,  halfHeight, 0.0f,  // Position 3
    1.0f,  1.0f   // TexCoord 3
  };
  /* GLfloat vVertices[] = { */
  /*   0,  height, 0.0f,  // Position 0 */
  /*   0.0f,  1.0f,  // TexCoord 0 */

  /*   0,  0, 0.0f,  // Position 1 */
  /*   0.0f,  0.0f,  // TexCoord 1 */

  /*   width,  0, 0.0f,  // Position 2 */
  /*   1.0f,  0.0f,  // TexCoord 2 */

  /*   width,  height, 0.0f,  // Position 3 */
  /*   1.0f,  1.0f   // TexCoord 3 */
  /* }; */
  GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

  glVertexAttribPointer ( 0, 3, GL_FLOAT,
			  GL_FALSE, 5 * sizeof ( GLfloat ), vVertices );

  glVertexAttribPointer ( 1, 2, GL_FLOAT,
			  GL_FALSE, 5 * sizeof ( GLfloat ), &vVertices[3] );

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  struct wlr_texture *stexture = wlr_surface_get_texture(surface);
  struct wlr_gles2_texture_attribs attrs;
  wlr_gles2_texture_get_attribs(stexture, &attrs);

  GLint appTexture = attrs.tex;
  GLenum appTextureTarget = attrs.target;

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(appTextureTarget, appTexture);

  glTexParameteri(appTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(appTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glUniform1i(glGetUniformLocation(ctx->output->windowShader->ID, "s_texture"), 0);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  wlr_surface_send_frame_done(surface, &now);
}

void renderUI(struct Output *output) {
}
