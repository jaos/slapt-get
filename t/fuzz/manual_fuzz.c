#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "fuzz_target.h"

typedef struct {
    uint64_t state;
} fuzz_rng_t;

typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t len;
} fbuf_t;

static const char *const names[] = { "foo", "test-package", "cpan2tgz", "my-lib", "a", "x-y-z" };
static const char *const versions[] = {
    "1.0-i486-1",
    "1.0.0-x86_64-1",
    "0.6.8-noarch-1",
    "99999999999999999999-x86-1",
    "2147483648-x86-2147483648",
    "1.0-i486-1rob",
};

static const char *const exts[] = { ".txz", ".tgz", ".tz", ".taz", ".tx" };
static const char *const locations[] = {
    "./foo",
    "./a/b/c",
    "./testing/foo",
    "./extra/x",
    "./pasture/y",
    "../evil",
    "./a..b",
};


static uint64_t rng_next(fuzz_rng_t *rng)
{
    uint64_t z = (rng->state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void fuzz_rng_init(fuzz_rng_t *rng, uint64_t seed)
{
    rng->state = seed;
}

static uint32_t rng_range(fuzz_rng_t *rng, uint32_t n)
{
    return (uint32_t)(rng_next(rng) % n);
}

static bool rng_chance(fuzz_rng_t *rng, uint32_t pct)
{
    return rng_range(rng, 100) < pct;
}

static void fbuf_put(fbuf_t *b, const void *p, size_t n)
{
    if (b->len + n > b->cap)
        n = b->cap - b->len;
    if (n == 0)
        return;
    memcpy(b->buf + b->len, p, n);
    b->len += n;
}

static void fbuf_str(fbuf_t *b, const char *s)
{
    fbuf_put(b, s, strlen(s));
}

static void fbuf_line(fbuf_t *b, const char *s)
{
    fbuf_str(b, s);
    fbuf_str(b, "\n");
}

static void emit_name(fuzz_rng_t *rng, fbuf_t *b)
{
    const char *name = names[rng_range(rng, sizeof names / sizeof names[0])];
    const char *ver = versions[rng_range(rng, sizeof versions / sizeof versions[0])];
    const char *ext = exts[rng_range(rng, sizeof exts / sizeof exts[0])];
    char line[512];

    switch (rng_range(rng, 20)) {
    case 10: /* missing colon -> outer loop skips the line */
        snprintf(line, sizeof line, "PACKAGE NAME %s-%s%s", name, ver, ext);
        fbuf_line(b, line);
        break;
    case 11: /* tab after colon: the [ ]{1,} in the pattern rejects it */
        snprintf(line, sizeof line, "PACKAGE NAME:\t%s-%s%s", name, ver, ext);
        fbuf_line(b, line);
        break;
    case 12: /* lowercase marker: REG_ICASE, so this still matches */
        snprintf(line, sizeof line, "package name:  %s-%s%s", name, ver, ext);
        fbuf_line(b, line);
        break;
    case 13: /* double space in the marker: no match, line skipped */
        snprintf(line, sizeof line, "PACKAGE  NAME:  %s-%s%s", name, ver, ext);
        fbuf_line(b, line);
        break;
    case 14: /* version without the two dashes the regex requires */
        snprintf(line, sizeof line, "PACKAGE NAME:  %s%s", name, ext);
        fbuf_line(b, line);
        break;
    case 15: /* NUL byte inside the name */
        fbuf_str(b, "PACKAGE NAME:  \x00");
        snprintf(line, sizeof line, "%s-%s%s", name, ver, ext);
        fbuf_line(b, line);
        break;
    case 16: /* trailing spaces (the pattern tolerates [ ]{0,}$) */
        snprintf(line, sizeof line, "PACKAGE NAME:  %s-%s%s   ", name, ver, ext);
        fbuf_line(b, line);
        break;
    case 17: /* single space after the colon */
        snprintf(line, sizeof line, "PACKAGE NAME: %s-%s%s", name, ver, ext);
        fbuf_line(b, line);
        break;
    case 18: /* marker line with no usable content after it */
        fbuf_line(b, "PACKAGE NAME:  ");
        break;
    case 19: /* no NAME line at all: the record's lines are dead weight */
        fbuf_line(b, "random junk line");
        break;
    default: /* 0-9: valid */
        snprintf(line, sizeof line, "PACKAGE NAME:  %s-%s%s", name, ver, ext);
        fbuf_line(b, line);
        break;
    }
}

static void emit_mirror(fuzz_rng_t *rng, fbuf_t *b)
{
    if (!rng_chance(rng, 30))
        return;

    switch (rng_range(rng, 10)) {
    case 0: /* valid, trailing slash */
        fbuf_line(b, "PACKAGE MIRROR:  http://example.org/repo/");
        break;
    case 1: /* valid, no trailing slash: exercises the fixup realloc */
        fbuf_line(b, "PACKAGE MIRROR:  http://example.org/repo");
        break;
    case 2: /* empty value */
        fbuf_line(b, "PACKAGE MIRROR: ");
        break;
    case 3: /* NUL byte in the value */
        fbuf_str(b, "PACKAGE MIRROR:  http://\x00ex.org/\n");
        break;
    case 4: /* space inside the value */
        fbuf_line(b, "PACKAGE MIRROR:  http://ex.org/a b/");
        break;
    case 5: /* missing colon: treated as "not provided" (rewind) */
        fbuf_line(b, "PACKAGE MIRROR http://ex.org/");
        break;
    default: /* long value (250 bytes) */
        fbuf_str(b, "PACKAGE MIRROR:  http://ex.org/");
        for (uint32_t i = 0; i < 250; ++i)
            fbuf_put(b, "a", 1);
        fbuf_str(b, "/\n");
        break;
    }
}

static void emit_priority(fuzz_rng_t *rng, fbuf_t *b)
{
    if (!rng_chance(rng, 30))
        return;

    char line[64];

    switch (rng_range(rng, 10)) {
    case 0: /* valid range 0-7 */
        snprintf(line, sizeof line, "PACKAGE PRIORITY:  %u", rng_range(rng, 8));
        fbuf_line(b, line);
        break;
    case 1: /* 8: above SLAPT_PRIORITY_CUSTOM_PATCH -> package dropped */
        fbuf_line(b, "PACKAGE PRIORITY:  8");
        break;
    case 2: /* huge: strtoul overflow path */
        fbuf_line(b, "PACKAGE PRIORITY:  99999999999999999999");
        break;
    case 3: /* leading zeros */
        fbuf_line(b, "PACKAGE PRIORITY:  007");
        break;
    case 4: /* empty value: regex misses -> rewind */
        fbuf_line(b, "PACKAGE PRIORITY:  ");
        break;
    case 5: /* missing colon: regex misses -> rewind */
        fbuf_line(b, "PACKAGE PRIORITY 2");
        break;
    default: /* no line: priority stays default */
        break;
    }
}

static void emit_location(fuzz_rng_t *rng, fbuf_t *b)
{
    switch (rng_range(rng, 20)) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14:
        /* valid (incl. the workaround prefixes and the ".." drop paths) */
        fbuf_line(b, "PACKAGE LOCATION:  ");
        fbuf_str(b, locations[rng_range(rng, sizeof locations / sizeof locations[0])]);
        break;
    case 15: /* empty value: location is mandatory -> dropped */
        fbuf_line(b, "PACKAGE LOCATION: ");
        break;
    case 16: /* NUL byte in the value: regexec stops at the NUL, the
              * capture is empty -> dropped */
        fbuf_str(b, "PACKAGE LOCATION:  \x00./foo\n");
        break;
    case 17: /* tab after the colon: [ ]+ rejects it -> dropped */
        fbuf_str(b, "PACKAGE LOCATION:\t./foo\n");
        break;
    case 18: /* missing line: regex fails on the next line -> dropped */
        fbuf_line(b, "some junk instead");
        break;
    default: /* missing line, EOF-ish: nothing emitted */
        break;
    }
}

static void emit_size(fuzz_rng_t *rng, fbuf_t *b, bool compressed)
{
    const char *marker = compressed ? "PACKAGE SIZE (compressed)" : "PACKAGE SIZE (uncompressed)";

    char line[80];

    switch (rng_range(rng, 20)) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16:
        /* valid, 1-99999 K */
        snprintf(line, sizeof line, "%s:  %u K", marker, 1 + rng_range(rng, 99999));
        fbuf_line(b, line);
        break;
    case 17: /* zero */
        fbuf_line(b, compressed ? "PACKAGE SIZE (compressed):  0 K" : "PACKAGE SIZE (uncompressed):  0 K");
        break;
    case 18: /* huge: strtoul wraps to UINT32_MAX */
        fbuf_line(b, compressed ? "PACKAGE SIZE (compressed):  99999999999999999999 K"
                                : "PACKAGE SIZE (uncompressed):  99999999999999999999 K");
        break;
    case 19: /* missing line -> dropped with "regexec failed to parse size_*" */
    default:
        break;
    }
}

