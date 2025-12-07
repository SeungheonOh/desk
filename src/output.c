#include "output.h"
#include "layer.h"
#include "aux.h"
#include <math.h>

static void renderLayer(struct Output *output, struct wl_list *layer_list, float *depth);

struct Output *mkOutput(struct DeskServer *container, struct wlr_output* data){
  struct Output *output = calloc(1, sizeof(struct Output));
  output->server = container;
  output->wlr_output = data;
  output->shader_initialized = false;
  output->windowShader = NULL;
  output->frame_pending = false;
  output->needs_full_damage = true;

  wlr_damage_ring_init(&output->damage_ring);
  pixman_region32_init(&output->prev_damage);

  for (int i = 0; i < 4; i++) {
    wl_list_init(&output->layers[i]);
  }

  data->data = output;

  ATTACH(Output, output, data->events.frame, frame);
  ATTACH(Output, output, data->events.present, present);
  ATTACH(Output, output, data->events.request_state, requestState);
  ATTACH(Output, output, data->events.destroy, destroy);

  wl_list_insert(&container->outputs, &output->link);

  return output;
}

void destroyOutput(struct Output *container){
  wlr_damage_ring_finish(&container->damage_ring);
  pixman_region32_fini(&container->prev_damage);
  wl_list_remove(&container->frame.link);
  wl_list_remove(&container->present.link);
  wl_list_remove(&container->requestState.link);
  wl_list_remove(&container->destroy.link);
  wl_list_remove(&container->link);
  free(container);
}

void damageOutputWhole(struct Output *output) {
  struct wlr_box box = {
    .x = 0,
    .y = 0,
    .width = output->wlr_output->width,
    .height = output->wlr_output->height,
  };
  wlr_damage_ring_add_box(&output->damage_ring, &box);
  wlr_output_schedule_frame(output->wlr_output);
}

void damageOutputBox(struct Output *output, struct wlr_box *box) {
  wlr_damage_ring_add_box(&output->damage_ring, box);
  wlr_output_schedule_frame(output->wlr_output);
}

