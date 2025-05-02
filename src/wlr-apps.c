#define _POSIX_C_SOURCE 200809L
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client.h>

// ----- Macros -----

#define WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION 3
#define SOCKET_PATH "/tmp/wlr-apps.socket"
#define BUFFER_SIZE 256
#define MAX_CLIENTS 1
#define POLL_TIMEOUT_MS 100

// ---- Enums -----

enum toplevel_state_field {
  TOPLEVEL_STATE_MAXIMIZED = (1 << 0),
  TOPLEVEL_STATE_MINIMIZED = (1 << 1),
  TOPLEVEL_STATE_ACTIVATED = (1 << 2),
  TOPLEVEL_STATE_FULLSCREEN = (1 << 3),
  TOPLEVEL_STATE_INVALID = (1 << 4),
};

// ---- Structs ----

static struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager = NULL;
static struct wl_list toplevel_list;

struct toplevel_state {
  char *title;
  char *app_id;

  uint32_t state;
  uint32_t parent_id;
};

static uint32_t global_id = 0;
struct toplevel_v1 {
  struct wl_list link;
  struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel;

  uint32_t seed;
  uint32_t id;
  struct toplevel_state current, pending;
};

static struct wl_output *pref_output = NULL;
struct wl_seat *seat = NULL;

struct toplevel_info {
  uint32_t tl_id;
  char *tl_title;
  char *tl_app_id;
  uint32_t tl_parent_id;
  bool maximized;
  bool minimized;
  bool active;
  bool fullscreen;
};

struct toplevel_list {
  struct toplevel_info *items;
  size_t count;
  size_t capacity;
};

struct toplevel_list global_info_list = {0};

struct wl_display *global_display = NULL;
struct wl_display *wl_display_get_default(void) {
  return global_display; // Provide access to the global display
}
struct wl_registry *registry = NULL;

struct toplevel_info *find_info_by_id(struct toplevel_list *list, uint32_t id) {
  for (size_t i = 0; i < list->count; ++i) {
    if (list->items[i].tl_id == id) {
      return &list->items[i];
    }
  }
  return NULL;
}

// ---- Global Variables ----

static const uint32_t no_parent = (uint32_t)-1;
static uint32_t pref_output_id = UINT32_MAX;
bool json_out = false;
bool sort_out = false;
static void update_toplevel_info_state(struct toplevel_v1 *toplevel);

// ---- Print Functions ----

static void print_help(void) {
  static const char usage[] =
      "Usage: wlr-apps [OPTIONS] [ARGUMENT]...\n"
      "Manage and view information about toplevel windows.\n"
      "\n"
      "  -f <id>         focus\n"
      "  -s <id>         fullscreen\n"
      "  -o <output_id>  select output for fullscreen toplevel to appear on. Use this\n"
      "                  option with -s. View available outputs with wayland-info.\n"
      "  -S <id>         unfullscreen\n"
      "  -a <id>         maximize\n"
      "  -u <id>         unmaximize\n"
      "  -i <id>         minimize\n"
      "  -r <id>         restore(unminimize)\n"
      "  -c <id>         close\n"
      "  -q              Sort output, this will ensure the order of the toplevels doesn't change\n"
      "                  between updates, specially when removing toplevels.\n"
      "  -m              continuously print changes to the list of opened toplevels\n"
      "                  Can be used together with some of the previous options.\n"
      "  -x              Launch instance in client mode, sending an event to the main\n"
      "  |               instance of the program via UNIX socket and exiting.\n"
      "  |               Available options:\n"
      "  |                \"f <id>\" (focus)\n"
      "  |                \"s <id>\" (fullscreen)\n"
      "  |                \"S <id>\" (unfullscreen)\n"
      "  |                \"a <id>\" (maximize)\n"
      "  |                \"i <id>\" (minimize)\n"
      "  |                \"r <id>\" (restore)\n"
      "  |                \"c <id>\" (close)\n"
      "  |                \"q\" (toggle sorting on/off)\n"
      "                  Example: wlr-apps -x \"close <id>\".\n"
      "  -j              Print the output in json format, this can used alone "
      "                  to print\n"
      "                  once and exit, or along -m to continously print "
      "                  changes in json format\n"
      "  -h              print help message and quit\n";
  fprintf(stderr, "%s", usage);
}

