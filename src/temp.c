#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <xkbcommon/xkbcommon.h>
#include <render/egl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GL/gl.h>
#include <cairo.h>
#include <cglm/vec3.h>
#include <cglm/mat4.h>
#include <cglm/affine.h>
#include <cglm/cam.h>

#define PI 3.1415926535


GLuint texture;

/* For brevity's sake, struct members are annotated where they are used. */
enum tinywl_cursor_mode {
  TINYWL_CURSOR_PASSTHROUGH,
  TINYWL_CURSOR_MOVE,
  TINYWL_CURSOR_RESIZE,
};

struct tinywl_server {
  struct wl_display *wl_display;
  struct wlr_backend *backend;
  struct wlr_renderer *renderer;
  struct wlr_allocator *allocator;
  struct wlr_scene *scene;

  struct wlr_xdg_shell *xdg_shell;
  struct wl_listener new_xdg_surface;
  struct wl_list views;

  struct wlr_cursor *cursor;
  struct wlr_xcursor_manager *cursor_mgr;
  struct wl_listener cursor_motion;
  struct wl_listener cursor_motion_absolute;
  struct wl_listener cursor_button;
  struct wl_listener cursor_axis;
  struct wl_listener cursor_frame;

  struct wlr_seat *seat;
  struct wl_listener new_input;
  struct wl_listener request_cursor;
  struct wl_listener request_set_selection;
  struct wl_list keyboards;
  enum tinywl_cursor_mode cursor_mode;
  struct tinywl_view *grabbed_view;
  double grab_x, grab_y;
  struct wlr_box grab_geobox;
  uint32_t resize_edges;

  struct wlr_output_layout *output_layout;
  struct wl_list outputs;
  struct wl_listener new_output;

  vec3 cameraPos;
  vec3 cameraTarget;
  vec3 cameraDirection;

  vec3 up;
  vec3 cameraRight;
  vec3 cameraUp;

  vec3 mov;
};

struct tinywl_output {
  struct wl_list link;
  struct tinywl_server *server;
  struct wlr_output *wlr_output;
  struct wl_listener frame;
  struct wl_listener request_state;
  struct wl_listener destroy;
};

struct tinywl_view {
  struct wl_list link;
  struct tinywl_server *server;
  struct wlr_xdg_toplevel *xdg_toplevel;
  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener destroy;
  struct wl_listener request_move;
  struct wl_listener request_resize;
  struct wl_listener request_maximize;
  struct wl_listener request_fullscreen;
};

struct tinywl_keyboard {
  struct wl_list link;
  struct tinywl_server *server;
  struct wlr_keyboard *wlr_keyboard;

  struct wl_listener modifiers;
  struct wl_listener key;
  struct wl_listener destroy;
};

const char* prettyEglErr(EGLint err) {
  switch(err) {
  case EGL_SUCCESS: return "The last function succeeded without error";
  case EGL_NOT_INITIALIZED: return "EGL is not initialized, or could not be initialized, for the specified EGL display connection";
  case EGL_BAD_ACCESS: return "EGL cannot access a requested resource (for example a context is bound in another thread)";
  case EGL_BAD_ALLOC: return "EGL failed to allocate resources for the requested operation";
  case EGL_BAD_ATTRIBUTE: return "An unrecognized attribute or attribute value was passed in the attribute list";
  case EGL_BAD_CONTEXT: return "An EGLContext argument does not name a valid EGL rendering context";
  case EGL_BAD_CONFIG: return "An EGLConfig argument does not name a valid EGL frame buffer configuration";
  case EGL_BAD_CURRENT_SURFACE: return "The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid";
  case EGL_BAD_DISPLAY: return "An EGLDisplay argument does not name a valid EGL display connection";
  case EGL_BAD_SURFACE: return "An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering";
  case EGL_BAD_MATCH: return "Arguments are inconsistent (for example, a valid context requires buffers not supplied by a valid surface)";
  case EGL_BAD_PARAMETER: return "One or more argument values are invalid";
  case EGL_BAD_NATIVE_PIXMAP: return "A NativePixmapType argument does not refer to a valid native pixmap";
  case EGL_BAD_NATIVE_WINDOW: return "A NativeWindowType argument does not refer to a valid native window";
  case EGL_CONTEXT_LOST: return "A power management event has occurred. The application must destroy all contexts and reinitialise OpenGL ES state and objects to continue rendering";
  default: return "Unknown error";
  }
}

