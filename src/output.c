#include "output.h"

struct Output *mkOutput(struct DeskServer *container, struct wlr_output* data){
  struct Output *output = malloc(sizeof(struct Output));
  output->server = container;
  output->wlr_output = data;

  ATTACH(Output, output, data->events.frame, frame);
  ATTACH(Output, output, data->events.request_state, requestState);
  ATTACH(Output, output, data->events.destroy, destroy);

  output->windowShader = newShader("./src/shader/vert_flat.glsl", "./src/shader/frag_flat.glsl");      

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
  wlr_output_attach_render(container->wlr_output, NULL);  
  wlr_renderer_begin(container->server->renderer, container->wlr_output->width, container->wlr_output->height);

  glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  float n = -0.5f, p = 0.5f;
  GLfloat vVertices[] = { n,  p, 0.0f,  // Position 0
			  0.0f,  1.0f,        // TexCoord 0

			  n, n, 0.0f,  // Position 1
			  0.0f,  0.0f,        // TexCoord 1

			  p, n, 0.0f,  // Position 2
			  1.0f,  0.0f,        // TexCoord 2

			  p,  p, 0.0f,  // Position 3
			  1.0f,  1.0f         // TexCoord 3
  };
  GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

  useShader(container->windowShader);

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

HANDLE(requestState, struct wlr_output, Output) {
  LOG ("State request");
}

HANDLE(destroy, struct wlr_output, Output) {
  LOG ("Destroying %s", data->name);

  destroyOutput(container);
}
