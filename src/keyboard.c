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
  }  
}
HANDLE(destroy, void, Keyboard){
}