static void output_frame(struct wl_listener *listener, void *data) {
  /* This function is called every time an output is ready to display a frame,
   * generally at the output's refresh rate (e.g. 60Hz). */
  struct tinywl_output *output = wl_container_of(listener, output, frame);

  wlr_output_attach_render(output->wlr_output, NULL);
  wlr_renderer_begin(output->server->renderer, output->wlr_output->width, output->wlr_output->height);

  /* GLuint frameBufferName = 0; */
  /* glGenFramebuffers(1, &frameBufferName); */
  /* glBindFramebuffer(GL_FRAMEBUFFER, frameBufferName); */

  /* GLuint renderedTexture; */
  /* glGenTextures(1, &renderedTexture); */

  /* glBindTexture(GL_TEXTURE_2D, renderedTexture); */
  /* glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, output->wlr_output->width, output->wlr_output->height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0); */

  /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); */
  /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); */

  /* glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderedTexture, 0); */
  /* glBindFramebuffer(GL_FRAMEBUFFER, frameBufferName); */

  const char *vertexShaderSource =
    "#version 300 es                            \n"
    "layout(location = 0) in vec4 a_position;   \n"
    "layout(location = 1) in vec2 a_texCoord;   \n"
    "out vec2 v_texCoord;                       \n"
    "uniform mat4 model;                        \n"
    "uniform mat4 view;                         \n"
    "uniform mat4 projection;                   \n"
    "void main()                                \n"
    "{                                          \n"
    "   gl_Position = projection * view * model * a_position;\n"
    "   v_texCoord = a_texCoord;                \n"
    "}                                          \n";

  const char *fragmentShaderSource =
    "#version 300 es                                     \n"
    "precision mediump float;                            \n"
    "in vec2 v_texCoord;                                 \n"
    "layout(location = 0) out vec4 outColor;             \n"
    "uniform sampler2D s_texture;                        \n"
    "void main()                                         \n"
    "{                                                   \n"
    "  outColor = texture2D(s_texture, v_texCoord.xy).bgra;   \n"
    "}                                                   \n";

  unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader(vertexShader);
  // check for shader compile errors
  int success;
  char infoLog[512];
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);

    wlr_log(WLR_ERROR, "vertex shader failed to compile: %s", infoLog);
  }
  // fragment shader
  unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);
  // check for shader compile errors
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
    wlr_log(WLR_ERROR, "fragment shader failed to compile: %s", infoLog);
  }
  // link shaders
  unsigned int shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  // check for linking errors
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
    wlr_log(WLR_ERROR, "shaders failed to link: %s", infoLog);
  }
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  GLint samplerLoc = glGetUniformLocation(shaderProgram, "s_texture");
  GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
  GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
  GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");

  float n = -1.0f, p = 1.0f;

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

  GLint appTexture;
  GLenum appTextureTarget = GL_TEXTURE_2D;

  if(wl_list_empty(&output->server->views))
    appTexture = texture;
  else {
    struct tinywl_view *curr = output->server->views.next;
    struct wlr_seat *seat = output->server->seat;

    struct wlr_box geo_box;
    wlr_xdg_surface_get_geometry(curr->xdg_toplevel->base, &geo_box);

    printf("x: %d, y: %d, width: %d, height: %d\n", geo_box.x, geo_box.y, geo_box.width, geo_box.height);

    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
    wlr_xdg_toplevel_set_activated(curr->xdg_toplevel, true);

    wlr_seat_keyboard_notify_enter(seat, curr->xdg_toplevel->base->surface,
				   keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);

    struct wlr_texture *t = wlr_surface_get_texture(curr->xdg_toplevel->base->surface);

    struct wlr_gles2_texture_attribs attrs;
    wlr_gles2_texture_get_attribs(t, &attrs);

    appTexture = attrs.tex;
    appTextureTarget = attrs.target;

    if(curr->xdg_toplevel->title)
      printf("xdg toplevel title: %s\n", curr->xdg_toplevel->title);
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Draw
  glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(shaderProgram);

  glVertexAttribPointer ( 0, 3, GL_FLOAT,
			  GL_FALSE, 5 * sizeof ( GLfloat ), vVertices );
  // Load the texture coordinate
  glVertexAttribPointer ( 1, 2, GL_FLOAT,
			  GL_FALSE, 5 * sizeof ( GLfloat ), &vVertices[3] );

  mat4 trans = GLM_MAT4_IDENTITY_INIT;
  int height = output->wlr_output->height, width = output->wlr_output->width;

  glm_translate(trans, (float[3]){(float)output->server->cursor->x - (float)width/2, (float)output->server->cursor->y - (float)height/2, -1.0f});
  glm_scale(trans, (float[3]){50.0f, 50.0f, 1.0f});

  /* glm_rotate(trans, PI / 4, GLM_YUP); */
  /* glm_rotate(trans, PI / 4, GLM_XUP); */

  mat4 proj = GLM_MAT4_IDENTITY_INIT;
  glm_perspective(PI/2, (float)width/(float)height, 0.0001f, 10000.0f, proj);

  mat4 view = GLM_MAT4_IDENTITY_INIT;
  static mat4 viewM = GLM_MAT4_IDENTITY_INIT;
#define MIN(a, b) a < b ? a : b
  float scale = 1 / (MIN((float)width, (float)height) / 2);
  glm_scale(view, (float[3]){scale, scale, 1.0f});
  //glm_scale(view, (float[3]){1/1280.0f, 1/720.0f, 1.0f});
  //glm_translate(viewM, (float[3]){0.0f, 0.0f, output->server->mov[0] * 5});

  glm_mat4_mul(view, viewM, view);
  /* glm_rotate(view, PI/10 * output->server->mov[0], GLM_YUP); */
  /* glm_rotate(view, PI/10 * output->server->mov[1] * -1, GLM_XUP); */
  glm_vec3_zero(output->server->mov);
  /* glm_rotate(view, PI/20, GLM_ZUP); */
  /* glm_rotate(view, PI/30 * output->server->test, GLM_XUP); */

  glUniformMatrix4fv(modelLoc, 1, GL_FALSE, trans);
  glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);
  glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, proj);

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(appTextureTarget, appTexture);

  glUniform1i(samplerLoc, 0);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

  wlr_renderer_end(output->server->renderer);
  wlr_output_commit(output->wlr_output);
}