HANDLE(frame, void, Output) {
  if (container->frame_pending) {
    return;
  }

  int view_count = 0;
  struct View *v;
  wl_list_for_each(v, &container->server->views, link) {
    view_count++;
    if(v->fadeIn > 0) {
      v->fadeIn -= 0.1f;
      if(v->fadeIn > 0.0f) {
        damageOutputWhole(container);
      }
    }
  }
  static int once = 1;
  if(once && view_count == 0) {
    LOG("WARNING: No views to render");
    once = 0;
  }

  if (container->needs_full_damage) {
    damageOutputWhole(container);
    container->needs_full_damage = false;
  }
  


  struct wlr_output_state state;
  wlr_output_state_init(&state);

  container->pass = wlr_output_begin_render_pass(container->wlr_output, &state, NULL);
  if (!container->pass) {
    wlr_output_state_finish(&state);
    return;
  }

  int output_width = container->wlr_output->width;
  int output_height = container->wlr_output->height;
  
  /* Capture current frame damage for debug display before modifications */
  pixman_region32_t debug_damage;
  pixman_region32_init(&debug_damage);
  if (container->server->debugDamage) {
    pixman_region32_copy(&debug_damage, &container->damage_ring.current);
    /* Force full redraw when debug mode is on to avoid double-buffer artifacts */
    struct wlr_box full = { 0, 0, output_width, output_height };
    wlr_damage_ring_add_box(&container->damage_ring, &full);
  }
  
  /* Accumulate current + previous frame damage for double buffering */
  pixman_region32_t accumulated_damage;
  pixman_region32_init(&accumulated_damage);
  pixman_region32_union(&accumulated_damage, &container->damage_ring.current, &container->prev_damage);
  
  /* Save current damage for next frame, then clear */
  pixman_region32_copy(&container->prev_damage, &container->damage_ring.current);
  pixman_region32_clear(&container->damage_ring.current);
  
  /* Get individual damage rectangles for efficient scissoring */
  int num_rects = 0;
  pixman_box32_t *damage_rects = pixman_region32_rectangles(&accumulated_damage, &num_rects);
  
  /* If no damage, skip rendering entirely */
  if (num_rects == 0) {
    pixman_region32_fini(&accumulated_damage);
    wlr_render_pass_submit(container->pass);
    wlr_output_commit_state(container->wlr_output, &state);
    wlr_output_state_finish(&state);
    return;
  }

  /* Initialize shader on first frame when GL context is available (within render pass) */
  if (!container->shader_initialized) {
   container->windowShader = newShader(WINDOW_VERTEX_SHADER, WINDOW_FRAGMENT_SHADER);
   if (!container->windowShader) {
     LOG("Failed to load shader, skipping frame");
     wlr_output_state_finish(&state);
     return;
   }
   container->windowShaderExternal = newShader(WINDOW_VERTEX_SHADER, WINDOW_FRAGMENT_SHADER_EXTERNAL);
   if (!container->windowShaderExternal) {
     LOG("Failed to load external shader, skipping frame");
     wlr_output_state_finish(&state);
     return;
   }
   container->cursorShader = newShader(CURSOR_VERTEX_SHADER, CURSOR_FRAGMENT_SHADER);
   if (!container->cursorShader) {
     LOG("Failed to load cursor shader, skipping frame");
     wlr_output_state_finish(&state);
     return;
   }
   container->debugShader = newShader(DEBUG_VERTEX_SHADER, DEBUG_FRAGMENT_SHADER);
   if (!container->debugShader) {
     LOG("Failed to load debug shader, skipping frame");
     wlr_output_state_finish(&state);
     return;
   }
   GL_CHECK(glGenTextures(1, &container->uiTexture));
   
   /* Create screen texture for cursor effect */
   GL_CHECK(glGenTextures(1, &container->screenTexture));
   glBindTexture(GL_TEXTURE_2D, container->screenTexture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 
                container->wlr_output->width, container->wlr_output->height,
                0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
   
   container->shader_initialized = true;
   LOG("Shaders initialized successfully");
  }

  /* Begin GL rendering within the render pass */
  glEnable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  /* Render each damage rectangle separately for efficiency */
  for (int rect_idx = 0; rect_idx < num_rects; rect_idx++) {
    pixman_box32_t *rect = &damage_rects[rect_idx];
    
    int scissor_x = rect->x1;
    int scissor_y = rect->y1;
    int scissor_width = rect->x2 - rect->x1;
    int scissor_height = rect->y2 - rect->y1;
    
    /* Clamp to valid range */
    if (scissor_x < 0) scissor_x = 0;
    if (scissor_y < 0) scissor_y = 0;
    if (scissor_width <= 0 || scissor_height <= 0) continue;
    
    glScissor(scissor_x, scissor_y, scissor_width, scissor_height);
    
    glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    useShader(container->windowShader);
    
    /* Setup projection for orthographic view */
    mat4 proj = GLM_MAT4_IDENTITY_INIT;
    glm_ortho(0, container->wlr_output->width, 0, container->wlr_output->height, -10.0f, 10.0f, proj);
    set4fv(container->windowShader, "projection", 1, GL_FALSE, (float*)proj);
    
    mat4 view_mat = GLM_MAT4_IDENTITY_INIT;
    set4fv(container->windowShader, "view", 1, GL_FALSE, (float*)view_mat);

    /* Render in layer order: BACKGROUND -> BOTTOM -> views -> TOP -> OVERLAY */
    float depth = -9;

    /* BACKGROUND layer (layer 0) */
    renderLayer(container, &container->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], &depth);

    /* BOTTOM layer (layer 1) */
    renderLayer(container, &container->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &depth);

    /* Regular views (back-to-front: focused view is at front of list, should render last) */
    struct View *e;
    wl_list_for_each_reverse(e, &container->server->views, link) {
      if (!e->xdg || !e->xdg->surface || !e->xdg->surface->mapped) {
        depth++;
        continue;
      }

      struct RenderContext renderContext = {
        .output = container,
        .view = e,
        .offsetX = 0,
        .offsetY = 0,
        .depth = depth,
      };

      /* Iterate all surfaces in the xdg tree (toplevel + popups + subsurfaces) */
      wlr_xdg_surface_for_each_surface(e->xdg, renderSurfaceIter, &renderContext);
      depth++;
    }

    /* TOP layer (layer 2) */
    renderLayer(container, &container->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &depth);

    /* OVERLAY layer (layer 3) */
    renderLayer(container, &container->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &depth);

    /* Capture screen content for cursor effect (clamped to screen bounds) */
    int copy_x = scissor_x;
    int copy_y = scissor_y;
    int copy_w = scissor_width;
    int copy_h = scissor_height;
    if (copy_x + copy_w > output_width) copy_w = output_width - copy_x;
    if (copy_y + copy_h > output_height) copy_h = output_height - copy_y;
    if (copy_w > 0 && copy_h > 0) {
      glBindTexture(GL_TEXTURE_2D, container->screenTexture);
      glCopyTexSubImage2D(GL_TEXTURE_2D, 0, copy_x, copy_y, copy_x, copy_y, copy_w, copy_h);
    }

    /* Draw fancy cursor */
    useShader(container->cursorShader);
    
    float cursorX = container->server->cursor->x;
    float cursorY = container->server->cursor->y;
    float radius = 14.0f;
    
    glUniform2f(glGetUniformLocation(container->cursorShader->ID, "u_resolution"),
                (float)container->wlr_output->width, (float)container->wlr_output->height);
    glUniform2f(glGetUniformLocation(container->cursorShader->ID, "u_center"),
                cursorX, cursorY);
    glUniform1f(glGetUniformLocation(container->cursorShader->ID, "u_radius"), radius);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, container->screenTexture);
    glUniform1i(glGetUniformLocation(container->cursorShader->ID, "u_screen_texture"), 0);
    
    /* Draw cursor quad using immediate vertex data */
    GLfloat cursorVertices[] = {
      -1.0f, -1.0f,
       1.0f, -1.0f,
       1.0f,  1.0f,
      -1.0f, -1.0f,
       1.0f,  1.0f,
      -1.0f,  1.0f,
    };
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), cursorVertices);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    
  }
  
  /* Debug: draw damage region overlay after all rendering */
  if (container->server->debugDamage) {
    int debug_num_rects = 0;
    pixman_box32_t *debug_rects = pixman_region32_rectangles(&debug_damage, &debug_num_rects);
    
    if (debug_num_rects > 0) {
      useShader(container->debugShader);
      glUniform4f(glGetUniformLocation(container->debugShader->ID, "u_color"), 1.0f, 0.0f, 0.0f, 0.5f);
      
      for (int rect_idx = 0; rect_idx < debug_num_rects; rect_idx++) {
        pixman_box32_t *rect = &debug_rects[rect_idx];
        
        int scissor_x = rect->x1;
        int scissor_y = rect->y1;
        int scissor_width = rect->x2 - rect->x1;
        int scissor_height = rect->y2 - rect->y1;
        
        if (scissor_x < 0) scissor_x = 0;
        if (scissor_y < 0) scissor_y = 0;
        if (scissor_width <= 0 || scissor_height <= 0) continue;
        
        glScissor(scissor_x, scissor_y, scissor_width, scissor_height);
        
        GLfloat debugVertices[] = {
          -1.0f, -1.0f,
           1.0f, -1.0f,
           1.0f,  1.0f,
          -1.0f, -1.0f,
           1.0f,  1.0f,
          -1.0f,  1.0f,
        };
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), debugVertices);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(0);
      }
    }
  }
  
  pixman_region32_fini(&debug_damage);
  pixman_region32_fini(&accumulated_damage);
  
  /* Disable scissor after all rendering is complete */
  glDisable(GL_SCISSOR_TEST);

  if (!wlr_render_pass_submit(container->pass)) {
    LOG("Failed to submit render pass");
    wlr_output_state_finish(&state);
    return;
  }

  container->frame_pending = true;
  if (!wlr_output_commit_state(container->wlr_output, &state)) {
    container->frame_pending = false;
    wlr_output_schedule_frame(container->wlr_output);
  }

  wlr_output_state_finish(&state);
}

