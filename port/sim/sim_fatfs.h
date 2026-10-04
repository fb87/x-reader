#ifndef SIM_FATFS_H
#define SIM_FATFS_H

#include <stdint.h>
#include <stdio.h>

typedef uint32_t UINT;
typedef uint32_t FSIZE_t;
typedef enum { FR_OK = 0, FR_DISK_ERR, FR_NO_FILE } FRESULT;
typedef struct { FILE *handle; FSIZE_t size; } FIL;
typedef struct { void *handle; } FDIR;
typedef struct { char fname[256]; } FILINFO;

#define FA_READ 0x01u

void sim_fatfs_mount(const char *root);
FRESULT f_open(FIL *file, const char *path, uint8_t mode);
FRESULT f_read(FIL *file, void *buffer, UINT size, UINT *read);
FRESULT f_lseek(FIL *file, FSIZE_t offset);
FRESULT f_close(FIL *file);
FSIZE_t f_size(const FIL *file);
FRESULT f_opendir(FDIR *dir, const char *path);
FRESULT f_readdir(FDIR *dir, FILINFO *info);
FRESULT f_closedir(FDIR *dir);

#endif
