#include <stdio.h>
#include <string.h>

#include "archive.h"

#define PROGRAM_VERSION "1.0.0"

static void usage(FILE *f, const char *prog)
{
    fprintf(f,
        "myar %s - pack many files into one binary archive\n\n"
        "Usage:\n"
        "  %s pack    <archive> <file>...   create an archive\n"
        "  %s list    <archive>             show the contents\n"
        "  %s test    <archive>             verify checksums\n"
        "  %s extract <archive> [dir]       extract (never overwrites)\n"
        "  %s --help | --version\n",
        PROGRAM_VERSION, prog, prog, prog, prog, prog);
}

int main(int argc, char **argv)
{
    const char *prog = argv[0];
    int rc;

    if (argc < 2) {
        usage(stderr, prog);
        return 1;
    }
    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        usage(stdout, prog);
        return 0;
    }
    if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
        printf("myar %s (archive format v%d)\n", PROGRAM_VERSION, AR_VERSION);
        return 0;
    }

    if (!strcmp(argv[1], "pack") && argc >= 4)
        rc = ar_pack(argv[2], argc - 3, argv + 3);
    else if (!strcmp(argv[1], "list") && argc == 3)
        rc = ar_list(argv[2]);
    else if (!strcmp(argv[1], "test") && argc == 3)
        rc = ar_test(argv[2]);
    else if (!strcmp(argv[1], "extract") && (argc == 3 || argc == 4))
        rc = ar_extract(argv[2], argc == 4 ? argv[3] : NULL);
    else {
        usage(stderr, prog);
        return 1;
    }
    return rc == 0 ? 0 : 1;
}
