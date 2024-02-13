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

  glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  useShader(container->windowShader);
  
  mat4 proj = GLM_MAT4_IDENTITY_INIT;
  mat4 view = GLM_MAT4_IDENTITY_INIT;
  mat4 trans = GLM_MAT4_IDENTITY_INIT;    
  glm_ortho(0, container->wlr_output->width, 0, container->wlr_output->height, 0.00001f, 10000.0f, proj);
  //glm_ortho(-1, 1, -1, 1, 0.00001f, 10000.0f, proj);  

  set4fv(container->windowShader, "projection", 1, GL_FALSE, (float*)proj);
  set4fv(container->windowShader, "view", 1, GL_FALSE, (float*)view);
  set4fv(container->windowShader, "model", 1, GL_FALSE, (float*)trans);  
  
  
  /* struct View *e; */
  /* wl_list_for_each(e, &container->server->views, link) { */
  /*   //LOG("at: %d %d", e->x, e->y); */
    
  /*   struct RenderContext renderContext = { */
  /*     .output = container, */
  /*     .view = e, */
  /*   }; */
      
  /*   wlr_surface_for_each_surface(e->xdgToplevel->base->surface, renderSurfaceIter, &renderContext); */
  /* }     */

  float n = -0.5f, p = 0.5f;
  GLfloat vVertices[] = { 0,  0, 0.0f,  // Position 0
			  0.0f,  1.0f,        // TexCoord 0

			  0, 500, 0.0f,  // Position 1
			  0.0f,  0.0f,        // TexCoord 1

			  500, 500, 0.0f,  // Position 2
			  1.0f,  0.0f,        // TexCoord 2

			  500,  0, 0.0f,  // Position 3
			  1.0f,  1.0f         // TexCoord 3
  };
  GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

  setFloat(container->windowShader, "time", container->server->foo);

  glVertexAttribPointer ( 0, 3, GL_FLOAT,
			  GL_FALSE, 5 * sizeof ( GLfloat ), vVertices );

  glVertexAttribPointer ( 1, 2, GL_FLOAT,
			  GL_FALSE, 5 * sizeof ( GLfloat ), &vVertices[3] );

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  //glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  wlr_renderer_end(container->server->renderer);
  wlr_output_commit(container->wlr_output);
}

HANDLE(requestState, struct wlr_output, Output) {
  LOG ("State request");
}

HANDLE(destroy, struct wlr_output, Output) {
  LOG ("Destroying %s", data->name);

  destroyOutput(container);
}

// wlr_surface_iterator_func_t
void renderSurfaceIter(struct wlr_surface *surface, int x, int y, void *data) {
  struct RenderContext *ctx = (struct RenderContext*)data;
  LOG("%d %d %d %d", x, y, surface->current.width, surface->current.height);

  mat4 trans = GLM_MAT4_IDENTITY_INIT;    

  //glm_translate(trans, (float[3]){(float)box.x, (float)box.y, 0.0f});
  glm_translate(trans, (float[3]){(float)surface->current.width, (float)surface->current.height , 0.0f});
  //glm_translate(trans, (float[3]){(float)box.width/2, (float)box.height/2 , 0.0f});

  //glm_rotate_at(trans, (float[3]){0, 0, 0.0f}, rot, GLM_ZUP);
  glm_translate(trans, (float[3]){x, y, 0.0f});
  //glm_translate(trans, (float[3]){-(float)box.width/2, -(float)box.height/2 , 0.0f});

  glm_scale(trans, (float[3]) {(float)surface->current.width, (float)surface->current.height, 1.0f});

  set4fv(ctx->output->windowShader, "model", 1, GL_FALSE, trans);
    

  GLfloat vVertices[] = {
    0,  500, 0.0f,  // Position 0
    0.0f,  1.0f,  // TexCoord 0

    0,  0, 0.0f,  // Position 1
    0.0f,  0.0f,  // TexCoord 1

    500,  0, 0.0f,  // Position 2
    1.0f,  0.0f,  // TexCoord 2

    500,  500, 0.0f,  // Position 3
    1.0f,  1.0f   // TexCoord 3
  };
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
  
}