static void print_toplevel(struct toplevel_v1 *toplevel, bool print_endl) {

  printf("-> %d. title=%s app_id=%s", toplevel->id,
         toplevel->current.title ?: "(nil)",
         toplevel->current.app_id ?: "(nil)");

  if (toplevel->current.parent_id != no_parent) {
    printf(" parent=%u", toplevel->current.parent_id);
  } else {
    printf(" no parent");
  }

  if (print_endl) {
    printf("\n");
  }
}

static void print_toplevel_state(struct toplevel_v1 *toplevel,
                                 bool print_endl) {

  if (toplevel->current.state & TOPLEVEL_STATE_MAXIMIZED) {
    printf(" maximized");
  } else {
    printf(" unmaximized");
  }

  if (toplevel->current.state & TOPLEVEL_STATE_MINIMIZED) {
    printf(" minimized");
  } else {
    printf(" unminimized");
  }

  if (toplevel->current.state & TOPLEVEL_STATE_ACTIVATED) {
    printf(" active");
  } else {
    printf(" inactive");
  }

  if (toplevel->current.state & TOPLEVEL_STATE_FULLSCREEN) {
    printf(" fullscreen");
  }

  if (print_endl) {
    printf("\n");
  }
}

void print_json_string(const char *str) {

  if (str == NULL) {
    printf("null");
    return;
  }

  printf("\"");

  for (const char *p = str; *p; ++p) {
    switch (*p) {
    case '"':
      printf("\\\"");
      break;
    case '\\':
      printf("\\\\");
      break;
    case '\b':
      printf("\\\\");
      break;
    case '\f':
      printf("\\\\");
      break;
    case '\n':
      printf("\\\\");
      break;
    case '\r':
      printf("\\\\");
      break;
    case '\t':
      printf("\\\\");
      break;
    default:
      if (*p >= 0x00 && *p <= 0x1F) {
        printf("\\u%04x", (unsigned int)*p);
      } else {
        printf("%c", *p);
      }
      break;
    }
  }

  printf("\"");
}

int compare_toplevel_info(const void *a, const void *b){
  const struct toplevel_info *info_a = (const struct toplevel_info *)a;
  const struct toplevel_info *info_b = (const struct toplevel_info *)b;

  if (info_a->tl_id < info_b->tl_id){
    return -1;
  } else if (info_a->tl_id > info_b->tl_id) {
    return 1;
  } else {
    return 0;
  }
}

void print_toplevel_json_array(void) {

  if (global_info_list.count > 0 && sort_out) {
    qsort(global_info_list.items, global_info_list.count, sizeof(struct toplevel_info), compare_toplevel_info);
  }

  printf("[");

  for (size_t i = 0; i < global_info_list.count; ++i) {
    struct toplevel_info *info = &global_info_list.items[i];

    printf("{\"id\":%u,", info->tl_id);

    printf("\"title\":");
    print_json_string(info->tl_title);
    printf(",");

    printf("\"app_id\":");
    print_json_string(info->tl_app_id);
    printf(",");

    if (info->tl_parent_id != no_parent) {
      printf("\"parent_id\":%u,", info->tl_parent_id);
    } else {
      printf("\"parent_id\":null,");
    }

    printf(
        "\"maximized\":%s,\"minimized\":%s,\"active\":%s,\"fullscreen\":%s}%s",
        info->maximized ? "true" : "false", info->minimized ? "true" : "false",
        info->active ? "true" : "false", info->fullscreen ? "true" : "false",
        (i < global_info_list.count - 1) ? "," : "");
  }

  printf("]\n");
  fflush(stdout);
}

// ---- Helper Functions ----

