#ifndef PC98FM_FS_H
#define PC98FM_FS_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define FS_PATH_MAX 4096
#define FS_NAME_MAX 512
#define FS_MAX_ENTRIES 4096

typedef struct {
  char name[FS_NAME_MAX];
  int is_dir;
  int is_executable;
  int stat_ok;
  int64_t size;
  time_t mtime;
} FsEntry;

typedef struct {
  FsEntry entries[FS_MAX_ENTRIES];
  size_t count;
  size_t file_count;
  uint64_t total_size;
  int truncated;
} FsDirectory;

typedef struct {
  char name[FS_NAME_MAX];
  char path[FS_PATH_MAX];
} FsDrive;

#define FS_MAX_DRIVES 64

int fs_get_current_path(char *buffer, size_t size);
int fs_list_drives(FsDrive *drives, size_t maximum, size_t *count);
int fs_join_path(char *buffer, size_t size,
                 const char *directory, const char *name);
int fs_parent_path(char *path, size_t size);
int fs_path_basename(const char *path, char *buffer, size_t size);
int fs_load_directory(const char *path, FsDirectory *directory);
uint64_t fs_free_space(const char *path);
int fs_path_exists(const char *path);
int fs_path_is_directory(const char *path);
int fs_execute(const char *path, const char *working_directory);
int fs_copy(const char *source, const char *destination);
int fs_rename(const char *source, const char *destination);
int fs_make_directory(const char *path);
int fs_remove(const char *path);

#endif