HANDLE(present, struct wlr_output_event_present, Output) {
  container->frame_pending = false;
  /* Only schedule next frame if there's pending damage */
  if (!pixman_region32_not_empty(&container->damage_ring.current)) {
    return;
  }
  wlr_output_schedule_frame(container->wlr_output);
}

HANDLE(requestState, struct wlr_output_event_request_state, Output) {
  LOG ("State request");
  wlr_output_commit_state(container->wlr_output, data->state);
}

HANDLE(destroy, struct wlr_output, Output) {
  LOG ("Destroying %s", data->name);

  destroyOutput(container);
}

void printPoint(struct point p) {
  LOG("%.1f %.1f", p.x, p.y);
}

// wlr_surface_iterator_func_t
void renderSurfaceIter(struct wlr_surface *surface, int x, int y, void *data) {
  struct RenderContext *ctx = (struct RenderContext*)data;

  struct wlr_texture *texture = wlr_surface_get_texture(surface);
  if (!texture) {
    goto frame_done;
  }


  int width = surface->current.width;
  int height = surface->current.height;

  /* Get the underlying GL texture from wlroots */
  struct wlr_gles2_texture_attribs attribs;
  wlr_gles2_texture_get_attribs(texture, &attribs);

  /* Select shader based on texture target */
  struct shader *shader = (attribs.target == GL_TEXTURE_EXTERNAL_OES) 
    ? ctx->output->windowShaderExternal 
    : ctx->output->windowShader;
  useShader(shader);

  /* Setup projection for orthographic view */
  mat4 proj = GLM_MAT4_IDENTITY_INIT;
  glm_ortho(0, ctx->output->wlr_output->width, 0, ctx->output->wlr_output->height, -10.0f, 10.0f, proj);
  set4fv(shader, "projection", 1, GL_FALSE, (float*)proj);
  
  mat4 view = GLM_MAT4_IDENTITY_INIT;
  set4fv(shader, "view", 1, GL_FALSE, (float*)view);

  /* Setup model matrix with rotation */
  mat4 model = GLM_MAT4_IDENTITY_INIT;
  
  /* Get view's main surface extents for rotation center */
  struct wlr_box view_box = {0};
  wlr_surface_get_extents(ctx->view->xdg->surface, &view_box);
  
  /* Rotation pivot is the view's center (main surface center) */
  float pivot_x = ctx->view->x + view_box.width / 2.0f;
  float pivot_y = ctx->view->y + view_box.height / 2.0f;
  
  /* This surface's position relative to view origin */
  float surface_x = ctx->view->x + x;
  float surface_y = ctx->view->y + y;
  
  /* Translate to pivot, rotate, translate back, then position surface */
  glm_translate(model, (vec3){pivot_x, pivot_y, ctx->depth});
  if (ctx->view->rot != 0.0f) {
    glm_rotate_z(model, ctx->view->rot, model);
  }
  /* Translate from pivot to surface position */
  glm_translate(model, (vec3){surface_x - pivot_x, surface_y - pivot_y, 0});

  set4fv(shader, "model", 1, GL_FALSE, (float*)model);

  /* Bind texture and render quad */
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(attribs.target, attribs.tex);
  
  /* Set texture parameters - important for external textures */
  glTexParameteri(attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(attribs.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(attribs.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  
  glUniform1i(glGetUniformLocation(shader->ID, "s_texture"), 0);

  /* Vertex data for quad */
  GLfloat vVertices[] = {
    0,  0, 0.0f,    0.0f,  0.0f,
    0, height, 0.0f, 0.0f,  1.0f,
    width, height, 0.0f, 1.0f,  1.0f,
    width,  0, 0.0f, 1.0f,  0.0f
  };
  GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vVertices);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3]);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

frame_done:
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  wlr_surface_send_frame_done(surface, &now);
}