static void emit_required(fuzz_rng_t *rng, fbuf_t *b)
{
    if (!rng_chance(rng, 25))
        return;

    switch (rng_range(rng, 10)) {
    case 0:
        fbuf_line(b, "PACKAGE REQUIRED:  perl");
        break;
    case 1:
        fbuf_line(b, "PACKAGE REQUIRED:  perl, openssl");
        break;
    case 2: /* versioned operators */
        fbuf_line(b, "PACKAGE REQUIRED:  perl >= 5.0, openssl < 1.1");
        break;
    case 3: /* empty list parts */
        fbuf_line(b, "PACKAGE REQUIRED:  ,,,");
        break;
    case 4: /* operator with no version */
        fbuf_line(b, "PACKAGE REQUIRED:  perl ==");
        break;
    case 5: /* dangling operator */
        fbuf_line(b, "PACKAGE REQUIRED:  perl >");
        break;
    case 6: /* empty value */
        fbuf_line(b, "PACKAGE REQUIRED:  ");
        break;
    case 7: /* NUL in the value */
        fbuf_str(b, "PACKAGE REQUIRED:  pe\x00rl\n");
        break;
    default: /* bare marker, no colon: consumed, not rewound */
        fbuf_line(b, "PACKAGE REQUIRED");
        break;
    }
}

