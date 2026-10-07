#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#include "archive.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CHUNK_SIZE       65536
#define ARCHIVE_HDR_SIZE 12                     /* magic(4) version(4) count(4)     */
#define ENTRY_HDR_SIZE   (AR_MAX_NAME + 8 + 4)  /* name(256) size(8) crc32(4) = 268 */

typedef struct {
    char     name[AR_MAX_NAME];
    uint64_t size;
    uint32_t crc;
} Entry;

typedef enum { MODE_LIST, MODE_TEST, MODE_EXTRACT } Mode;

/* ---------- CRC-32 (IEEE 802.3, same as zip/gzip) ---------- */

static uint32_t crc_table[256];
static int crc_ready = 0;

static void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
    crc_ready = 1;
}

static uint32_t crc_update(uint32_t crc, const unsigned char *buf, size_t len)
{
    if (!crc_ready) crc_init();
    uint32_t c = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/* ---------- Little-endian (de)serialisation ----------
 * Fields are written byte by byte, so the format does not depend on
 * struct padding or on the endianness of the machine. */

static void put_u32(unsigned char *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) p[i] = (unsigned char)(v >> (8 * i));
}

static void put_u64(unsigned char *p, uint64_t v)
{
    for (int i = 0; i < 8; i++) p[i] = (unsigned char)(v >> (8 * i));
}

static uint32_t get_u32(const unsigned char *p)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= (uint32_t)p[i] << (8 * i);
    return v;
}

static uint64_t get_u64(const unsigned char *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

static int write_archive_header(FILE *f, uint32_t count)
{
    unsigned char buf[ARCHIVE_HDR_SIZE];
    memcpy(buf, AR_MAGIC, 4);
    put_u32(buf + 4, AR_VERSION);
    put_u32(buf + 8, count);
    return fwrite(buf, 1, sizeof buf, f) == sizeof buf ? 0 : -1;
}

static int read_archive_header(FILE *f, uint32_t *count)
{
    unsigned char buf[ARCHIVE_HDR_SIZE];
    if (fread(buf, 1, sizeof buf, f) != sizeof buf || memcmp(buf, AR_MAGIC, 4) != 0) {
        fprintf(stderr, "error: not a valid archive (bad magic)\n");
        return -1;
    }
    uint32_t version = get_u32(buf + 4);
    if (version != AR_VERSION) {
        fprintf(stderr, "error: unsupported format version %" PRIu32 " (expected %d)\n",
                version, AR_VERSION);
        return -1;
    }
    *count = get_u32(buf + 8);
    return 0;
}

static int write_entry(FILE *f, const Entry *e)
{
    unsigned char buf[ENTRY_HDR_SIZE];
    memset(buf, 0, sizeof buf);
    memcpy(buf, e->name, strlen(e->name));
    put_u64(buf + AR_MAX_NAME, e->size);
    put_u32(buf + AR_MAX_NAME + 8, e->crc);
    return fwrite(buf, 1, sizeof buf, f) == sizeof buf ? 0 : -1;
}

static int read_entry(FILE *f, Entry *e)
{
    unsigned char buf[ENTRY_HDR_SIZE];
    if (fread(buf, 1, sizeof buf, f) != sizeof buf) return -1;
    memcpy(e->name, buf, AR_MAX_NAME);
    e->name[AR_MAX_NAME - 1] = '\0';          /* never trust the file */
    e->size = get_u64(buf + AR_MAX_NAME);
    e->crc  = get_u32(buf + AR_MAX_NAME + 8);
    return 0;
}

/* ---------- Helpers ---------- */

/* Copy exactly n bytes from src to dst. dst may be NULL (read only).
 * If crc is not NULL it is updated with the bytes that were read. */
static int copy_bytes(FILE *src, FILE *dst, uint64_t n, uint32_t *crc)
{
    unsigned char buf[CHUNK_SIZE];
    while (n > 0) {
        size_t want = n < CHUNK_SIZE ? (size_t)n : CHUNK_SIZE;
        size_t got = fread(buf, 1, want, src);
        if (got == 0) return -1;              /* shorter than expected */
        if (crc) *crc = crc_update(*crc, buf, got);
        if (dst && fwrite(buf, 1, got, dst) != got) return -1;
        n -= got;
    }
    return 0;
}

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

/* Names must be plain file names: no directories, no "." / "..".
 * This blocks "zip-slip" style attacks from malicious archives. */
static int valid_name(const char *n)
{
    return n[0] != '\0' && strcmp(n, ".") != 0 && strcmp(n, "..") != 0
        && strchr(n, '/') == NULL && strchr(n, '\\') == NULL;
}

/* ---------- Pack ---------- */

int ar_pack(const char *archive, int count, char **files)
{
    FILE *in = NULL, *out = NULL;

    if (count <= 0) {
        fprintf(stderr, "error: no input files\n");
        return -1;
    }

    /* Validate everything first so a bad argument never leaves a half-written archive. */
    for (int i = 0; i < count; i++) {
        const char *name = base_name(files[i]);
        struct stat st;

        if (strlen(name) >= AR_MAX_NAME || !valid_name(name)) {
            fprintf(stderr, "error: invalid or too long file name: '%s'\n", files[i]);
            return -1;
        }
        if (stat(files[i], &st) != 0) {
            fprintf(stderr, "error: %s: %s\n", files[i], strerror(errno));
            return -1;
        }
        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "error: %s is not a regular file\n", files[i]);
            return -1;
        }
        if (strcmp(files[i], archive) == 0) {
            fprintf(stderr, "error: the archive cannot contain itself\n");
            return -1;
        }
        for (int j = 0; j < i; j++) {
            if (strcmp(base_name(files[j]), name) == 0) {
                fprintf(stderr, "error: duplicate file name '%s'\n", name);
                return -1;
            }
        }
    }

    out = fopen(archive, "wb");
    if (!out) {
        fprintf(stderr, "error: %s: %s\n", archive, strerror(errno));
        return -1;
    }
    if (write_archive_header(out, (uint32_t)count) != 0) goto io_fail;

    for (int i = 0; i < count; i++) {
        struct stat st;
        Entry e;
        uint32_t crc = 0;

        in = fopen(files[i], "rb");
        if (!in || stat(files[i], &st) != 0) {
            fprintf(stderr, "error: %s: %s\n", files[i], strerror(errno));
            goto fail;
        }

        memset(&e, 0, sizeof e);
        strcpy(e.name, base_name(files[i]));
        e.size = (uint64_t)st.st_size;

        /* Pass 1: checksum (the header comes before the data). */
        if (copy_bytes(in, NULL, e.size, &crc) != 0) goto io_fail;
        e.crc = crc;
        rewind(in);

        /* Pass 2: header + data. */
        if (write_entry(out, &e) != 0 || copy_bytes(in, out, e.size, NULL) != 0)
            goto io_fail;

        fclose(in);
        in = NULL;
        printf("added: %s (%" PRIu64 " bytes)\n", e.name, e.size);
    }

    if (fclose(out) != 0) {
        out = NULL;
        goto io_fail;
    }
    return 0;