void renderUI(struct Output *output) {

}

void renderLayerSurfaceIter(struct wlr_surface *surface, int x, int y, void *data) {
  struct LayerRenderContext *ctx = (struct LayerRenderContext*)data;

  struct wlr_texture *texture = wlr_surface_get_texture(surface);
  if (!texture) {
    goto frame_done;
  }

  int width = surface->current.width;
  int height = surface->current.height;

  struct wlr_gles2_texture_attribs attribs;
  wlr_gles2_texture_get_attribs(texture, &attribs);

  struct shader *shader = (attribs.target == GL_TEXTURE_EXTERNAL_OES) 
    ? ctx->output->windowShaderExternal 
    : ctx->output->windowShader;
  useShader(shader);

  mat4 proj = GLM_MAT4_IDENTITY_INIT;
  glm_ortho(0, ctx->output->wlr_output->width, 0, ctx->output->wlr_output->height, -10.0f, 10.0f, proj);
  set4fv(shader, "projection", 1, GL_FALSE, (float*)proj);
  
  mat4 view = GLM_MAT4_IDENTITY_INIT;
  set4fv(shader, "view", 1, GL_FALSE, (float*)view);

  mat4 model = GLM_MAT4_IDENTITY_INIT;
  float surface_x = ctx->x + x;
  float surface_y = ctx->y + y;
  glm_translate(model, (vec3){surface_x, surface_y, ctx->depth});
  set4fv(shader, "model", 1, GL_FALSE, (float*)model);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(attribs.target, attribs.tex);
  
  glTexParameteri(attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(attribs.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(attribs.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  
  glUniform1i(glGetUniformLocation(shader->ID, "s_texture"), 0);

  GLfloat vVertices[] = {
    0,  0, 0.0f,    0.0f,  0.0f,
    0, height, 0.0f, 0.0f,  1.0f,
    width, height, 0.0f, 1.0f,  1.0f,
    width,  0, 0.0f, 1.0f,  0.0f
  };
  GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vVertices);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3]);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

frame_done:
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  wlr_surface_send_frame_done(surface, &now);
}

static void renderLayer(struct Output *output, struct wl_list *layer_list, float *depth) {
  struct LayerSurface *ls;
  wl_list_for_each(ls, layer_list, link) {
    if (!ls->mapped || !ls->layer_surface->surface->mapped) {
      continue;
    }

    struct LayerRenderContext ctx = {
      .output = output,
      .x = ls->x,
      .y = ls->y,
      .depth = *depth,
    };

    wlr_surface_for_each_surface(ls->layer_surface->surface, 
                                  renderLayerSurfaceIter, &ctx);
    (*depth)++;
  }
}
