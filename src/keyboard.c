#include "keyboard.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>

static void spawn_wayland_client(struct DeskServer *server, const char *cmd) {
  pid_t pid = fork();
  if (pid < 0) {
    wlr_log_errno(WLR_ERROR, "fork failed");
    return;
  }

  if (pid == 0) {
    // Child process - client connects via WAYLAND_DISPLAY (inherited from parent)
    execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
    wlr_log_errno(WLR_ERROR, "exec failed");
    exit(1);
  }

  // Parent process - nothing to do, client connects automatically via the socket
}

HANDLE(modifiers, void, Keyboard){
  wlr_seat_set_keyboard(container->server->seat, container->wlr_keyboard);
  wlr_seat_keyboard_notify_modifiers(container->server->seat,
				     &container->wlr_keyboard->modifiers);
  
  // Track Alt key state - Alt enables both move and rotation mode
  uint32_t mods = wlr_keyboard_get_modifiers(container->wlr_keyboard);
  bool altPressed = (mods & WLR_MODIFIER_ALT) != 0;
  container->server->superPressed = altPressed;
  container->server->rotationMode = altPressed ? 1 : 0;
  
  // Cancel move if Alt is released during drag
  if (!altPressed && container->server->moveMode) {
    container->server->moveMode = false;
    container->server->grabbed_view = NULL;
  }
}
HANDLE(key, struct wlr_keyboard_key_event, Keyboard){
  wlr_seat_set_keyboard(container->server->seat, container->wlr_keyboard);
  wlr_seat_keyboard_notify_key(container->server->seat, data->time_msec,
			       data->keycode, data->state);
  // libinput to xkb keycode
  uint32_t keycode = data->keycode + 8;

  const xkb_keysym_t *syms;
  int nsyms = xkb_state_key_get_syms(container->wlr_keyboard->xkb_state, keycode, &syms);
  // Check for Alt key via modifiers - Alt enables both move and rotation mode
  uint32_t mods = wlr_keyboard_get_modifiers(container->wlr_keyboard);
  bool altPressed = (mods & WLR_MODIFIER_ALT) != 0;
  container->server->superPressed = altPressed;
  container->server->rotationMode = altPressed ? 1 : 0;

  for(int i = 0; i < nsyms; i++) {
    if(syms[i] == XKB_KEY_Escape && altPressed) {
      wl_display_destroy_clients(container->server->display);
      wl_display_terminate(container->server->display);
      return;
    }
    if(syms[i] == XKB_KEY_q && altPressed) {
      wl_display_destroy_clients(container->server->display);
      return;
    }

    if(syms[i] == XKB_KEY_a && altPressed && data->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      LOG("running movie");
      spawn_wayland_client(container->server, "mpv -- vid.mp4 --loop 2>/dev/null");
      return;
    }
    if(syms[i] == XKB_KEY_b && altPressed && data->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      LOG("running foot");
      spawn_wayland_client(container->server, "weston-terminal");
      return;
    }
    if(syms[i] == XKB_KEY_c && altPressed && data->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      LOG("running foot");
      spawn_wayland_client(container->server, "./test_click.py");
      return;
    }
    if(syms[i] == XKB_KEY_x && altPressed && data->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      LOG("running rofi");
      spawn_wayland_client(container->server, "rofi -show drun");
      return;
    }            

    if(syms[i] == XKB_KEY_r && altPressed && data->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      LOG("re-compiling shader, %d", wl_list_length(&container->server->outputs));

      struct Output *e;
      wl_list_for_each(e, &container->server->outputs, link) {
	struct wlr_egl *egl = wlr_gles2_renderer_get_egl(e->wlr_output->renderer);
	if (eglGetCurrentContext() != wlr_egl_get_context(egl)) {
	  eglMakeCurrent(wlr_egl_get_display(egl), EGL_NO_SURFACE, EGL_NO_SURFACE, wlr_egl_get_context(egl));
	}

        reloadShader(e->windowShader);
      }
      return;
    }
    
    if(syms[i] == XKB_KEY_d && altPressed && data->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      container->server->debugDamage = !container->server->debugDamage;
      LOG("Debug damage: %s", container->server->debugDamage ? "ON" : "OFF");
      damageWholeServer(container->server);
      return;
    }
  }
}
HANDLE(destroy, void, Keyboard){
}