static void output_request_state(struct wl_listener *listener, void *data) {
  /* This function is called when the backend requests a new state for
   * the output. For example, Wayland and X11 backends request a new mode
   * when the output window is resized. */
  struct tinywl_output *output = wl_container_of(listener, output, request_state);
  const struct wlr_output_event_request_state *event = data;
  wlr_output_commit_state(output->wlr_output, event->state);
}

static void output_destroy(struct wl_listener *listener, void *data) {
  struct tinywl_output *output = wl_container_of(listener, output, destroy);

  wl_list_remove(&output->frame.link);
  wl_list_remove(&output->request_state.link);
  wl_list_remove(&output->destroy.link);
  wl_list_remove(&output->link);
  free(output);
}

static void server_new_output(struct wl_listener *listener, void *data) {
  /* This event is raised by the backend when a new output (aka a display or
   * monitor) becomes available. */
  struct tinywl_server *server =
    wl_container_of(listener, server, new_output);
  struct wlr_output *wlr_output = data;

  /* Configures the output created by the backend to use our allocator
   * and our renderer. Must be done once, before commiting the output */
  wlr_output_init_render(wlr_output, server->allocator, server->renderer);

  /* The output may be disabled, switch it on. */
  struct wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_enabled(&state, true);

  /* Some backends don't have modes. DRM+KMS does, and we need to set a mode
   * before we can use the output. The mode is a tuple of (width, height,
   * refresh rate), and each monitor supports only a specific set of modes. We
   * just pick the monitor's preferred mode, a more sophisticated compositor
   * would let the user configure it. */
  struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
  if (mode != NULL) {
    wlr_output_state_set_mode(&state, mode);
  }

  /* Atomically applies the new output state. */
  wlr_output_commit_state(wlr_output, &state);
  wlr_output_state_finish(&state);

  /* Allocates and configures our state for this output */
  struct tinywl_output *output =
    calloc(1, sizeof(struct tinywl_output));
  output->wlr_output = wlr_output;
  output->server = server;

  /* Sets up a listener for the frame event. */
  output->frame.notify = output_frame;
  wl_signal_add(&wlr_output->events.frame, &output->frame);

  /* Sets up a listener for the state request event. */
  output->request_state.notify = output_request_state;
  wl_signal_add(&wlr_output->events.request_state, &output->request_state);

  /* Sets up a listener for the destroy event. */
  output->destroy.notify = output_destroy;
  wl_signal_add(&wlr_output->events.destroy, &output->destroy);

  wl_list_insert(&server->outputs, &output->link);

  /* Adds this to the output layout. The add_auto function arranges outputs
   * from left-to-right in the order they appear. A more sophisticated
   * compositor would let the user configure the arrangement of outputs in the
   * layout.
   *
   * The output layout utility automatically adds a wl_output global to the
   * display, which Wayland clients can see to find out information about the
   * output (such as DPI, scale factor, manufacturer, etc).
   */
  wlr_output_layout_add_auto(server->output_layout, wlr_output);

  printf("width: %d, height: %d\n", output->wlr_output->width, output->wlr_output->height);

  cairo_surface_t *hugme = cairo_image_surface_create_from_png("hug_me_no_text.png");
  cairo_surface_flush(hugme);

  unsigned char *hugmeData = cairo_image_surface_get_data(hugme);

  int hugmeWidth = cairo_image_surface_get_width(hugme), hugmeHeight = cairo_image_surface_get_height(hugme);

  printf("hugme width and height: %d, %d, format: %d\n", hugmeWidth, hugmeHeight, cairo_image_surface_get_format(hugme) == CAIRO_FORMAT_ARGB32);

  // generate texture
  glGenTextures(1, &texture);

  // bind the texture
  glBindTexture(GL_TEXTURE_2D, texture);

  // set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, hugmeWidth, hugmeHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, hugmeData);

  glBindTexture(GL_TEXTURE_2D, 0);
}

