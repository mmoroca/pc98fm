#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "fs.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/mount.h>
#elif defined(__linux__)
#include <mntent.h>
#endif

static int add_drive(FsDrive *drives, size_t maximum, size_t *count,
                     const char *path) {
  if (*count >= maximum || path[0] == '\0') return 0;

  for (size_t i = 0; i < *count; i++) {
    if (strcmp(drives[i].path, path) == 0) return 1;
  }

  strncpy(drives[*count].path, path, sizeof(drives[*count].path) - 1);
  drives[*count].path[sizeof(drives[*count].path) - 1] = '\0';
  strncpy(drives[*count].name, path, sizeof(drives[*count].name) - 1);
  drives[*count].name[sizeof(drives[*count].name) - 1] = '\0';
  (*count)++;
  return 1;
}

static int is_user_mount(const char *path) {
  static const char *prefixes[] = {
    "/Volumes/", "/media/", "/run/media/", "/mnt/"
  };

  if (strcmp(path, "/") == 0 || strcmp(path, "/Volumes") == 0 ||
      strcmp(path, "/media") == 0 || strcmp(path, "/run/media") == 0 ||
      strcmp(path, "/mnt") == 0)
    return 1;

  for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
    size_t length = strlen(prefixes[i]);
    if (strncmp(path, prefixes[i], length) == 0) return 1;
  }

  return 0;
}

int fs_get_current_path(char *buffer, size_t size) {
  return getcwd(buffer, size) != NULL;
}

int fs_list_drives(FsDrive *drives, size_t maximum, size_t *count) {
  if (!count || maximum == 0) return 0;
  *count = 0;
  add_drive(drives, maximum, count, "/");

#if defined(__APPLE__)
  {
    struct statfs *mounts;
    int mount_count = getmntinfo(&mounts, MNT_NOWAIT);

    for (int i = 0; i < mount_count; i++) {
      if (is_user_mount(mounts[i].f_mntonname))
        add_drive(drives, maximum, count, mounts[i].f_mntonname);
    }
  }
#elif defined(__linux__)
  {
    FILE *mounts = setmntent("/proc/self/mounts", "r");
    struct mntent *mount;

    if (mounts) {
      while ((mount = getmntent(mounts)) != NULL) {
        if (is_user_mount(mount->mnt_dir))
          add_drive(drives, maximum, count, mount->mnt_dir);
      }
      endmntent(mounts);
    }
  }
#endif

  return *count > 0;
}

int fs_join_path(char *buffer, size_t size,
                 const char *directory, const char *name) {
  size_t directory_length = strlen(directory);
  size_t name_length = strlen(name);
  int separator = directory_length > 0 &&
                  directory[directory_length - 1] != '/';
  size_t required = directory_length + (size_t)separator + name_length + 1;

  if (required > size) return 0;

  memcpy(buffer, directory, directory_length);
  if (separator) buffer[directory_length++] = '/';
  memcpy(buffer + directory_length, name, name_length + 1);
  return 1;
}

int fs_parent_path(char *path, size_t size) {
  size_t length;
  char *separator;

  (void)size;
  length = strlen(path);
  if (length == 0) return 0;

  while (length > 1 && path[length - 1] == '/') path[--length] = '\0';
  if (length == 1) return 1;

  separator = strrchr(path, '/');
  if (!separator || separator == path) {
    path[0] = '/';
    path[1] = '\0';
  } else {
    *separator = '\0';
  }
  return 1;
}

int fs_path_basename(const char *path, char *buffer, size_t size) {
  size_t length = strlen(path);
  const char *start;

  if (size == 0) return 0;
  if (length == 1 && path[0] == '/') {
    buffer[0] = '\0';
    return 1;
  }
  while (length > 1 && path[length - 1] == '/') length--;
  start = path + length;
  while (start > path && start[-1] != '/') start--;

  if (length - (size_t)(start - path) >= size) return 0;
  memcpy(buffer, start, length - (size_t)(start - path));
  buffer[length - (size_t)(start - path)] = '\0';
  return 1;
}

