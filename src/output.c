#include "output.h"
#include "aux.h"
#include <math.h>

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
}

void damageOutputBox(struct Output *output, struct wlr_box *box) {
  wlr_damage_ring_add_box(&output->damage_ring, box);
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
  
  /* Accumulate current + previous frame damage for double buffering */
  pixman_region32_t accumulated_damage;
  pixman_region32_init(&accumulated_damage);
  pixman_region32_union(&accumulated_damage, &container->damage_ring.current, &container->prev_damage);
  
  /* Save current damage for next frame, then clear */
  pixman_region32_copy(&container->prev_damage, &container->damage_ring.current);
  pixman_region32_clear(&container->damage_ring.current);
  
  /* Get damage bounding box for scissor optimization */
  pixman_box32_t *damage_extents = pixman_region32_extents(&accumulated_damage);
  
  /* Scissor coordinates - no Y flip needed as rendering uses same coord system */
  int scissor_x = damage_extents->x1;
  int scissor_y = damage_extents->y1;
  int scissor_width = damage_extents->x2 - damage_extents->x1;
  int scissor_height = damage_extents->y2 - damage_extents->y1;
  
  pixman_region32_fini(&accumulated_damage);
  
  /* Clamp to valid range */
  if (scissor_x < 0) scissor_x = 0;
  if (scissor_y < 0) scissor_y = 0;
  if (scissor_width <= 0 || scissor_height <= 0) {
    /* No valid damage, render full screen */
    scissor_x = 0;
    scissor_y = 0;
    scissor_width = output_width;
    scissor_height = output_height;
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
  /* Enable scissor test to only render damaged region */
  glEnable(GL_SCISSOR_TEST);
  glScissor(scissor_x, scissor_y, scissor_width, scissor_height);
  
  glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  useShader(container->windowShader);
  
  /* Setup projection for orthographic view */
  mat4 proj = GLM_MAT4_IDENTITY_INIT;
  glm_ortho(0, container->wlr_output->width, 0, container->wlr_output->height, -10.0f, 10.0f, proj);
  set4fv(container->windowShader, "projection", 1, GL_FALSE, (float*)proj);
  
  mat4 view = GLM_MAT4_IDENTITY_INIT;
  set4fv(container->windowShader, "view", 1, GL_FALSE, (float*)view);

  /* Render client surfaces with rotation support */
  /* Render back-to-front: focused view is at front of list, should render last (on top) */
  float depth = -6;
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

  /* Capture screen content for cursor effect (within scissor region) */
  glBindTexture(GL_TEXTURE_2D, container->screenTexture);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 
                      container->wlr_output->width, container->wlr_output->height);

  /* Draw fancy cursor (keep scissor enabled to avoid drawing stale content) */
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
  
  /* Debug: draw damage region overlay (inside scissor - no extra damage needed) */
  if (container->server->debugDamage) {
    /* Varying color based on frame for visibility */
    static int frame_counter = 0;
    frame_counter++;
    float r = 0.3f + 0.4f * sinf(frame_counter * 0.1f);
    float g = 0.3f + 0.4f * sinf(frame_counter * 0.13f + 2.0f);
    float b = 0.8f;
    
    useShader(container->debugShader);
    glUniform4f(glGetUniformLocation(container->debugShader->ID, "u_color"), r, g, b, 0.4f);
    
    /* Draw filled quad covering entire screen - scissor clips it to damage region */
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
