#ifndef PC98FM_ARCHIVE_ZIP_H
#define PC98FM_ARCHIVE_ZIP_H

#include <stddef.h>

/* ZIP archive operations used by the file manager. */
int archive_pack(const char *source, const char *destination);
int archive_find_conflict(const char *archive, const char *destination,
                          char *relative_name, size_t size);
int archive_unpack(const char *archive, const char *destination);

#endif
