// The web build only: Emscripten's SDL3 port has no file dialogs. lba2cc's
// SDL3 host links the folder picker, which the prototype never opens (the page
// hands it the game files); say "no dialog" as SDL does on an error.
#include <SDL3/SDL_dialog.h>

extern "C" void SDL_ShowOpenFolderDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window,
                                         const char *default_location, bool allow_many) {
    (void)window;
    (void)default_location;
    (void)allow_many;
    if (callback)
        callback(userdata, NULL, -1);
}
