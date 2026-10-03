#ifdef _WIN32

#define _CRT_SECURE_NO_WARNINGS

#include "fs.h"

#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>

static int is_separator(char value) {
  return value == '\\' || value == '/';
}

static int is_directory(const struct _stat64 *st) {
  return (st->st_mode & _S_IFDIR) != 0;
}

static int has_executable_extension(const char *name) {
  const char *extension = strrchr(name, '.');

  if (!extension) return 0;
  return _stricmp(extension, ".exe") == 0 ||
         _stricmp(extension, ".com") == 0 ||
         _stricmp(extension, ".bat") == 0 ||
         _stricmp(extension, ".cmd") == 0;
}

static time_t file_time_to_time_t(FILETIME value) {
  ULARGE_INTEGER integer;
  const unsigned long long epoch = 116444736000000000ULL;

  integer.LowPart = value.dwLowDateTime;
  integer.HighPart = value.dwHighDateTime;
  if (integer.QuadPart < epoch) return (time_t)0;
  return (time_t)((integer.QuadPart - epoch) / 10000000ULL);
}

int fs_get_current_path(char *buffer, size_t size) {
  return _getcwd(buffer, (int)size) != NULL;
}

int fs_list_drives(FsDrive *drives, size_t maximum, size_t *count) {
  if (!count || maximum == 0) return 0;
  *count = 0;

  for (char letter = 'A'; letter <= 'Z' && *count < maximum; letter++) {
    char root[] = "A:\\";
    UINT type;

    root[0] = letter;
    type = GetDriveTypeA(root);
    if (type == DRIVE_NO_ROOT_DIR || type == DRIVE_UNKNOWN) continue;

    snprintf(drives[*count].name, sizeof(drives[*count].name), "%c:", letter);
    strncpy(drives[*count].path, root, sizeof(drives[*count].path) - 1);
    drives[*count].path[sizeof(drives[*count].path) - 1] = '\0';
    (*count)++;
  }

  return *count > 0;
}

int fs_join_path(char *buffer, size_t size,
                 const char *directory, const char *name) {
  size_t directory_length = strlen(directory);
  size_t name_length = strlen(name);
  int separator = directory_length > 0 &&
                  !is_separator(directory[directory_length - 1]);
  size_t required = directory_length + (size_t)separator + name_length + 1;

  if (required > size) return 0;
  memcpy(buffer, directory, directory_length);
  if (separator) buffer[directory_length++] = '\\';
  memcpy(buffer + directory_length, name, name_length + 1);
  return 1;
}

