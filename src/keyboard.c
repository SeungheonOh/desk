#include "keyboard.h"

HANDLE(modifiers, void, Keyboard){
}
HANDLE(key, struct wlr_keyboard_key_event, Keyboard){
  // libinput to xkb keycode
  uint32_t keycode = data->keycode + 8;
  
  const xkb_keysym_t *syms;
  int nsyms = xkb_state_key_get_syms(container->wlr_keyboard->xkb_state, keycode, &syms);
  for(int i = 0; i < nsyms; i++) {
    if(syms[i] == XKB_KEY_Escape) {
      wl_display_terminate((container->server)->display);
      return;
    }

    if(syms[i] == XKB_KEY_a && data->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      LOG("running kitty");
      if (fork() == 0) {
	execl("/bin/sh", "/bin/sh", "-c", "nix run nixpkgs#kitty &> /dev/null", (void *)NULL);
      }      
    }
    
    if(syms[i] == XKB_KEY_r && data->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      LOG("re-compiling shader, %d", wl_list_length(&container->server->outputs));

      struct Output *e;
      wl_list_for_each(e, &container->server->outputs, link) {
	struct wlr_egl *egl = wlr_gles2_renderer_get_egl(e->wlr_output->renderer);
	if (eglGetCurrentContext() != wlr_egl_get_context(egl)) {
	  eglMakeCurrent(wlr_egl_get_display(egl), EGL_NO_SURFACE, EGL_NO_SURFACE, wlr_egl_get_context(egl));
	}

	struct shader *old = e ->windowShader;
	struct shader *new = newShader("./src/shader/vert_flat.glsl", "./src/shader/frag_flat.glsl");
	if(!new) continue;
	e->windowShader = new;
	destroyShader(old);
      }      
    }    
  }  
}
HANDLE(destroy, void, Keyboard){
}