void server_new_input(struct wl_listener *listener, void *data) {
  /* This event is raised by the backend when a new input device becomes
   * available. */
  struct tinywl_server *server =
    wl_container_of(listener, server, new_input);
  struct wlr_input_device *device = data;
  switch (device->type) {
  case WLR_INPUT_DEVICE_KEYBOARD:
    server_new_keyboard(server, device);
    break;
  case WLR_INPUT_DEVICE_POINTER:
    server_new_pointer(server, device);
    break;
  default:
    break;
  }
  /* We need to let the wlr_seat know what our capabilities are, which is
   * communiciated to the client. In TinyWL we always have a cursor, even if
   * there are no pointer devices, so we always include that capability. */
  uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
  if (!wl_list_empty(&server->keyboards)) {
    caps |= WL_SEAT_CAPABILITY_KEYBOARD;
  }
  wlr_seat_set_capabilities(server->seat, caps);
}

static void keyboard_handle_modifiers(
				      struct wl_listener *listener, void *data) {
  /* This event is raised when a modifier key, such as shift or alt, is
   * pressed. We simply communicate this to the client. */
  struct tinywl_keyboard *keyboard =
    wl_container_of(listener, keyboard, modifiers);
  /*
   * A seat can only have one keyboard, but this is a limitation of the
   * Wayland protocol - not wlroots. We assign all connected keyboards to the
   * same seat. You can swap out the underlying wlr_keyboard like this and
   * wlr_seat handles this transparently.
   */
  wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
  /* Send modifiers to the client. */
  wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
				     &keyboard->wlr_keyboard->modifiers);
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
  /* This event is raised by the keyboard base wlr_input_device to signal
   * the destruction of the wlr_keyboard. It will no longer receive events
   * and should be destroyed.
   */
  struct tinywl_keyboard *keyboard =
    wl_container_of(listener, keyboard, destroy);
  wl_list_remove(&keyboard->modifiers.link);
  wl_list_remove(&keyboard->key.link);
  wl_list_remove(&keyboard->destroy.link);
  wl_list_remove(&keyboard->link);
  free(keyboard);
}