static void emit_conflicts_suggests(fuzz_rng_t *rng, fbuf_t *b, const char *marker)
{
    if (!rng_chance(rng, 15))
        return;

    char line[96];

    switch (rng_range(rng, 10)) {
    case 0: case 1: case 2:
        snprintf(line, sizeof line, "%s:  foo", marker);
        fbuf_line(b, line);
        break;
    case 3: case 4:
        snprintf(line, sizeof line, "%s:  a, b", marker);
        fbuf_line(b, line);
        break;
    case 5: case 6: /* empty value */
        snprintf(line, sizeof line, "%s:  ", marker);
        fbuf_line(b, line);
        break;
    case 7: /* NUL in the value */
        snprintf(line, sizeof line, "%s:  a\x00b", marker);
        fbuf_line(b, line);
        break;
    case 8: /* bare marker, no colon: consumed, not rewound */
        snprintf(line, sizeof line, "%s", marker);
        line[strcspn(line, ":")] = '\0';
        fbuf_line(b, line);
        break;
    default: /* missing line */
        break;
    }
}

static void emit_md5(fuzz_rng_t *rng, fbuf_t *b)
{
    if (!rng_chance(rng, 40))
        return;

    static const char hexd[] = "0123456789abcdef";
    char line[80];
    int n;

    switch (rng_range(rng, 10)) {
    case 0: case 1: case 2: /* valid 32-hex */
        for (n = 0; n < 32; ++n)
            line[n] = hexd[rng_range(rng, 16)];
        line[32] = '\0';
        fbuf_line(b, "PACKAGE MD5SUM:  ");
        fbuf_str(b, line);
        break;
    case 3: /* 31 hex: accepted, strlcpy handles it */
        for (n = 0; n < 31; ++n)
            line[n] = hexd[rng_range(rng, 16)];
        line[31] = '\0';
        fbuf_line(b, "PACKAGE MD5SUM:  ");
        fbuf_str(b, line);
        break;
    case 4: /* 33 hex: "md5 sum too long" -> dropped */
        for (n = 0; n < 33; ++n)
            line[n] = hexd[rng_range(rng, 16)];
        line[33] = '\0';
        fbuf_line(b, "PACKAGE MD5SUM:  ");
        fbuf_str(b, line);
        break;
    case 5: /* non-hex content */
        snprintf(line, sizeof line, "%.*s", 32, "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
        fbuf_line(b, "PACKAGE MD5SUM:  ");
        fbuf_str(b, line);
        break;
    case 6: /* bare marker without a colon */
        fbuf_line(b, "PACKAGE MD5");
        break;
    case 7: /* extra spaces after the colon, short value */
        fbuf_line(b, "PACKAGE MD5:   abc");
        break;
    case 8: /* "PACKAGE MD5SUM" vs "PACKAGE MD5" both pass the strstr gate */
        fbuf_line(b, "PACKAGE MD5SUM ");
        break;
    default: /* no md5 line */
        break;
    }
}

static void emit_description(fuzz_rng_t *rng, fbuf_t *b, bool last_record)
{
    switch (rng_range(rng, 20)) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13:
        /* 1-4 lines, then the terminating blank line */
        fbuf_line(b, "PACKAGE DESCRIPTION:");
        for (uint32_t i = 0, n = 1 + rng_range(rng, 4); i < n; ++i)
            fbuf_line(b, "pkg: some description text");
        fbuf_line(b, "");
        break;
    case 14: /* empty description */
        fbuf_line(b, "PACKAGE DESCRIPTION:");
        fbuf_line(b, "");
        break;
    case 15: /* NUL inside a description line */
        fbuf_line(b, "PACKAGE DESCRIPTION:");
        fbuf_str(b, "pkg: de\x00scription\n");
        fbuf_line(b, "");
        break;
    case 16: { /* long description line (1-8 KiB) */
        const uint32_t n = 1024 + rng_range(rng, 7 * 1024);
        fbuf_line(b, "PACKAGE DESCRIPTION:");
        fbuf_str(b, "pkg: ");
        for (uint32_t j = 0; j < n; ++j)
            fbuf_put(b, "x", 1);
        fbuf_line(b, "");
        break;
    }
    case 17: { /* no terminating blank line: for a non-last record the next
                * record's NAME line is swallowed into the description; for
                * the last record the description loop runs into EOF */
        fbuf_line(b, "PACKAGE DESCRIPTION:");
        fbuf_line(b, "pkg: unterminated");
        (void)last_record;
        break;
    }
    case 18: /* description text that looks like a marker */
        fbuf_line(b, "PACKAGE DESCRIPTION:");
        fbuf_line(b, "pkg: see PACKAGE NAME: for details");
        fbuf_line(b, "");
        break;
    default: /* missing marker -> "error attempting to read pkg description" */
        fbuf_line(b, "");
        break;
    }
}

