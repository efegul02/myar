#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stdint.h>

#define AR_MAGIC     "MYAR"
#define AR_VERSION   2
#define AR_MAX_NAME  256   /* includes the terminating '\0' */

/* All functions return 0 on success and -1 on failure.
 * Error messages are printed to stderr. */

/* Pack `count` files into a new archive. Only the base name of each
 * input path is stored. Removes the archive again if anything fails. */
int ar_pack(const char *archive, int count, char **files);

/* Print the table of contents. */
int ar_list(const char *archive);

/* Read every file and verify its CRC-32 without writing anything. */
int ar_test(const char *archive);

/* Extract all files into dest_dir (created if missing; NULL = current dir).
 * Existing files are never overwritten. */
int ar_extract(const char *archive, const char *dest_dir);

#endif /* ARCHIVE_H */