static void keyboard_handle_key(
				struct wl_listener *listener, void *data) {
  /* This event is raised when a key is pressed or released. */
  struct tinywl_keyboard *keyboard =
    wl_container_of(listener, keyboard, key);
  struct tinywl_server *server = keyboard->server;
  struct wlr_keyboard_key_event *event = data;
  struct wlr_seat *seat = server->seat;

  /* Translate libinput keycode -> xkbcommon */
  uint32_t keycode = event->keycode + 8;

  const xkb_keysym_t *syms;
  int nsyms = xkb_state_key_get_syms(
				     keyboard->wlr_keyboard->xkb_state, keycode, &syms);

  for (int i = 0; i < nsyms; i++) {
    if(syms[i] == XKB_KEY_Escape) {
      wl_display_terminate(server->wl_display);
      return;
    }
  }

  /* Otherwise, we pass it along to the client. */
  wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
  wlr_seat_keyboard_notify_key(seat, event->time_msec,
			       event->keycode, event->state);
}

void server_new_keyboard(struct tinywl_server *server,
			 struct wlr_input_device *device) {

  wlr_log(WLR_DEBUG, "newkeyboard");
  struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

  struct tinywl_keyboard *keyboard =
    calloc(1, sizeof(struct tinywl_keyboard));
  keyboard->server = server;
  keyboard->wlr_keyboard = wlr_keyboard;

  /* We need to prepare an XKB keymap and assign it to the keyboard. This
   * assumes the defaults (e.g. layout = "us"). */
  struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
							XKB_KEYMAP_COMPILE_NO_FLAGS);

  wlr_keyboard_set_keymap(wlr_keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

  /* Here we set up listeners for keyboard events. */
  keyboard->modifiers.notify = keyboard_handle_modifiers;
  wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
  keyboard->key.notify = keyboard_handle_key;
  wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
  keyboard->destroy.notify = keyboard_handle_destroy;
  wl_signal_add(&device->events.destroy, &keyboard->destroy);

  wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);

  /* And add the keyboard to our list of keyboards */
  wl_list_insert(&server->keyboards, &keyboard->link);
}

void server_new_pointer(struct tinywl_server *server,
			struct wlr_input_device *device) {
  /* We don't do anything special with pointers. All of our pointer handling
   * is proxied through wlr_cursor. On another compositor, you might take this
   * opportunity to do libinput configuration on the device to set
   * acceleration, etc. */
  wlr_cursor_attach_input_device(server->cursor, device);
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits a _relative_
   * pointer motion event (i.e. a delta) */
  struct tinywl_server *server =
    wl_container_of(listener, server, cursor_motion);
  struct wlr_pointer_motion_event *event = data;
  /* The cursor doesn't move unless we tell it to. The cursor automatically
   * handles constraining the motion to the output layout, as well as any
   * special configuration applied for the specific input device which
   * generated the event. You can pass NULL for the device if you want to move
   * the cursor around without any input. */
  wlr_cursor_move(server->cursor, &event->pointer->base,
		  event->delta_x, event->delta_y);

  /* printf("cursor at: %f, %f\n", server->cursor->x, server->cursor->y); */
  /* printf("cursor motion: %f, %f\n", event->delta_x, event->delta_y); */

  server->mov[0] = event->delta_x / 100;
  server->mov[1] = event->delta_y / 100;
  server->mov[2] = 0.0f;
}

static void server_cursor_motion_absolute(
					  struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits an _absolute_
   * motion event, from 0..1 on each axis. This happens, for example, when
   * wlroots is running under a Wayland window rather than KMS+DRM, and you
   * move the mouse over the window. You could enter the window from any edge,
   * so we have to warp the mouse there. There is also some hardware which
   * emits these events. */
  struct tinywl_server *server =
    wl_container_of(listener, server, cursor_motion_absolute);
  struct wlr_pointer_motion_absolute_event *event = data;
  wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x,
			   event->y);

  /* printf("cursor jump: %f, %f\n", event->x , event->y); */
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
  /* Called when the surface is mapped, or ready to display on-screen. */
  struct tinywl_view *view = wl_container_of(listener, view, map);

  wl_list_insert(&view->server->views, &view->link);
  wlr_xdg_toplevel_set_size(view->xdg_toplevel, 420, 42);

  printf("xdg toplevel mapped\n");
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
  /* Called when the surface is unmapped, and should no longer be shown. */
  struct tinywl_view *view = wl_container_of(listener, view, unmap);

  wl_list_remove(&view->link);

  printf("xdg toplevel unmapped\n");
}