static void gen_record(fuzz_rng_t *rng, fbuf_t *b, bool last_record)
{
    emit_name(rng, b);
    emit_mirror(rng, b);
    emit_priority(rng, b);
    emit_location(rng, b);
    emit_size(rng, b, true);
    emit_size(rng, b, false);
    emit_required(rng, b);
    emit_conflicts_suggests(rng, b, "PACKAGE CONFLICTS:");
    emit_conflicts_suggests(rng, b, "PACKAGE SUGGESTS:");
    emit_md5(rng, b);
    emit_description(rng, b, last_record);
}

/* convert all '\n' to '\r\n' in place */
static void fbuf_crlf(fbuf_t *b)
{
    if (b->len >= b->cap)
        return; /* no room to grow */

    for (size_t i = b->len; i-- > 0;) {
        if (b->buf[i] == '\n') {
            memmove(b->buf + i + 1, b->buf + i, b->len - i);
            b->buf[i] = '\r';
            b->len++;
        }
    }
}

static size_t fuzz_gen_packages_txt(fuzz_rng_t *rng, uint8_t *buf, size_t cap)
{
    fbuf_t b = { buf, cap, 0 };
    const uint32_t n_records = 1 + rng_range(rng, 3); /* 1-3 */

    for (uint32_t i = 0; i < n_records; ++i) {
        gen_record(rng, &b, i + 1 == n_records);
        if (i + 1 < n_records && rng_chance(rng, 80))
            fbuf_str(&b, "\n"); /* blank-line separator; absent otherwise */
    }

    /* file-level post-processes (independent, several may apply) */
    if (rng_chance(rng, 5)) {
        /* a single huge line after the records */
        fbuf_str(&b, "\n");
        for (uint32_t i = 0; i < 16384; ++i)
            fbuf_put(&b, "x", 1);
        fbuf_str(&b, "\n");
    }
    if (rng_chance(rng, 10)) {
        /* junk before the first record, one of which trips the outer
         * loop's "PACKAGE NAME" strstr gate and the regex-fail path */
        fbuf_line(&b, "JUNK LINE BEFORE RECORD");
        fbuf_line(&b, "PACKAGE NAME without a colon");
    }
    if (rng_chance(rng, 20))
        fbuf_crlf(&b);
    if (rng_chance(rng, 20) && b.len > 0 && b.buf[b.len - 1] == '\n')
        b.len--; /* no final newline */
    if (rng_chance(rng, 10)) {
        /* binary garbage tail */
        for (uint32_t i = 0, n = 1 + rng_range(rng, 64); i < n; ++i) {
            uint8_t byte = (uint8_t)(rng_next(rng) & 0xff);
            fbuf_put(&b, &byte, 1);
        }
    }
    if (rng_chance(rng, 15))
        b.len = rng_range(rng, (uint32_t)b.len + 1); /* truncate anywhere */

    return b.len;
}

