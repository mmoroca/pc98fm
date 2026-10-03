#include "archive_zip.h"
#include "fs.h"

#include "third_party/miniz.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARCHIVE_PATH_MAX FS_PATH_MAX

static int archive_name_join(char *buffer, size_t size,
                             const char *directory, const char *name) {
  int written;

  if (directory[0] == '\0') {
    written = snprintf(buffer, size, "%s", name);
  } else {
    written = snprintf(buffer, size, "%s/%s", directory, name);
  }

  return written >= 0 && (size_t)written < size;
}

static int add_directory_contents(mz_zip_archive *archive,
                                  const char *source,
                                  const char *archive_directory);

static int add_source_entry(mz_zip_archive *archive,
                            const char *source,
                            const char *archive_name) {
  if (!fs_path_is_directory(source))
    return mz_zip_writer_add_file(archive, archive_name, source, NULL, 0,
                                  MZ_DEFAULT_COMPRESSION) != 0;

  if (archive_name[0] != '\0') {
    char directory_name[ARCHIVE_PATH_MAX];
    int written = snprintf(directory_name, sizeof(directory_name), "%s/",
                           archive_name);
    if (written < 0 || (size_t)written >= sizeof(directory_name) ||
        !mz_zip_writer_add_mem(archive, directory_name, NULL, 0,
                               MZ_DEFAULT_COMPRESSION))
      return 0;
  }

  return add_directory_contents(archive, source, archive_name);
}

static int add_directory_contents(mz_zip_archive *archive,
                                  const char *source,
                                  const char *archive_directory) {
  FsDirectory *directory = malloc(sizeof(*directory));
  int success = 1;

  if (!directory || !fs_load_directory(source, directory)) {
    free(directory);
    return 0;
  }

  for (size_t i = 0; i < directory->count; i++) {
    char child_source[ARCHIVE_PATH_MAX];
    char child_name[ARCHIVE_PATH_MAX];
    const FsEntry *entry = &directory->entries[i];

    if (strcmp(entry->name, ".") == 0 ||
        strcmp(entry->name, "..") == 0)
      continue;

    if (!fs_join_path(child_source, sizeof(child_source), source,
                      entry->name) ||
        !archive_name_join(child_name, sizeof(child_name),
                           archive_directory, entry->name) ||
        !add_source_entry(archive, child_source, child_name)) {
      success = 0;
      break;
    }
  }

  free(directory);
  return success;
}

int archive_pack(const char *source, const char *destination) {
  mz_zip_archive archive;
  char source_name[FS_NAME_MAX];
  int success = 0;

  if (!fs_path_basename(source, source_name, sizeof(source_name))) return 0;
  memset(&archive, 0, sizeof(archive));

  if (!mz_zip_writer_init_file(&archive, destination, 0)) return 0;

  /* Directories keep their name; a single file remains one top-level entry. */
  success = add_source_entry(&archive, source, source_name);

  if (success) success = mz_zip_writer_finalize_archive(&archive) != 0;
  if (!mz_zip_writer_end(&archive)) success = 0;

  if (!success) fs_remove(destination);
  return success;
}

static int safe_archive_name(const char *name) {
  const char *cursor;
  const char *component;

  if (name[0] == '\0' || name[0] == '/' || name[0] == '\\') return 0;
  if (isalpha((unsigned char)name[0]) && name[1] == ':') return 0;

  component = name;
  for (cursor = name; ; cursor++) {
    if (*cursor == '\\') return 0;
    if (*cursor == '/' || *cursor == '\0') {
      size_t length = (size_t)(cursor - component);
      if (length == 0) {
        /* A single trailing slash is the directory marker. */
        if (*cursor == '\0' && cursor > name && cursor[-1] == '/') break;
        return 0;
      }
      if ((length == 1 && component[0] == '.') ||
          (length == 2 && component[0] == '.' && component[1] == '.'))
        return 0;
      if (*cursor == '\0') break;
      component = cursor + 1;
    }
  }

  return strlen(name) < ARCHIVE_PATH_MAX;
}

static int append_component(char *path, size_t size, const char *component) {
  char next[ARCHIVE_PATH_MAX];

  if (!fs_join_path(next, sizeof(next), path, component)) return 0;
  if (strlen(next) >= size) return 0;
  strcpy(path, next);
  return 1;
}

static int ensure_archive_directory(const char *root, const char *relative,
                                    char *result, size_t result_size) {
  const char *component = relative;
  char path[ARCHIVE_PATH_MAX];

  if (strlen(root) >= sizeof(path) || strlen(root) >= result_size) return 0;
  strcpy(path, root);

  while (*component) {
    const char *end = strchr(component, '/');
    size_t length = end ? (size_t)(end - component) : strlen(component);
    char name[FS_NAME_MAX];

    if (length == 0 || length >= sizeof(name)) return 0;
    memcpy(name, component, length);
    name[length] = '\0';

    if (!append_component(path, sizeof(path), name)) return 0;
    if (fs_path_exists(path)) {
      if (!fs_path_is_directory(path)) return 0;
    } else if (!fs_make_directory(path)) {
      return 0;
    }

    if (!end) break;
    component = end + 1;
  }

  if (strlen(path) >= result_size) return 0;
  strcpy(result, path);
  return 1;
}