static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
  /* This event is raised when wlr_xdg_shell receives a new xdg surface from a
   * client, either a toplevel (application window) or popup. */
  struct tinywl_server *server =
    wl_container_of(listener, server, new_xdg_surface);
  struct wlr_xdg_surface *xdg_surface = data;

  /* We must add xdg popups to the scene graph so they get rendered. The
   * wlroots scene graph provides a helper for this, but to use it we must
   * provide the proper parent scene node of the xdg popup. To enable this,
   * we always set the user data field of xdg_surfaces to the corresponding
   * scene node. */
  if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
    struct wlr_xdg_surface *parent =
      wlr_xdg_surface_try_from_wlr_surface(xdg_surface->popup->parent);
    assert(parent != NULL);
    return;
  }
  assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

  /* Allocate a tinywl_view for this surface */
  struct tinywl_view *view =
    calloc(1, sizeof(struct tinywl_view));
  view->server = server;
  view->xdg_toplevel = xdg_surface->toplevel;

  view->map.notify = xdg_toplevel_map;
  wl_signal_add(&xdg_surface->surface->events.map, &view->map);
  view->unmap.notify = xdg_toplevel_unmap;
  wl_signal_add(&xdg_surface->surface->events.unmap, &view->unmap);

  /* Listen to the various events it can emit */
  /* view->map.notify = xdg_toplevel_map; */
  /* wl_signal_add(&xdg_surface->surface->events.map, &view->map); */
  /* view->unmap.notify = xdg_toplevel_unmap; */
  /* wl_signal_add(&xdg_surface->surface->events.unmap, &view->unmap); */
  /* view->destroy.notify = xdg_toplevel_destroy; */
  /* wl_signal_add(&xdg_surface->events.destroy, &view->destroy); */

  /* cotd */
  /* struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel; */
  /* view->request_move.notify = xdg_toplevel_request_move; */
  /* wl_signal_add(&toplevel->events.request_move, &view->request_move); */
  /* view->request_resize.notify = xdg_toplevel_request_resize; */
  /* wl_signal_add(&toplevel->events.request_resize, &view->request_resize); */
  /* view->request_maximize.notify = xdg_toplevel_request_maximize; */
  /* wl_signal_add(&toplevel->events.request_maximize, */
  /* 	&view->request_maximize); */
  /* view->request_fullscreen.notify = xdg_toplevel_request_fullscreen; */
  /* wl_signal_add(&toplevel->events.request_fullscreen, */
  /* 	&view->request_fullscreen); */
}

