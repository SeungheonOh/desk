#pragma once
#include "imports.h"
#include "server.h"
#include "events.h"

typedef struct Keyboard {
  struct DeskServer *server;
  struct wl_list link;

  struct wlr_keyboard *wlr_keyboard;

  struct wl_listener modifiers;
  struct wl_listener key;
  struct wl_listener destroy;
} Keyboard;

LISTNER(modifiers, void, Keyboard);
LISTNER(key, struct wlr_keyboard_key_event, Keyboard);
LISTNER(destroy, void, Keyboard);