#define FUZZ_BUF_CAP (64 * 1024)

static void run_corpus(const char *dir_name)
{
    DIR *dir = opendir(dir_name);
    if (dir == NULL) {
        fprintf(stderr, "fuzz_runner: cannot open corpus dir %s\n", dir_name);
        exit(EXIT_FAILURE);
    }

    size_t count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        char path[4096];
        snprintf(path, sizeof path, "%s/%s", dir_name, entry->d_name);

        FILE *fh = fopen(path, "rb");
        if (fh == NULL) {
            fprintf(stderr, "fuzz_runner: cannot open corpus file %s\n", path);
            exit(EXIT_FAILURE);
        }
        struct stat st;
        if (fstat(fileno(fh), &st) != 0) {
            fprintf(stderr, "fuzz_runner: cannot stat corpus file %s\n", path);
            fclose(fh);
            exit(EXIT_FAILURE);
        }
        uint8_t *buf = malloc((size_t)st.st_size + 1);
        if (buf == NULL) {
            fprintf(stderr, "fuzz_runner: out of memory for %s\n", path);
            fclose(fh);
            exit(EXIT_FAILURE);
        }
        size_t len = fread(buf, 1, (size_t)st.st_size, fh);
        fclose(fh);

        fprintf(stderr, "fuzz_runner: corpus %s (%zu bytes)\n", entry->d_name, len);
        slapt_fuzz_packages_txt(buf, len);
        free(buf);
        ++count;
    }
    closedir(dir);

    fprintf(stderr, "fuzz_runner: corpus phase done, %zu files\n", count);
}

