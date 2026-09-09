#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned off;              /* offset relative to function start */
    unsigned char b[4];
    int wildcard;              /* 1 = PC-relative instruction, skip compare */
} tpl_entry_t;

typedef struct {
    const char     *name;      /* human-readable variant name       */
    unsigned        func_len;  /* template total length in bytes    */
    unsigned        patch_off; /* offset of block to NOP            */
    unsigned        patch_len; /* number of bytes to NOP            */
    const tpl_entry_t *ent;    /* fixed-byte entries                */
    unsigned        ent_n;
} patch_tpl_t;

/* ---------------- v1 template (16-byte patch @ +0x20) ---------------- */
static const tpl_entry_t ENT_V1[] = {
    { 0x00, { 0xFD,0x7B,0xBE,0xA9 }, 0 },  /* stp  x29,x30,[sp,#-0x20]! */
    { 0x04, { 0xFD,0x03,0x00,0x91 }, 0 },  /* mov  x29,sp               */
    { 0x08, { 0xF3,0x53,0x01,0xA9 }, 0 },  /* stp  x19,x20,[sp,#0x10]   */
    { 0x0C, { 0,0,0,0 },              1 },  /* adrp x19,<page>   (PC-rel)*/
    { 0x10, { 0xF4,0x03,0x00,0xAA }, 0 },  /* mov  x20,x0               */
    { 0x14, { 0,0,0,0 },              1 },  /* add  x19,x19,<low> (PC-rel)*/
    { 0x18, { 0x60,0x02,0x40,0xF9 }, 0 },  /* ldr  x0,[x19]             */
    { 0x1C, { 0xE1,0x03,0x14,0xAA }, 0 },  /* mov  x1,x20               */
    { 0x20, { 0,0,0,0 },              1 },  /* bl   <check>      (PC-rel)*/
    { 0x24, { 0x80,0x00,0x00,0x34 }, 0 },  /* cbz  w0,+0x10             */
    { 0x28, { 0x60,0x8E,0x40,0xF8 }, 0 },  /* ldr  x0,[x19,#imm]        */
    { 0x2C, { 0x60,0xFF,0xFF,0xB5 }, 0 },  /* cbnz x0,-0x14             */
    { 0x30, { 0x20,0x00,0x80,0x52 }, 0 },  /* mov  w0,#1                */
    { 0x34, { 0xF3,0x53,0x41,0xA9 }, 0 },  /* ldp  x19,x20,[sp,#0x10]   */
    { 0x38, { 0xFD,0x7B,0xC2,0xA8 }, 0 },  /* ldp  x29,x30,[sp],#0x20   */
    { 0x3C, { 0xC0,0x03,0x5F,0xD6 }, 0 },  /* ret                      */
};
#define ENT_V1_N (sizeof(ENT_V1)/sizeof(ENT_V1[0]))