int main(int argc, char *argv[]) {


  wlr_log_init(WLR_DEBUG, NULL);
  char *startup_cmd = NULL;

  int c;
  while ((c = getopt(argc, argv, "s:h")) != -1) {
    switch (c) {
    case 's':
      startup_cmd = optarg;
      break;
    default:
      printf("Usage: %s [-s startup command]\n", argv[0]);
      return 0;
    }
  }
  if (optind < argc) {
    printf("Usage: %s [-s startup command]\n", argv[0]);
    return 0;
  }

  struct tinywl_server server = {0};
  /* The Wayland display is managed by libwayland. It handles accepting
   * clients from the Unix socket, manging Wayland globals, and so on. */
  server.wl_display = wl_display_create();
  /* The backend is a wlroots feature which abstracts the underlying input and
   * output hardware. The autocreate option will choose the most suitable
   * backend based on the current environment, such as opening an X11 window
   * if an X11 server is running. */
  server.backend = wlr_backend_autocreate(server.wl_display, NULL);
  if (server.backend == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_backend");
    return 1;
  }

  /* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
   * can also specify a renderer using the WLR_RENDERER env var.
   * The renderer is responsible for defining the various pixel formats it
   * supports for shared memory, this configures that for clients. */
  server.renderer = wlr_renderer_autocreate(server.backend);
  if (server.renderer == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_renderer");
    return 1;
  }

  wlr_renderer_init_wl_display(server.renderer, server.wl_display);

  /* Autocreates an allocator for us.
   * The allocator is the bridge between the renderer and the backend. It
   * handles the buffer creation, allowing wlroots to render onto the
   * screen */
  server.allocator = wlr_allocator_autocreate(server.backend,
					      server.renderer);
  if (server.allocator == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_allocator");
    return 1;
  }

  /* This creates some hands-off wlroots interfaces. The compositor is
   * necessary for clients to allocate surfaces, the subcompositor allows to
   * assign the role of subsurfaces to surfaces and the data device manager
   * handles the clipboard. Each of these wlroots interfaces has room for you
   * to dig your fingers in and play with their behavior if you want. Note that
   * the clients cannot set the selection directly without compositor approval,
   * see the handling of the request_set_selection event below.*/
  wlr_compositor_create(server.wl_display, 5, server.renderer);
  wlr_subcompositor_create(server.wl_display);
  wlr_data_device_manager_create(server.wl_display);

  /* Creates an output layout, which a wlroots utility for working with an
   * arrangement of screens in a physical layout. */
  server.output_layout = wlr_output_layout_create();

  /* Configure a listener to be notified when new outputs are available on the
   * backend. */
  wl_list_init(&server.outputs);
  server.new_output.notify = server_new_output;
  wl_signal_add(&server.backend->events.new_output, &server.new_output);

  /* Create a scene graph. This is a wlroots abstraction that handles all
   * rendering and damage tracking. All the compositor author needs to do
   * is add things that should be rendered to the scene graph at the proper
   * positions and then call wlr_scene_output_commit() to render a frame if
   * necessary.
   */
  server.scene = wlr_scene_create();
  wlr_scene_attach_output_layout(server.scene, server.output_layout);

  /* Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
   * used for application windows. For more detail on shells, refer to my
   * article:
   *
   * https://drewdevault.com/2018/07/29/Wayland-shells.html
   */
  wl_list_init(&server.views);
  server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
  server.new_xdg_surface.notify = server_new_xdg_surface;
  wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);

  /*
   * Configures a seat, which is a single "seat" at which a user sits and
   * operates the computer. This conceptually includes up to one keyboard,
   * pointer, touch, and drawing tablet device. We also rig up a listener to
   * let us know when new input devices are available on the backend.
   */

  server.cursor = wlr_cursor_create();
  wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

  server.cursor_motion.notify = server_cursor_motion;
  wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
  server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
  wl_signal_add(&server.cursor->events.motion_absolute,
		&server.cursor_motion_absolute);

  wl_list_init(&server.keyboards);
  server.new_input.notify = server_new_input;
  wl_signal_add(&server.backend->events.new_input, &server.new_input);
  server.seat = wlr_seat_create(server.wl_display, "seat0");

  /* Add a Unix socket to the Wayland display. */
  const char *socket = wl_display_add_socket_auto(server.wl_display);
  if (!socket) {
    wlr_backend_destroy(server.backend);
    return 1;
  }

  /* Start the backend. This will enumerate outputs and inputs, become the DRM
   * master, etc */
  if (!wlr_backend_start(server.backend)) {
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.wl_display);
    return 1;
  }

  /* Set the WAYLAND_DISPLAY environment variable to our socket and run the
   * startup command if requested. */
  setenv("WAYLAND_DISPLAY", socket, true);

  /* Run the Wayland event loop. This does not return until you exit the
   * compositor. Starting the backend rigged up all of the necessary event
   * loop configuration to listen to libinput events, DRM events, generate
   * frame events at the refresh rate, and so on. */
  wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
	  socket);
  wl_display_run(server.wl_display);

  /* Once wl_display_run returns, we destroy all clients then shut down the
   * server. */
  wl_display_destroy_clients(server.wl_display);
  wlr_scene_node_destroy(&server.scene->tree.node);
  wlr_xcursor_manager_destroy(server.cursor_mgr);
  wlr_output_layout_destroy(server.output_layout);
  wl_display_destroy(server.wl_display);
  return 0;
}