io_fail:
    fprintf(stderr, "error: I/O error while writing '%s'\n", archive);
fail:
    if (in) fclose(in);
    if (out) fclose(out);
    remove(archive);
    return -1;
}

/* ---------- List / test / extract ---------- */

static int walk(const char *archive, Mode mode, const char *dest)
{
    FILE *in = fopen(archive, "rb");
    struct stat st;
    uint32_t count = 0;
    uint64_t total, sum = 0;
    int failed = 0;

    if (!in) {
        fprintf(stderr, "error: %s: %s\n", archive, strerror(errno));
        return -1;
    }
    if (fstat(fileno(in), &st) != 0) {
        fprintf(stderr, "error: %s: %s\n", archive, strerror(errno));
        goto fatal;
    }
    total = (uint64_t)st.st_size;

    if (read_archive_header(in, &count) != 0) goto fatal;

    if (mode == MODE_EXTRACT && dest && mkdir(dest, 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "error: cannot create '%s': %s\n", dest, strerror(errno));
        goto fatal;
    }

    if (mode == MODE_LIST)
        printf("%12s  %-8s  %s\n", "SIZE", "CRC32", "NAME");

    for (uint32_t i = 0; i < count; i++) {
        Entry e;
        uint64_t pos;
        uint32_t crc = 0;

        if (read_entry(in, &e) != 0) {
            fprintf(stderr, "error: archive truncated (entry %" PRIu32 " of %" PRIu32 ")\n",
                    i + 1, count);
            goto fatal;
        }
        if (!valid_name(e.name)) {
            fprintf(stderr, "error: refusing unsafe file name in archive: '%s'\n", e.name);
            goto fatal;
        }
        pos = (uint64_t)ftello(in);
        if (e.size > total - pos) {
            fprintf(stderr, "error: archive truncated while reading '%s'\n", e.name);
            goto fatal;
        }
        sum += e.size;

        if (mode == MODE_LIST) {
            printf("%12" PRIu64 "  %08" PRIx32 "  %s\n", e.size, e.crc, e.name);
            fseeko(in, (off_t)e.size, SEEK_CUR);

        } else if (mode == MODE_TEST) {
            if (copy_bytes(in, NULL, e.size, &crc) != 0) goto fatal;
            if (crc != e.crc) {
                printf("FAILED  %s (checksum mismatch)\n", e.name);
                failed = 1;
            } else {
                printf("OK      %s\n", e.name);
            }

        } else { /* MODE_EXTRACT */
            char path[4096];
            FILE *out;
            int n = dest ? snprintf(path, sizeof path, "%s/%s", dest, e.name)
                         : snprintf(path, sizeof path, "%s", e.name);
            if (n < 0 || (size_t)n >= sizeof path) {
                fprintf(stderr, "error: path too long: %s\n", e.name);
                goto fatal;
            }
            out = fopen(path, "wbx");         /* 'x' = fail if it already exists */
            if (!out) {
                if (errno == EEXIST) {
                    printf("skipped: %s (already exists)\n", path);
                    fseeko(in, (off_t)e.size, SEEK_CUR);
                    failed = 1;
                    continue;
                }
                fprintf(stderr, "error: %s: %s\n", path, strerror(errno));
                goto fatal;
            }
            if (copy_bytes(in, out, e.size, &crc) != 0 || fclose(out) != 0) {
                fprintf(stderr, "error: failed while extracting '%s'\n", path);
                remove(path);
                goto fatal;
            }
            if (crc != e.crc) {
                fprintf(stderr, "error: checksum mismatch, removed '%s'\n", path);
                remove(path);
                failed = 1;
            } else {
                printf("extracted: %s (%" PRIu64 " bytes)\n", path, e.size);
            }
        }
    }

    if (mode == MODE_LIST)
        printf("%" PRIu32 " file(s), %" PRIu64 " bytes total\n", count, sum);

    fclose(in);
    return failed ? -1 : 0;

fatal:
    fclose(in);
    return -1;
}

int ar_list(const char *archive)                      { return walk(archive, MODE_LIST, NULL); }
int ar_test(const char *archive)                      { return walk(archive, MODE_TEST, NULL); }
int ar_extract(const char *archive, const char *dest) { return walk(archive, MODE_EXTRACT, dest); }