/* ---------------- v2 template (24-byte patch @ +0x34) ---------------- */
static const tpl_entry_t ENT_V2[] = {
    { 0x00, { 0xFD,0x7B,0xBE,0xA9 }, 0 },  /* stp  x29,x30,[sp,#-0x20]! */
    { 0x04, { 0xFD,0x03,0x00,0x91 }, 0 },  /* mov  x29,sp               */
    { 0x08, { 0xF3,0x53,0x01,0xA9 }, 0 },  /* stp  x19,x20,[sp,#0x10]   */
    { 0x0C, { 0,0,0,0 },              1 },  /* adrp x19,<page>   (PC-rel)*/
    { 0x10, { 0xF4,0x03,0x00,0xAA }, 0 },  /* mov  x20,x0               */
    { 0x14, { 0,0,0,0 },              1 },  /* add  x19,x19,<low> (PC-rel)*/
    { 0x18, { 0,0,0,0 },              1 },  /* adrp x0,<g2>      (PC-rel)*/
    { 0x1C, { 0,0,0,0 },              1 },  /* add  x0,x0,<g2>    (PC-rel)*/
    { 0x20, { 0,0,0,0 },              1 },  /* add  x0,x0,#imm    (addr)  */
    { 0x24, { 0x04,0x00,0x00,0x14 }, 0 },  /* b    +0x10 (skip retry)    */
    { 0x28, { 0x62,0x86,0x40,0xF8 }, 0 },  /* ldr  x2,[x19,#0x30]        */
    { 0x2C, { 0xE0,0x03,0x02,0xAA }, 0 },  /* mov  x0,x2                 */
    { 0x30, { 0xE2,0x00,0x00,0xB4 }, 0 },  /* cbz  x2, ok                */
    { 0x34, { 0xE1,0x03,0x14,0xAA }, 0 },  /* mov  x1,x20   <- patch start*/
    { 0x38, { 0,0,0,0 },              1 },  /* bl   <check>      (PC-rel)*/
    { 0x3C, { 0x60,0xFF,0xFF,0x35 }, 0 },  /* cbnz w0, retry             */
    { 0x40, { 0xF3,0x53,0x41,0xA9 }, 0 },  /* ldp  x19,x20,[sp,#0x10]    */
    { 0x44, { 0xFD,0x7B,0xC2,0xA8 }, 0 },  /* ldp  x29,x30,[sp],#0x20    */
    { 0x48, { 0xC0,0x03,0x5F,0xD6 }, 0 },  /* ret  (failure epilogue)    */
    { 0x4C, { 0x20,0x00,0x80,0x52 }, 0 },  /* mov  w0,#1   (success)     */
    { 0x50, { 0xF3,0x53,0x41,0xA9 }, 0 },  /* ldp  x19,x20,[sp,#0x10]    */
    { 0x54, { 0xFD,0x7B,0xC2,0xA8 }, 0 },  /* ldp  x29,x30,[sp],#0x20    */
    { 0x58, { 0xC0,0x03,0x5F,0xD6 }, 0 },  /* ret                       */
};
#define ENT_V2_N (sizeof(ENT_V2)/sizeof(ENT_V2[0]))

static const patch_tpl_t TPLS[] = {
    { "v1 (16B @ +0x20)", 0x40, 0x20, 16, ENT_V1, ENT_V1_N },
    { "v2 (20B @ +0x38)", 0x5C, 0x38, 20, ENT_V2, ENT_V2_N },
};
#define TPLS_N (sizeof(TPLS)/sizeof(TPLS[0]))

static const unsigned char NOP4[4] = { 0x1F,0x20,0x03,0xD5 };

