#include "imports.h"
#include "events.h"

HANDLE(newXdgSurface, struct wlr_xdg_surface, struct DeskServer){
}
HANDLE(newInput, struct wlr_input_device, struct DeskServer){
  switch (data->type) {
  /* case WLR_INPUT_DEVICE_KEYBOARD: */
  /*   server_new_keyboard(server, device); */
  /*   break; */
  case WLR_INPUT_DEVICE_POINTER:
    wlr_cursor_attach_input_device(container->cursor, data);
    break;
  default:
    break;
  }
}
HANDLE(requestCursor, struct wlr_seat_pointer_request_set_cursor_event, struct DeskServer){
}
HANDLE(requestSetSelection, struct wlr_seat_request_set_selection_event, struct DeskServer){
}
HANDLE(cursorMotion, struct wlr_pointer_motion_event, struct DeskServer){
}
HANDLE(cursorMotionAbsolute, struct wlr_pointer_motion_event, struct DeskServer){
}
HANDLE(cursorButton, struct wlr_pointer_button_event, struct DeskServer){
}
HANDLE(cursorAxis, struct wlr_pointer_axis_event, struct DeskServer){
  wl_signal_emit_mutable(&container->resize, NULL);
}
HANDLE(cursorFrame, void, struct DeskServer){
}
HANDLE(newOutput, struct wlr_output, struct DeskServer){
  wlr_output_init_render(data, container->allocator, container->renderer);

  /* The output may be disabled, switch it on. */
  struct wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_enabled(&state, true);

  struct wlr_output_mode *mode = wlr_output_preferred_mode(data);
  if (mode != NULL) {
    wlr_output_state_set_mode(&state, mode);
  }

  wlr_output_commit_state(data, &state);
  wlr_output_state_finish(&state);
}

HANDLE(modifiers, void, struct Keyboard) {
}
HANDLE(key, void, struct Keyboard) {
}
HANDLE(destroy, struct wlr_keyboard_key_event, struct Keyboard) {
}


HANDLE(resizeHandler, void, struct DeskServer) {
  wlr_log(WLR_INFO, "==============hello world");
}
