#include "sim_fatfs.h"

#include <string.h>

static const char *s_root = ".";
void sim_fatfs_mount(const char *root) { s_root = root ? root : "."; }
FRESULT f_open(FIL *file, const char *path, uint8_t mode)
{
    char full[512];
    if (!file || !path || !(mode & FA_READ)) return FR_NO_FILE;
    if (path[0] == '/') snprintf(full, sizeof full, "%s", path);
    else snprintf(full, sizeof full, "%s/%s", s_root, path);
    file->handle = fopen(full, "rb");
    if (!file->handle) return FR_NO_FILE;
    fseek(file->handle, 0, SEEK_END); file->size = (FSIZE_t)ftell(file->handle); fseek(file->handle, 0, SEEK_SET);
    return FR_OK;
}
FRESULT f_read(FIL *file, void *buffer, UINT size, UINT *read)
{ *read = (UINT)fread(buffer, 1, size, file->handle); return ferror(file->handle) ? FR_DISK_ERR : FR_OK; }
FRESULT f_lseek(FIL *file, FSIZE_t offset) { return fseek(file->handle, offset, SEEK_SET) ? FR_DISK_ERR : FR_OK; }
FRESULT f_close(FIL *file) { return fclose(file->handle) ? FR_DISK_ERR : FR_OK; }
FSIZE_t f_size(const FIL *file) { return file->size; }