int fs_parent_path(char *path, size_t size) {
  size_t length = strlen(path);
  char *separator;

  (void)size;
  while (length > 3 && is_separator(path[length - 1]))
    path[--length] = '\0';

  if (length <= 3 && length >= 2 && path[1] == ':') return 1;
  separator = strrchr(path, '\\');
  {
    char *slash = strrchr(path, '/');
    if (!separator || (slash && slash > separator)) separator = slash;
  }

  if (!separator) return 0;
  if (separator == path + 2 && path[1] == ':') {
    path[3] = '\0';
  } else if (separator == path) {
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
  while (length > 3 && is_separator(path[length - 1])) length--;
  if ((length == 3 && path[1] == ':') || length == 0) {
    buffer[0] = '\0';
    return 1;
  }

  start = path + length;
  while (start > path && !is_separator(start[-1])) start--;
  if (length - (size_t)(start - path) >= size) return 0;
  memcpy(buffer, start, length - (size_t)(start - path));
  buffer[length - (size_t)(start - path)] = '\0';
  return 1;
}

static void add_special_entry(FsDirectory *directory, const char *name) {
  FsEntry *entry = &directory->entries[directory->count++];
  memset(entry, 0, sizeof(*entry));
  strcpy(entry->name, name);
  entry->is_dir = 1;
}

int fs_load_directory(const char *path, FsDirectory *directory) {
  char pattern[FS_PATH_MAX];
  WIN32_FIND_DATAA data;
  HANDLE handle;

  if (!fs_join_path(pattern, sizeof(pattern), path, "*")) return 0;
  handle = FindFirstFileA(pattern, &data);
  if (handle == INVALID_HANDLE_VALUE) return 0;

  directory->count = 0;
  directory->file_count = 0;
  directory->total_size = 0;
  directory->truncated = 0;
  add_special_entry(directory, ".");
  add_special_entry(directory, "..");

  do {
    FsEntry *entry;
    ULARGE_INTEGER file_size;

    if (strcmp(data.cFileName, ".") == 0 ||
        strcmp(data.cFileName, "..") == 0)
      continue;

    if (directory->count >= FS_MAX_ENTRIES) {
      directory->truncated = 1;
      break;
    }

    entry = &directory->entries[directory->count++];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, data.cFileName, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->is_dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    entry->is_executable = !entry->is_dir &&
                           has_executable_extension(entry->name);
    entry->stat_ok = 1;
    entry->mtime = file_time_to_time_t(data.ftLastWriteTime);
    file_size.LowPart = data.nFileSizeLow;
    file_size.HighPart = data.nFileSizeHigh;
    entry->size = entry->is_dir ? 0 : (int64_t)file_size.QuadPart;

    directory->file_count++;
    if (!entry->is_dir) directory->total_size += file_size.QuadPart;
  } while (FindNextFileA(handle, &data));

  FindClose(handle);
  return 1;
}

uint64_t fs_free_space(const char *path) {
  ULARGE_INTEGER available;

  if (!GetDiskFreeSpaceExA(path, &available, NULL, NULL)) return 0;
  return (uint64_t)available.QuadPart;
}

int fs_path_exists(const char *path) {
  struct _stat64 st;
  return _stat64(path, &st) == 0;
}

int fs_path_is_directory(const char *path) {
  struct _stat64 st;
  return _stat64(path, &st) == 0 && is_directory(&st);
}

int fs_execute(const char *path, const char *working_directory) {
  STARTUPINFOA startup;
  PROCESS_INFORMATION process;
  char command_line[FS_PATH_MAX + 4];
  const char *extension = strrchr(path, '.');
  DWORD exit_code;
  int written;

  memset(&startup, 0, sizeof(startup));
  memset(&process, 0, sizeof(process));
  startup.cb = sizeof(startup);

  if (extension && (_stricmp(extension, ".bat") == 0 ||
                    _stricmp(extension, ".cmd") == 0))
    written = snprintf(command_line, sizeof(command_line),
                       "cmd.exe /d /c \"\"%s\"\"", path);
  else
    written = snprintf(command_line, sizeof(command_line), "\"%s\"", path);

  if (written < 0 || (size_t)written >= sizeof(command_line)) return 0;
  if (!CreateProcessA(NULL, command_line, NULL, NULL, FALSE, 0, NULL,
                      working_directory, &startup, &process))
    return 0;

  WaitForSingleObject(process.hProcess, INFINITE);
  exit_code = 1;
  GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return exit_code == 0;
}

static int path_is_inside(const char *parent, const char *child) {
  size_t length = strlen(parent);

  if (_stricmp(parent, child) == 0) return 1;
  if (length > 0 && is_separator(parent[length - 1]))
    return _strnicmp(parent, child, length) == 0;
  return _strnicmp(parent, child, length) == 0 &&
         is_separator(child[length]);
}

static int copy_recursive(const char *source, const char *destination) {
  struct _stat64 source_stat;
  struct _stat64 destination_stat;

  if (_stat64(source, &source_stat) != 0) return 0;

  if (is_directory(&source_stat)) {
    char pattern[FS_PATH_MAX];
    WIN32_FIND_DATAA data;
    HANDLE handle;

    if (path_is_inside(source, destination)) return 0;

    if (_stat64(destination, &destination_stat) == 0) {
      if (!is_directory(&destination_stat)) return 0;
    } else if (_mkdir(destination) != 0) {
      return 0;
    }

    if (!fs_join_path(pattern, sizeof(pattern), source, "*")) return 0;
    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) return 0;

    do {
      char source_child[FS_PATH_MAX];
      char destination_child[FS_PATH_MAX];

      if (strcmp(data.cFileName, ".") == 0 ||
          strcmp(data.cFileName, "..") == 0)
        continue;

      if (!fs_join_path(source_child, sizeof(source_child),
                        source, data.cFileName) ||
          !fs_join_path(destination_child, sizeof(destination_child),
                        destination, data.cFileName) ||
          !copy_recursive(source_child, destination_child)) {
        FindClose(handle);
        return 0;
      }
    } while (FindNextFileA(handle, &data));

    FindClose(handle);
    return 1;
  }

  if (_stat64(destination, &destination_stat) == 0 &&
      is_directory(&destination_stat))
    return 0;

  return CopyFileA(source, destination, FALSE) != 0;
}

int fs_copy(const char *source, const char *destination) {
  return copy_recursive(source, destination);
}

int fs_rename(const char *source, const char *destination) {
  return MoveFileA(source, destination) != 0;
}

int fs_make_directory(const char *path) {
  return _mkdir(path) == 0;
}

static int remove_recursive(const char *path) {
  struct _stat64 st;

  if (_stat64(path, &st) != 0) return 0;
  if (is_directory(&st)) {
    char pattern[FS_PATH_MAX];
    WIN32_FIND_DATAA data;
    HANDLE handle;

    if (!fs_join_path(pattern, sizeof(pattern), path, "*")) return 0;
    handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE) return 0;

    do {
      char child[FS_PATH_MAX];

      if (strcmp(data.cFileName, ".") == 0 ||
          strcmp(data.cFileName, "..") == 0)
        continue;

      if (!fs_join_path(child, sizeof(child), path, data.cFileName) ||
          !remove_recursive(child)) {
        FindClose(handle);
        return 0;
      }
    } while (FindNextFileA(handle, &data));

    FindClose(handle);
    return RemoveDirectoryA(path) != 0;
  }

  return DeleteFileA(path) != 0;
}

int fs_remove(const char *path) {
  return remove_recursive(path);
}

#endif