static int match_at(const unsigned char *buf, size_t pos, const patch_tpl_t *t)
{
    unsigned k;
    for (k = 0; k < t->ent_n; k++) {
        const tpl_entry_t *e = &t->ent[k];
        if (e->wildcard)
            continue;
        if (memcmp(buf + pos + e->off, e->b, 4) != 0)
            return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    const char *path;
    int dry = 0;
    FILE *fp;
    long fsize;
    unsigned char *buf;
    size_t t, i;
    const patch_tpl_t *found = NULL;
    size_t found_pos = 0;
    unsigned tpl_seen = 0;   /* templates with >=1 hit            */
    unsigned tpl_multi = 0;  /* templates with >1 hit             */

    if (argc < 2 || argc > 3) {
        fprintf(stderr,
            "usage: %s <image.bin> [--dry-run]\n"
            "  default  : scan, report, and write <name>_patched<ext>\n"
            "  --dry-run: scan + report only, write nothing\n",
            argv[0]);
        return 2;
    }
    path = argv[1];
    if (argc == 3) {
        if (strcmp(argv[2], "--dry-run") == 0)
            dry = 1;
        else {
            fprintf(stderr, "error: unknown option '%s'\n", argv[2]);
            return 2;
        }
    }

    fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "error: cannot open %s\n", path); return 1; }
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize <= 0) { fclose(fp); fprintf(stderr, "error: empty file\n"); return 1; }
    buf = (unsigned char *)malloc((size_t)fsize);
    if (!buf) { fclose(fp); fprintf(stderr, "error: out of memory\n"); return 1; }
    if (fread(buf, 1, (size_t)fsize, fp) != (size_t)fsize) {
        fprintf(stderr, "error: short read on %s\n", path);
        free(buf); fclose(fp); return 1;
    }
    fclose(fp);

    /* scan every template; require exactly one template with exactly one hit */
    for (t = 0; t < TPLS_N; t++) {
        const patch_tpl_t *tp = &TPLS[t];
        size_t hits = 0, pos = 0;
        if ((size_t)fsize >= tp->func_len) {
            for (i = 0; i + tp->func_len <= (size_t)fsize; i++) {
                if (match_at(buf, i, tp)) {
                    hits++;
                    pos = i;
                }
            }
        }
        if (hits >= 1) {
            tpl_seen++;
            if (hits > 1) {
                tpl_multi++;
                fprintf(stderr, "[warn] template %s: %zu matches (not unique)\n",
                        tp->name, hits);
            } else {
                found = tp;
                found_pos = pos;
            }
        }
    }

    if (!found) {
        fprintf(stderr,
            "[result] no unique template match in %s\n"
            "         (already patched, different build, or template not in table).\n",
            path);
        free(buf);
        return 1;
    }
    if (tpl_seen > 1 || tpl_multi > 0) {
        fprintf(stderr,
            "[result] ambiguous: %u template(s) matched in %s; refusing to patch.\n",
            tpl_seen, path);
        free(buf);
        return 1;
    }

    printf("[result] %s (%ld bytes)\n", path, fsize);
    printf("[result] template    = %s\n", found->name);
    printf("[result] func_offset = 0x%zX\n", found_pos);
    printf("[result] patch_point = 0x%zX   (func+0x%X)\n",
           found_pos + found->patch_off, found->patch_off);
    printf("[result] patch_len   = %u bytes\n", found->patch_len);
    printf("[result] orig bytes  = ");
    for (i = 0; i < found->patch_len; i++)
        printf("%02X ", buf[found_pos + found->patch_off + i]);
    printf("\n[result] patch bytes = ");
    for (i = 0; i < found->patch_len; i++)
        printf("%02X ", NOP4[i & 3]);
    printf("(NOPs -> force mov w0,#1 success path)\n");

    if (dry) {
        printf("[result] DRY-RUN: nothing written.\n");
        free(buf);
        return 0;
    }

    /* default mode: write patched copy "<name>_patched<ext>", input untouched */
    {
        char out[4096];
        size_t last_sep = 0, last_dot = (size_t)-1, i;
        FILE *fout;

        /* find filename start (last path separator) */
        for (i = 0; path[i]; i++) {
            if (path[i] == '\\' || path[i] == '/')
                last_sep = i + 1;
        }
        /* find last '.' inside the filename */
        for (i = last_sep; path[i]; i++) {
            if (path[i] == '.')
                last_dot = i;
        }
        if (last_dot != (size_t)-1) {
            /* "dir/name.ext" -> "dir/name_patched.ext" */
            snprintf(out, sizeof(out), "%.*s_patched%s",
                     (int)last_dot, path, path + last_dot);
        } else {
            /* no extension: "dir/name" -> "dir/name_patched" */
            snprintf(out, sizeof(out), "%s_patched", path);
        }

        for (i = 0; i < found->patch_len; i++)
            buf[found_pos + found->patch_off + i] = NOP4[i & 3];

        fout = fopen(out, "wb");
        if (!fout) {
            fprintf(stderr, "error: cannot create %s\n", out);
            free(buf); return 1;
        }
        if (fwrite(buf, 1, (size_t)fsize, fout) != (size_t)fsize) {
            fprintf(stderr, "error: write failed on %s\n", out);
            fclose(fout); free(buf); return 1;
        }
        fclose(fout);

        printf("[result] PATCHED: wrote %s (original untouched)\n", out);
    }

    free(buf);
    return 0;
}