int fs_load_directory(const char *path, FsDirectory *directory) {
  DIR *dir = opendir(path);
  struct dirent *ent;

  /* Leave the previous contents untouched if opening fails. */
  if (!dir) return 0;

  directory->count = 0;
  directory->file_count = 0;
  directory->total_size = 0;
  directory->truncated = 0;

  while ((ent = readdir(dir)) != NULL) {
    FsEntry *entry;
    char fullpath[FS_PATH_MAX];
    struct stat st;

    if (directory->count >= FS_MAX_ENTRIES) {
      directory->truncated = 1;
      break;
    }

    entry = &directory->entries[directory->count];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, ent->d_name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';

    if (strcmp(entry->name, ".") != 0 && strcmp(entry->name, "..") != 0)
      directory->file_count++;

    if (fs_join_path(fullpath, sizeof(fullpath), path, ent->d_name) &&
        stat(fullpath, &st) == 0) {
      entry->stat_ok = 1;
      entry->is_dir = S_ISDIR(st.st_mode);
      entry->is_executable = !entry->is_dir &&
                             (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
      entry->size = st.st_size < 0 ? 0 : (int64_t)st.st_size;
      entry->mtime = st.st_mtime;

      if (!entry->is_dir && st.st_size > 0)
        directory->total_size += (uint64_t)st.st_size;
    }

    directory->count++;
  }

  closedir(dir);
  return 1;
}

uint64_t fs_free_space(const char *path) {
  struct statvfs vfs;

  if (statvfs(path, &vfs) != 0) return 0;
  return (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_frsize;
}

int fs_path_exists(const char *path) {
  struct stat st;
  return lstat(path, &st) == 0;
}

int fs_path_is_directory(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int fs_execute(const char *path, const char *working_directory) {
  pid_t child;
  int status;
  void (*old_interrupt)(int);
  void (*old_quit)(int);

  child = fork();
  if (child < 0) return 0;

  if (child == 0) {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    if (working_directory && chdir(working_directory) != 0)
      _exit(126);

    execl(path, path, (char *)NULL);
    _exit(errno == EACCES ? 126 : 127);
  }

  /* Ctrl-C should stop the foreground child, not the file manager. */
  old_interrupt = signal(SIGINT, SIG_IGN);
  old_quit = signal(SIGQUIT, SIG_IGN);
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) {
      signal(SIGINT, old_interrupt);
      signal(SIGQUIT, old_quit);
      return 0;
    }
  }
  signal(SIGINT, old_interrupt);
  signal(SIGQUIT, old_quit);

  if (!WIFEXITED(status)) return 0;
  return WEXITSTATUS(status) == 0;
}

static int copy_file(const char *source, const char *destination) {
  FILE *input = fopen(source, "rb");
  FILE *output;
  unsigned char buffer[8192];
  size_t bytes_read;
  int success = 1;

  if (!input) return 0;

  output = fopen(destination, "wb");
  if (!output) {
    fclose(input);
    return 0;
  }

  while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
    if (fwrite(buffer, 1, bytes_read, output) != bytes_read) {
      success = 0;
      break;
    }
  }

  if (ferror(input)) success = 0;
  if (fclose(input) != 0) success = 0;
  if (fclose(output) != 0) success = 0;
  return success;
}

static int path_is_inside(const char *parent, const char *child) {
  size_t length = strlen(parent);

  if (strcmp(parent, child) == 0) return 1;
  if (length > 0 && parent[length - 1] == '/')
    return strncmp(parent, child, length) == 0;
  return strncmp(parent, child, length) == 0 && child[length] == '/';
}

static int copy_recursive(const char *source, const char *destination) {
  struct stat source_stat;
  struct stat destination_stat;

  if (lstat(source, &source_stat) != 0) return 0;

  if (S_ISDIR(source_stat.st_mode)) {
    DIR *dir;
    struct dirent *ent;
    int success = 1;

    if (path_is_inside(source, destination)) return 0;

    if (lstat(destination, &destination_stat) == 0) {
      if (S_ISLNK(destination_stat.st_mode) ||
          !S_ISDIR(destination_stat.st_mode))
        return 0;
    } else if (mkdir(destination, source_stat.st_mode & 0777) != 0) {
      return 0;
    }

    dir = opendir(source);
    if (!dir) return 0;

    while ((ent = readdir(dir)) != NULL) {
      char source_child[FS_PATH_MAX];
      char destination_child[FS_PATH_MAX];

      if (strcmp(ent->d_name, ".") == 0 ||
          strcmp(ent->d_name, "..") == 0)
        continue;

      if (!fs_join_path(source_child, sizeof(source_child),
                        source, ent->d_name) ||
          !fs_join_path(destination_child, sizeof(destination_child),
                        destination, ent->d_name) ||
          !copy_recursive(source_child, destination_child)) {
        success = 0;
        break;
      }
    }

    closedir(dir);
    return success;
  }

  if (lstat(destination, &destination_stat) == 0 &&
      (S_ISDIR(destination_stat.st_mode) ||
       S_ISLNK(destination_stat.st_mode)))
    return 0;

  return copy_file(source, destination);
}

int fs_copy(const char *source, const char *destination) {
  return copy_recursive(source, destination);
}

int fs_rename(const char *source, const char *destination) {
  return rename(source, destination) == 0;
}

int fs_make_directory(const char *path) {
  return mkdir(path, 0777) == 0;
}

int fs_remove(const char *path) {
  struct stat st;

  if (lstat(path, &st) != 0) return 0;

  if (S_ISDIR(st.st_mode)) {
    DIR *dir = opendir(path);
    struct dirent *ent;
    int success = 1;

    if (!dir) return 0;

    while ((ent = readdir(dir)) != NULL) {
      char child[FS_PATH_MAX];

      if (strcmp(ent->d_name, ".") == 0 ||
          strcmp(ent->d_name, "..") == 0)
        continue;

      if (!fs_join_path(child, sizeof(child), path, ent->d_name) ||
          !fs_remove(child)) {
        success = 0;
        break;
      }
    }

    closedir(dir);
    if (!success) return 0;
    return rmdir(path) == 0;
  }

  return unlink(path) == 0;
}