static size_t gen_input_at(uint64_t seed, uint64_t index, uint8_t *buf, size_t cap)
{
    fuzz_rng_t rng;
    fuzz_rng_init(&rng, seed);
    for (uint64_t i = 0; i < index; ++i)
        fuzz_gen_packages_txt(&rng, buf, cap);
    return fuzz_gen_packages_txt(&rng, buf, cap);
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--single") == 0) {
        if (argc != 4) {
            fprintf(stderr, "usage: %s --single <seed> <index>\n", argv[0]);
            return EXIT_FAILURE;
        }
        uint64_t seed = strtoull(argv[2], NULL, 10);
        uint64_t index = strtoull(argv[3], NULL, 10);

        static uint8_t buf[FUZZ_BUF_CAP];
        size_t len = gen_input_at(seed, index, buf, sizeof buf);
        fprintf(stderr, "fuzz_runner: running single input seed=%llu index=%llu (%zu bytes)\n",
                (unsigned long long)seed, (unsigned long long)index, len);
        slapt_fuzz_packages_txt(buf, len);
        return EXIT_SUCCESS;
    }

    if (argc >= 2 && strcmp(argv[1], "--dump") == 0) {
        if (argc != 5) {
            fprintf(stderr, "usage: %s --dump <seed> <index> <outfile>\n", argv[0]);
            return EXIT_FAILURE;
        }
        uint64_t seed = strtoull(argv[2], NULL, 10);
        uint64_t index = strtoull(argv[3], NULL, 10);

        static uint8_t buf[FUZZ_BUF_CAP];
        size_t len = gen_input_at(seed, index, buf, sizeof buf);

        FILE *out = fopen(argv[4], "wb");
        if (out == NULL) {
            fprintf(stderr, "fuzz_runner: cannot open %s for writing\n", argv[4]);
            return EXIT_FAILURE;
        }
        fwrite(buf, 1, len, out);
        fclose(out);
        return EXIT_SUCCESS;
    }

    if (argc != 4) {
        fprintf(stderr,
                "usage: %s <corpus_dir> <seed> <count>\n"
                "       %s --single <seed> <index>\n"
                "       %s --dump <seed> <index> <outfile>\n",
                argv[0], argv[0], argv[0]);
        return EXIT_FAILURE;
    }
    const char *corpus_dir = argv[1];
    uint64_t seed = strtoull(argv[2], NULL, 10);
    unsigned long count = strtoul(argv[3], NULL, 10);

    run_corpus(corpus_dir);

    static uint8_t buf[FUZZ_BUF_CAP];
    fuzz_rng_t rng;
    fuzz_rng_init(&rng, seed);

    struct timespec t0, t_last;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    t_last = t0;

    for (unsigned long i = 0; i < count; ++i) {
        size_t len = fuzz_gen_packages_txt(&rng, buf, sizeof buf);
        slapt_fuzz_packages_txt(buf, len);

        if (i % 10000 == 9999) {
            struct timespec t1;
            clock_gettime(CLOCK_MONOTONIC, &t1);
            fprintf(stderr, "fuzz_runner: %lu/%lu inputs (%.1fs)\n",
                    i + 1, count,
                    (double)(t1.tv_sec - t_last.tv_sec) + (double)(t1.tv_nsec - t_last.tv_nsec) / 1e9);
            t_last = t1;
        }
    }

    struct timespec t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    fprintf(stderr, "fuzz_runner: done, %lu inputs in %.1fs (seed %llu)\n",
            count, (double)(t_end.tv_sec - t0.tv_sec) + (double)(t_end.tv_nsec - t0.tv_nsec) / 1e9,
            (unsigned long long)seed);
    return EXIT_SUCCESS;
}