static int archive_output_path(const char *root, const char *relative,
                               char *result, size_t result_size) {
  const char *component = relative;
  char path[ARCHIVE_PATH_MAX];

  if (strlen(root) >= sizeof(path)) return 0;
  strcpy(path, root);

  while (*component) {
    const char *end = strchr(component, '/');
    size_t length = end ? (size_t)(end - component) : strlen(component);
    char name[FS_NAME_MAX];

    if (length == 0 || length >= sizeof(name)) return 0;
    memcpy(name, component, length);
    name[length] = '\0';
    if (!append_component(path, sizeof(path), name)) return 0;

    if (!end) break;
    component = end + 1;
  }

  if (strlen(path) >= result_size) return 0;
  strcpy(result, path);
  return 1;
}

int archive_find_conflict(const char *archive_path, const char *destination,
                          char *relative_name, size_t size) {
  mz_zip_archive archive;
  char filename[ARCHIVE_PATH_MAX];
  int result = 0;

  if (!relative_name || size == 0) return -1;
  relative_name[0] = '\0';
  memset(&archive, 0, sizeof(archive));
  if (!mz_zip_reader_init_file(&archive, archive_path, 0)) return -1;

  for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&archive); i++) {
    char top_level[FS_NAME_MAX];
    char output[ARCHIVE_PATH_MAX];
    const char *separator;
    size_t length;
    mz_uint filename_length;

    filename_length = mz_zip_reader_get_filename(&archive, i, filename,
                                                  sizeof(filename));
    if (filename_length == 0 || filename_length >= sizeof(filename) ||
        !safe_archive_name(filename)) {
      result = -1;
      break;
    }

    separator = strchr(filename, '/');
    length = separator ? (size_t)(separator - filename) : strlen(filename);
    if (length == 0 || length >= sizeof(top_level) ||
        length >= size) {
      result = -1;
      break;
    }

    memcpy(top_level, filename, length);
    top_level[length] = '\0';
    if (!fs_join_path(output, sizeof(output), destination, top_level)) {
      result = -1;
      break;
    }

    if (fs_path_exists(output)) {
      strcpy(relative_name, top_level);
      result = 1;
      break;
    }
  }

  mz_zip_reader_end(&archive);
  return result;
}

int archive_unpack(const char *archive_path, const char *destination) {
  mz_zip_archive archive;
  char filename[ARCHIVE_PATH_MAX];
  int success = 1;
  int created_destination = 0;

  if (fs_path_exists(destination)) {
    if (!fs_path_is_directory(destination)) return 0;
  } else if (!fs_make_directory(destination)) {
    return 0;
  } else {
    created_destination = 1;
  }

  memset(&archive, 0, sizeof(archive));
  if (!mz_zip_reader_init_file(&archive, archive_path, 0)) return 0;

  for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&archive); i++) {
    mz_uint length;
    mz_zip_archive_file_stat stat;
    char output[ARCHIVE_PATH_MAX];
    const char *last_separator;

    length = mz_zip_reader_get_filename(&archive, i, filename,
                                        sizeof(filename));
    if (length == 0 || length >= sizeof(filename) ||
        !safe_archive_name(filename) ||
        !mz_zip_reader_file_stat(&archive, i, &stat) ||
        !mz_zip_reader_is_file_supported(&archive, i) ||
        mz_zip_reader_is_file_encrypted(&archive, i)) {
      success = 0;
      break;
    }

    if (mz_zip_reader_is_file_a_directory(&archive, i)) {
      if (!ensure_archive_directory(destination, filename,
                                     output, sizeof(output))) {
        success = 0;
        break;
      }
      continue;
    }

    last_separator = strrchr(filename, '/');
    if (last_separator) {
      char directory[ARCHIVE_PATH_MAX];
      size_t directory_length = (size_t)(last_separator - filename);

      if (directory_length >= sizeof(directory)) {
        success = 0;
        break;
      }
      memcpy(directory, filename, directory_length);
      directory[directory_length] = '\0';
      if (!ensure_archive_directory(destination, directory,
                                    output, sizeof(output))) {
        success = 0;
        break;
      }
    }

    if (!archive_output_path(destination, filename, output, sizeof(output)) ||
        !mz_zip_reader_extract_to_file(&archive, i, output, 0)) {
      success = 0;
      break;
    }
  }

  mz_zip_reader_end(&archive);
  if (!success && created_destination) fs_remove(destination);
  return success;
}
