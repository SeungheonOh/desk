#pragma once
#include "imports.h"

#include "state.h"

#define LISTNER(name, dataType, containerType)	\
  void name##EventWrapper(containerType*, dataType*); \
  void name##Event(struct wl_listener *listener, void *data);

#define HANDLE(name, dataType, containerType) \
  void name##Event(struct wl_listener *listener, void *data) { \
    containerType *container = wl_container_of(listener, container, name); \
    name##EventWrapper(container, (dataType*)data); \
  } \
  void name##EventWrapper(containerType *container, dataType *data)

#define ATTACH(container, signal, name)	\
  container->name.notify = name##Event; \
  wl_signal_add(&signal, &container->name)

// DeskServer
LISTNER(newXdgSurface, struct wlr_xdg_surface, struct DeskServer);
LISTNER(newInput, struct wlr_input_device, struct DeskServer);
LISTNER(requestCursor, struct wlr_seat_pointer_request_set_cursor_event, struct DeskServer);
LISTNER(requestSetSelection, struct wlr_seat_request_set_selection_event, struct DeskServer);
LISTNER(cursorMotion, struct wlr_pointer_motion_event, struct DeskServer);
LISTNER(cursorMotionAbsolute, struct wlr_pointer_motion_event, struct DeskServer);
LISTNER(cursorButton, struct wlr_pointer_button_event, struct DeskServer);
LISTNER(cursorAxis, struct wlr_pointer_axis_event, struct DeskServer);
LISTNER(cursorFrame, void, struct DeskServer);
LISTNER(newOutput, struct wlr_output, struct DeskServer);

// Keyboard
LISTNER(modifiers, void, struct Keyboard);
LISTNER(key, void, struct Keyboard);
LISTNER(destroy, struct wlr_keyboard_key_event, struct Keyboard);

LISTNER(resizeHandler, void, struct DeskServer);
