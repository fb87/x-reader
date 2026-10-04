/* app.h - the X-Reader application, built only on the xr framework. */
#ifndef APP_H
#define APP_H

#include "xr/xr.h"

const xr_theme_t *app_theme(void);
void app_start(xr_shell_t *shell);
typedef bool (*app_epub_loader_fn)(const char *path, const char *title);
typedef bool (*app_dir_entry_fn)(void *context, const char *path,
                                 bool (*entry)(const char *name, bool directory, void *user),
                                 void *user);
void app_library_clear(void);
bool app_library_add(const char *title);
bool app_library_add_path(const char *path, const char *title);
void app_set_epub_loader(app_epub_loader_fn loader);
void app_register_storage(const char *root, app_dir_entry_fn list_dir, void *context);
void app_scan_library(const char *root, app_dir_entry_fn list_dir, void *context);
bool app_load_epub(const xr_storage_t *storage, const char *title);

#endif /* APP_H */
