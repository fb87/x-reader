/* app.h - the X-Reader application, built only on the xr framework. */
#ifndef APP_H
#define APP_H

#include "xr/xr.h"

const xr_theme_t *app_theme(void);
void app_start(xr_shell_t *shell);

#endif /* APP_H */
