#pragma once
#include "imports.h"

#define LISTNER(name, dataType, containerType)	\
  void name##EventWrapper##containerType(containerType*, dataType*); \
  void name##Event##containerType(struct wl_listener *listener, void *data);

#define HANDLE(name, dataType, containerType) \
  void name##Event##containerType(struct wl_listener *listener, void *data) { \
    containerType *container = wl_container_of(listener, container, name); \
    name##EventWrapper##containerType(container, (dataType*)data); \
  } \
  void name##EventWrapper##containerType(containerType *container, dataType *data)

#define ATTACH(containerType, container, signal, name)	       \
  container->name.notify = name##Event##containerType; \
  wl_signal_add(&signal, &container->name)

// DeskServer

// Output
/* LISTNER(frame, void, struct Output); */

/* // Keyboard */
/* /\* LISTNER(modifiers, void, struct Keyboard); *\/ */
/* /\* LISTNER(key, void, struct Keyboard); *\/ */
/* /\* LISTNER(destroy, struct wlr_keyboard_key_event, struct Keyboard); *\/ */

/* LISTNER(resizeHandler, int, struct DeskServer); */