void init_toplevel_list(struct toplevel_list *list) {
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

static void copy_state(struct toplevel_state *current,
                       struct toplevel_state *pending,
                       struct toplevel_v1 *toplevel) {
  if (current->title && pending->title) {
    free(current->title);
  }

  if (current->app_id && pending->app_id) {
    free(current->app_id);
  }

  if (pending->title) {
    current->title = pending->title;
    pending->title = NULL;
  }

  if (pending->app_id) {
    current->app_id = pending->app_id;
    pending->app_id = NULL;
  }

  if (!(pending->state & TOPLEVEL_STATE_INVALID)) {
    current->state = pending->state;
  }

  current->parent_id = pending->parent_id;
  pending->state = TOPLEVEL_STATE_INVALID;

  update_toplevel_info_state(toplevel);
}

static void update_toplevel_info_state(struct toplevel_v1 *toplevel) {
  struct toplevel_info *info = find_info_by_id(&global_info_list, toplevel->id);

  if (!info)
    return;

  info->tl_parent_id = toplevel->current.parent_id;
  info->maximized = toplevel->current.state & TOPLEVEL_STATE_MAXIMIZED;
  info->minimized = toplevel->current.state & TOPLEVEL_STATE_MINIMIZED;
  info->active = toplevel->current.state & TOPLEVEL_STATE_ACTIVATED;
  info->fullscreen = toplevel->current.state & TOPLEVEL_STATE_FULLSCREEN;

  if (toplevel->current.title) {
    free(info->tl_title);
    info->tl_title = strdup(toplevel->current.title);
  }

  if (toplevel->current.app_id) {
    free(info->tl_app_id);
    info->tl_app_id = strdup(toplevel->current.app_id);
  }
}

bool add_toplevel_info(struct toplevel_list *list,
                       struct toplevel_info *new_info) {
  if (list->count == list->capacity) {
    size_t new_capacity = (list->capacity == 0) ? 4 : list->capacity * 2;
    struct toplevel_info *new_items =
        realloc(list->items, new_capacity * sizeof(struct toplevel_info));
    if (!new_items)
      return false;
    list->items = new_items;
    list->capacity = new_capacity;
  }

  list->items[list->count++] = *new_info;
  return true;
}

void remove_toplevel_info(struct toplevel_list *list, uint32_t id) {
  for (size_t i = 0; i < list->count; ++i) {
    if (list->items[i].tl_id == id) {
      free(list->items[i].tl_title);
      free(list->items[i].tl_app_id);

      list->items[i] = list->items[list->count - 1];
      list->count--;

      return;
    }
  }
}

// ---- Wayland Callback Functions ----

static void toplevel_handle_title(
    void *data,
     struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
    const char *title) {
  struct toplevel_v1 *toplevel = data;
  free(toplevel->pending.title);
  toplevel->pending.title = strdup(title);
}

static void toplevel_handle_app_id(
    void *data,
     struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
    const char *app_id) {
  struct toplevel_v1 *toplevel = data;
  free(toplevel->pending.app_id);
  toplevel->pending.app_id = strdup(app_id);
}

static void toplevel_handle_output_enter(
    void *data,
     struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
    struct wl_output *output) {
   struct toplevel_v1 *toplevel = data;
  //print_toplevel(toplevel, false);
  printf(" enter output %u\n",
         (uint32_t)(size_t)wl_output_get_user_data(output));
}

static void toplevel_handle_output_leave(
    void *data,
     struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
    struct wl_output *output) {
      struct toplevel_v1 *toplevel = data;
      print_toplevel(toplevel, false);
  
      printf(" leave output %u\n",
         (uint32_t)(size_t)wl_output_get_user_data(output));
}

static uint32_t array_to_state(struct wl_array *array) {
  uint32_t state = 0;
  uint32_t *entry;

  wl_array_for_each(entry, array) {
    if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
      state |= TOPLEVEL_STATE_MAXIMIZED;
    if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
      state |= TOPLEVEL_STATE_MINIMIZED;
    if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
      state |= TOPLEVEL_STATE_ACTIVATED;
    if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
      state |= TOPLEVEL_STATE_FULLSCREEN;
  }

  return state;
}

static void toplevel_handle_state(
    void *data,
     struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
    struct wl_array *state) {
  struct toplevel_v1 *toplevel = data;
  toplevel->pending.state = array_to_state(state);
}

static void finish_toplevel_state(struct toplevel_state *state) {
  free(state->title);
  free(state->app_id);
}

static void toplevel_handle_parent(
    void *data,
     struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_parent) {
  struct toplevel_v1 *toplevel = data;
  toplevel->pending.parent_id = no_parent;

  if (zwlr_parent) {
    struct toplevel_v1 *toplevel_tmp;
    wl_list_for_each(toplevel_tmp, &toplevel_list, link) {

      if (toplevel_tmp->zwlr_toplevel == zwlr_parent) {
        toplevel->pending.parent_id = toplevel_tmp->id;
        break;
      }
    }

    if (toplevel->pending.parent_id == no_parent) {
      fprintf(stderr, "Cannot find parent toplevel!\n");
    }
  }
}

static void toplevel_handle_done(
    void *data,
     struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel) {
  struct toplevel_v1 *toplevel = data;
  bool state_changed = toplevel->current.state != toplevel->pending.state;

  copy_state(&toplevel->current, &toplevel->pending, toplevel);

  if (!json_out) {
    print_toplevel(toplevel, !state_changed);
    if (state_changed) {
      print_toplevel_state(toplevel, true);
    }
  }
}

static void
toplevel_handle_closed(void *data,
                       struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel) {
  struct toplevel_v1 *toplevel = data;

  if (!json_out) {
    print_toplevel(toplevel, false);
  }

  remove_toplevel_info(&global_info_list, toplevel->id);
  wl_list_remove(&toplevel->link);

  zwlr_foreign_toplevel_handle_v1_destroy(zwlr_toplevel);

  finish_toplevel_state(&toplevel->current);
  finish_toplevel_state(&toplevel->pending);

  free(toplevel);
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_impl = {
    .title = toplevel_handle_title,
    .app_id = toplevel_handle_app_id,
    .output_enter = toplevel_handle_output_enter,
    .output_leave = toplevel_handle_output_leave,
    .state = toplevel_handle_state,
    .done = toplevel_handle_done,
    .closed = toplevel_handle_closed,
    .parent = toplevel_handle_parent};

static void toplevel_manager_handle_toplevel(
     void *data,
     struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel) {
  struct toplevel_v1 *toplevel = calloc(1, sizeof(*toplevel));
  if (!toplevel) {
    fprintf(stderr, "Failed to allocate memory for toplevel\n");
    return;
  }

  toplevel->id = global_id;
  global_id++;

  toplevel->zwlr_toplevel = zwlr_toplevel;
  toplevel->current.parent_id = no_parent;
  toplevel->pending.parent_id = no_parent;

  wl_list_insert(&toplevel_list, &toplevel->link);

  struct toplevel_info new_info = {.tl_id = toplevel->id,
                                   .tl_title = NULL,
                                   .tl_app_id = NULL,
                                   .tl_parent_id = no_parent,
                                   .maximized = false,
                                   .minimized = false,
                                   .active = false,
                                   .fullscreen = false};

  add_toplevel_info(&global_info_list, &new_info);

  zwlr_foreign_toplevel_handle_v1_add_listener(zwlr_toplevel, &toplevel_impl,
                                               toplevel);
}

static void toplevel_manager_handle_finished(
    void *data, struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager) {
  zwlr_foreign_toplevel_manager_v1_destroy(toplevel_manager);
}

static const struct zwlr_foreign_toplevel_manager_v1_listener
    toplevel_manager_impl = {
        .toplevel = toplevel_manager_handle_toplevel,
        .finished = toplevel_manager_handle_finished,
};

static void handle_global( void *data,
                          struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {

  if (strcmp(interface, wl_output_interface.name) == 0) {
    if (name == pref_output_id) {
      pref_output =
          wl_registry_bind(registry, name, &wl_output_interface, version);
    }
  } else if (strcmp(interface,
                    zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
    toplevel_manager = wl_registry_bind(
        registry, name, &zwlr_foreign_toplevel_manager_v1_interface,
        WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION);

    wl_list_init(&toplevel_list);
    zwlr_foreign_toplevel_manager_v1_add_listener(toplevel_manager,
                                                  &toplevel_manager_impl, NULL);
  } else if (strcmp(interface, wl_seat_interface.name) == 0 && seat == NULL) {
    seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
  }
}

static void handle_global_remove( void *data,
                                  struct wl_registry *registry,
                                  uint32_t name) {
  // who cares
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

static struct toplevel_v1 *toplevel_by_id_or_bail(int32_t id) {
  if (id == -1) {
    return NULL;
  }

  struct toplevel_v1 *toplevel;
  wl_list_for_each(toplevel, &toplevel_list, link) {
    if (toplevel->id == (uint32_t)id) {
      return toplevel;
    }
  }
  return 0;
}

// ---- Unix Socket Event Handler ---- //

void handle_event(int client_fd, const char *event_data) {

  int focus_id = -1, close_id = -1;
  int maximize_id = -1, unmaximize_id = -1;
  int minimize_id = -1, restore_id = -1;
  int fullscreen_id = -1, unfullscreen_id = -1;

  if (event_data == NULL || *event_data == '\0') {
    fprintf(stderr, "Received empty data event from client %d.\n", client_fd);
    return;
  }

  if ((strlen(event_data) < 3 && event_data[0] != 'q') || (event_data[1] != ' ' && event_data[0] != 'q')) {
    fprintf(stderr,
            "Invalid event data format from client %d: '%s'. Expected 'opt "
            "<id>'.\n",
            client_fd, event_data);
    print_help();
    return;
  }

  char command_char = event_data[0];
  const char *argument_str = event_data + 2;

  char *endptr;
  uint32_t window_id;

  window_id = strtol(argument_str, &endptr, 10);

  if (endptr == argument_str) {
    fprintf(stderr, "Error: no digits found in argument '%s' from client %d.\n",
            argument_str, client_fd);
    print_help();
    return;
  }

  if (*endptr != '\0' && !isspace((unsigned char)*endptr)) {
    fprintf(stderr,
            "Error: Invalid characters in argument '%s' after number for "
            "client %d.\n",
            argument_str, client_fd);
    print_help();
    return;
  }

  switch (command_char) {
  case 'q':
    if (sort_out) {
      sort_out = false;
    } else {
      sort_out = true;
    }
    break;
  case 'f':
    focus_id = window_id;
    break;
  case 'a':
    maximize_id = window_id;
    break;
  case 'u':
    unmaximize_id = window_id;
    break;
  case 'i':
    minimize_id = window_id;
    break;
  case 'r':
    restore_id = window_id;
    break;
  case 'c':
    close_id = window_id;
    break;
  case 's':
    fullscreen_id = window_id;
    break;
  case 'S':
    unfullscreen_id = window_id;
    break;
  case '?':
    print_help();
    return;
  }

  struct toplevel_v1 *toplevel;
  if ((toplevel = toplevel_by_id_or_bail(focus_id))) {
    zwlr_foreign_toplevel_handle_v1_activate(toplevel->zwlr_toplevel, seat);
  }
  if ((toplevel = toplevel_by_id_or_bail(maximize_id))) {
    zwlr_foreign_toplevel_handle_v1_set_maximized(toplevel->zwlr_toplevel);
  }
  if ((toplevel = toplevel_by_id_or_bail(unmaximize_id))) {
    zwlr_foreign_toplevel_handle_v1_unset_maximized(toplevel->zwlr_toplevel);
  }
  if ((toplevel = toplevel_by_id_or_bail(minimize_id))) {
    zwlr_foreign_toplevel_handle_v1_set_minimized(toplevel->zwlr_toplevel);
  }
  if ((toplevel = toplevel_by_id_or_bail(restore_id))) {
    zwlr_foreign_toplevel_handle_v1_unset_minimized(toplevel->zwlr_toplevel);
  }
  if ((toplevel = toplevel_by_id_or_bail(fullscreen_id))) {
    if (pref_output_id != UINT32_MAX && pref_output == NULL) {
      fprintf(stderr, "Could not find output %i\n", pref_output_id);
    }
    zwlr_foreign_toplevel_handle_v1_set_fullscreen(toplevel->zwlr_toplevel,
                                                   pref_output);
  }
  if ((toplevel = toplevel_by_id_or_bail(unfullscreen_id))) {
    zwlr_foreign_toplevel_handle_v1_unset_fullscreen(toplevel->zwlr_toplevel);
  }
  if ((toplevel = toplevel_by_id_or_bail(close_id))) {
    zwlr_foreign_toplevel_handle_v1_close(toplevel->zwlr_toplevel);
  }
}

// ---- Main Function ---- //

int main(int argc, char **argv) {
  int listen_socket = -1, client_socket = -1;
  struct sockaddr_un server_addr;
  struct pollfd fds[MAX_CLIENTS + 2];
  int nfds = 0;
  socklen_t client_addr_len;
  const char *event_message = NULL;
  int focus_id = -1, close_id = -1;
  int maximize_id = -1, unmaximize_id = -1;
  int minimize_id = -1, restore_id = -1;
  int fullscreen_id = -1, unfullscreen_id = -1;
  int one_shot = 1;
  int client_mode = 0;
  int c;

  // Initialize fds entries to -1
  for (int i = 0; i < MAX_CLIENTS + 2; i++) {
    fds[i].fd = -1;
  }

  while ((c = getopt(argc, argv, "f:a:u:i:r:c:s:S:mo:h:mjqx")) != -1) {
    switch (c) {
    case 'q':
      sort_out = true;
      break;
    case 'f':
      focus_id = atoi(optarg);
      break;
    case 'a':
      maximize_id = atoi(optarg);
      break;
    case 'u':
      unmaximize_id = atoi(optarg);
      break;
    case 'i':
      minimize_id = atoi(optarg);
      break;
    case 'r':
      restore_id = atoi(optarg);
      break;
    case 'c':
      close_id = atoi(optarg);
      break;
    case 's':
      fullscreen_id = atoi(optarg);
      break;
    case 'S':
      unfullscreen_id = atoi(optarg);
      break;
    case 'm':
      one_shot = 0; // Server mode
      break;
    case 'j':
      json_out = true; // Use boolean true
      break;
    case 'x':
      // Check if there's an argument after -x
      if (optind < argc && argv[optind] != NULL) {
        event_message = argv[optind];
        client_mode = 1; // Client mode
      } else {
        fprintf(stderr, "Option -x requires an argument, see -h for more.\n");
        print_help();
        return EXIT_FAILURE;
      }
      // Increment optind to consume the argument
      optind++;
      break;
    case 'o':
      pref_output_id = atoi(optarg);
      break;
    case '?':
      print_help();
      return EXIT_FAILURE;
      break;
    case 'h':
      print_help();
      return EXIT_SUCCESS;
      break;
    }
  }

  if (one_shot == 0) { // Server mode

    global_display = wl_display_connect(NULL);
    if (global_display == NULL) {
      fprintf(stderr, "Failed to connect to Wayland display.\n");
      return EXIT_FAILURE;
    }
    // printf("Connected to Wayland display.\n");

    struct wl_registry *registry = wl_display_get_registry(global_display);
    if (registry == NULL) {
      fprintf(stderr, "Failed to get Wayland registry.\n");
      wl_display_disconnect(global_display);
      return EXIT_FAILURE;
    }
    wl_registry_add_listener(registry, &registry_listener, NULL);

    // Initial Wayland dispatch to get global objects
    if (wl_display_roundtrip(global_display) == -1) {
      fprintf(stderr, "Wayland initial roundtrip failed.\n");
      wl_registry_destroy(registry);
      wl_display_disconnect(global_display);
      return EXIT_FAILURE;
    }
    //printf("Wayland initial roundtrip complete.\n");

    // Check if toplevel_manager is available after the roundtrip
    if (toplevel_manager == NULL) {
      fprintf(stderr, "wlr-foreign-toplevel not available\n");
      wl_registry_destroy(registry);
      wl_display_disconnect(global_display);
      return EXIT_FAILURE;
    }

    // Another roundtrip to load toplevel details after binding to
    // toplevel_manager
    if (wl_display_roundtrip(global_display) == -1) {

      fprintf(stderr, "Wayland second roundtrip failed.\n");
      wl_registry_destroy(registry);
      wl_display_disconnect(global_display);

      return EXIT_FAILURE;
    }
    //printf("Wayland second roundtrip complete (toplevel details loaded).\n");

    listen_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_socket == -1) {
      perror("Error creating socket");
      wl_registry_destroy(registry);
      wl_display_disconnect(global_display);
      exit(EXIT_FAILURE);
    }

    int wayland_fd = wl_display_get_fd(global_display);
    if (wayland_fd < 0) {
      fprintf(stderr, "Failed to get Wayland display file descriptor. \n");
      close(listen_socket);
      wl_registry_destroy(registry);
      wl_display_disconnect(global_display);
      exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH,
            sizeof(server_addr.sun_path) - 1);

    unlink(SOCKET_PATH); // Remove the socket file if it already exists

    if (bind(listen_socket, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) == -1) {
      perror("Error binding socket");
      close(listen_socket);
      wl_registry_destroy(registry);
      wl_display_disconnect(global_display);
      exit(EXIT_FAILURE);
    }

    if (listen(listen_socket, 5) == -1) {
      perror("Error listening on a socket");
      close(listen_socket);
      wl_registry_destroy(registry);
      wl_display_disconnect(global_display);
      exit(EXIT_FAILURE);
    }

    // printf("Listening for connections on socket: %s\n", SOCKET_PATH);

    // Add listening socket and Wayland FD to pollfds
    fds[0].fd = listen_socket;
    fds[0].events = POLLIN;
    nfds++;

    fds[1].fd = wayland_fd;
    fds[1].events = POLLIN;
    nfds++;

    wl_display_flush(global_display);

    int running = 1;
    while (running) {
      client_addr_len = sizeof(struct sockaddr_un);

      // printf("listening for wayland/socket events...\n");
      // Wait for events on monitored file descriptors (sockets and Wayland)
      int poll_count = poll(fds, nfds, -1);

      if (poll_count == -1) {
        if (errno == EINTR) {
          continue; // Interrupted by signal, continue.
        }
        perror("poll error");
        running = 0; // Exit loop on poll error.
        break;
      }

      // Process events on file descriptors
      for (int i = 0; i < nfds; i++) {

        // Check if the descriptor is valid and has events
        if (fds[i].fd >= 0 && (fds[i].revents & POLLIN)) {

          if (i == 0) {

            // Event on the listening socket
            struct sockaddr_un client_addr;
            socklen_t current_client_addr_len = sizeof(client_addr);

            int client_socket =
                accept(listen_socket, (struct sockaddr *)&client_addr,
                       &current_client_addr_len);

            if (client_socket == -1) {
              perror("Error accepting connection");
              continue; // Continue processing other events
            }

            // printf("Client connected (fd: %d).\n", client_socket);

            // Add the new client socket to the list of monitored fds
            int j;

            for (j = 2; j < MAX_CLIENTS + 2; ++j) {
              if (fds[j].fd == -1) { // Find the first unused slot

                fds[j].fd = client_socket;
                fds[j].events = POLLIN;

                if (j >=
                    nfds) { // Update nfds if we added beyond the current count

                  nfds = j + 1;
                }
                break;
              }
            }

            if (j >= MAX_CLIENTS + 2) {

              fprintf(
                  stderr,
                  "Maximum number of clients reached. Connection rejected.\n");
              close(client_socket); // Close the new connection immediately
            }

          } else if (i == 1) {

            if (wl_display_dispatch(global_display) == -1) {
              fprintf(stderr, "Wayland display disconnected.\n");
              running = 0; // Exit the loop on Wayland disconnection
              break;       // Exit the inner for loop as well
            }

            // After dispatching, flush any pending requests to the compositor
            wl_display_flush(global_display);

            if (json_out) {
              print_toplevel_json_array();
            }

          } else {

            // Event on a client socket
            char buffer[BUFFER_SIZE];
            int bytes_received = recv(fds[i].fd, buffer, BUFFER_SIZE - 1, 0);

            if (bytes_received > 0) {

              buffer[bytes_received] = '\0';
              handle_event(fds[i].fd, buffer);
              wl_display_flush(global_display);

              // Since the client is only meant to send one event and exit, we
              // close the socket right away to avoid it being left open.
              close(client_socket);
              fds[i].fd = -1; // Mark slot as unused
              // Adjust nfds if the highest index fd disconnected
              while (nfds > 2 &&
                     fds[nfds - 1].fd == -1) { // Start checking from index 2
                nfds--;
              }

            } else if (bytes_received == 0) {

              // Client disconnected
              printf("Client disconnected (fd: %d).\n", fds[i].fd);
              close(fds[i].fd);

              fds[i].fd = -1;
              while (nfds > 2 &&
                     fds[nfds - 1].fd == -1) { // Start checking from index 2
                nfds--;
              }

            } else {

              // Error receiving data
              if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Error receiving data.");
              }
              close(fds[i].fd);
              fds[i].fd = -1;
              while (nfds > 2 && fds[nfds - 1].fd == -1) {
                nfds--;
              }
            }
          }
        }

        // Check for other events like POLLERR, POLLHUP, etc. on any valid fd
        if (fds[i].fd >= 0 &&
            (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))) {
          fprintf(stderr, "Error or hang-up on fd %d (events: %x). Closing.\n",
                  fds[i].fd, fds[i].revents);
          close(fds[i].fd);
          fds[i].fd = -1;
          while (nfds > 2 && fds[nfds - 1].fd == -1) {
            nfds--;
          }
        }
      }
    }
  } else if (client_mode == 1) {

    // Client mode
    client_socket = socket(AF_UNIX, SOCK_STREAM, 0);

    if (client_socket == -1) {
      perror("Error creating client socket.");
      exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH,
            sizeof(server_addr.sun_path) - 1);

    if (connect(client_socket, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == -1) {
      perror("Error connecting to socket");
      close(client_socket);
      exit(EXIT_FAILURE);
    }

    printf("Connected to socket: %s\n", SOCKET_PATH);

    if (event_message != NULL &&
        send(client_socket, event_message, strlen(event_message), 0) == -1) {
      perror("Error sending data");
      close(client_socket);
      exit(EXIT_FAILURE);
    }

    printf("Sent Message: %s\n", event_message);

  } else {
    struct wl_display *display = wl_display_connect(NULL);
    if (display == NULL) {
      fprintf(stderr, "Failed to create display\n");
      return EXIT_FAILURE;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (toplevel_manager == NULL) {
      fprintf(stderr, "wlr-foreign-toplevel not available\n");
      return EXIT_FAILURE;
    }
    wl_display_roundtrip(display); // load list of toplevels
    wl_display_roundtrip(display); // load toplevel details

    struct toplevel_v1 *toplevel;
    if ((toplevel = toplevel_by_id_or_bail(focus_id))) {
      zwlr_foreign_toplevel_handle_v1_activate(toplevel->zwlr_toplevel, seat);
    }
    if ((toplevel = toplevel_by_id_or_bail(maximize_id))) {
      zwlr_foreign_toplevel_handle_v1_set_maximized(toplevel->zwlr_toplevel);
    }
    if ((toplevel = toplevel_by_id_or_bail(unmaximize_id))) {
      zwlr_foreign_toplevel_handle_v1_unset_maximized(toplevel->zwlr_toplevel);
    }
    if ((toplevel = toplevel_by_id_or_bail(minimize_id))) {
      zwlr_foreign_toplevel_handle_v1_set_minimized(toplevel->zwlr_toplevel);
    }
    if ((toplevel = toplevel_by_id_or_bail(restore_id))) {
      zwlr_foreign_toplevel_handle_v1_unset_minimized(toplevel->zwlr_toplevel);
    }
    if ((toplevel = toplevel_by_id_or_bail(fullscreen_id))) {
      if (pref_output_id != UINT32_MAX && pref_output == NULL) {
        fprintf(stderr, "Could not find output %i\n", pref_output_id);
      }

      zwlr_foreign_toplevel_handle_v1_set_fullscreen(toplevel->zwlr_toplevel,
                                                     pref_output);
    }
    if ((toplevel = toplevel_by_id_or_bail(unfullscreen_id))) {
      zwlr_foreign_toplevel_handle_v1_unset_fullscreen(toplevel->zwlr_toplevel);
    }
    if ((toplevel = toplevel_by_id_or_bail(close_id))) {
      zwlr_foreign_toplevel_handle_v1_close(toplevel->zwlr_toplevel);
    }

    wl_display_flush(display);

    if (json_out) {
      print_toplevel_json_array();
    }
  }

  // Close all active file descriptors
  if (one_shot == 0 || client_mode == 1) {
    for (int i = 0; i < MAX_CLIENTS + 2; ++i) {
      if (fds[i].fd >= 0) {
        close(fds[i].fd);
        fds[i].fd = -1;
      }
    }

    // Close listen socket if it was opened
    if (listen_socket != -1) {
      close(listen_socket);
    }
    // Close client socket if it was opened in client mode
    if (client_socket != -1) {
      // sleep(0.2);
      close(client_socket);
    }

    // Remove the socket file if it exists and we were in server mode
    if (one_shot == 0) {
      unlink(SOCKET_PATH);
    }
  }
  return EXIT_SUCCESS;
}
