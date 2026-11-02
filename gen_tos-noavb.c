#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "sprdsec_header.h"

#define ERR_EXIT(...)                 \
    do                                \
    {                                 \
        fprintf(stderr, __VA_ARGS__); \
        exit(1);                      \
    } while (0)

static uint8_t *loadfile(const char *fn, size_t *num, size_t extra)
{
    size_t n, j = 0;
    uint8_t *buf = 0;
    FILE *fi = fopen(fn, "rb");
    if (fi)
    {
        fseek(fi, 0, SEEK_END);
        n = ftell(fi);
        size_t total = n + extra;
        if (n && total >= n)
        {
            fseek(fi, 0, SEEK_SET);
            buf = (uint8_t *)malloc(total);
            if (buf)
                j = fread(buf, 1, n, fi);
        }
        fclose(fi);
    }
    if (num)
        *num = j;
    return buf;
}

#define max_size(x, y) ((x) > (y) ? (x) : (y))

// For any DHTB data block, compute its true footprint including internal SIMGHDR footer + certs
static size_t dhtb_data_size(const uint8_t *base, uint32_t mImgSize, size_t max_rel)
{
    size_t raw_end = 0x200 + mImgSize;
    if (raw_end + sizeof(sprdsignedimageheader) > max_rel)
        return raw_end;
    const sprdsignedimageheader *pf = (const sprdsignedimageheader *)(base + raw_end);
    // Parse SIMGHDR-style footer fields if present (magic is advisory, not enforced)
    size_t max_end = raw_end + sizeof(sprdsignedimageheader);
    #define check_seg(s, o) do { \
        uint64_t ss = (s), so = (o); \
        if (ss && so && so + ss > max_end && so + ss <= max_rel) \
            max_end = (size_t)(so + ss); \
    } while (0)
    check_seg(pf->cert_size, pf->cert_offset);
    check_seg(pf->priv_size, pf->priv_offset);
    check_seg(pf->cert_dbg_developer_size, pf->cert_dbg_developer_offset);
    #undef check_seg
    return max_end;
}

/* The TOS payload starts right after the 0x200-byte DHTB header, so the
 * in-image virtual address of a byte is (file offset - DHTB_HDR_SIZE). */
#define DHTB_HDR_SIZE 0x200u

static size_t find_bytes(const uint8_t *mem, size_t size, const char *pat)
{
    size_t plen = strlen(pat);
    if (size < plen)
        return (size_t)-1;
    for (size_t i = 0; i + plen <= size; i++)
        if (!memcmp(&mem[i], pat, plen))
            return i;
    return (size_t)-1;
}

static int is_func_prologue(uint32_t insn)
{
    if ((insn & 0xFFC07FFFu) == 0xA9807BFDu) /* stp x29, x30, [sp, #-imm]! */
        return 1;
    if ((insn & 0xFFC003FFu) == 0xD10003FFu) /* sub sp, sp, #imm */
        return 1;
    return 0;
}

/* Find the instruction that loads the address of the marker string.
 * libavb code uses adrp/add pairs for string literals; the adrp page is
 * computed from the in-image virtual address. */
static size_t find_marker_ref(const uint8_t *mem, size_t size, size_t marker_off)
{
    uint64_t target = (uint64_t)marker_off - DHTB_HDR_SIZE;
    uint64_t page[32] = {0};
    int valid[32] = {0};

    for (size_t i = DHTB_HDR_SIZE; i + 4 <= size; i += 4)
    {
        uint32_t insn = *(const uint32_t *)&mem[i];
        if ((insn & 0x9F000000u) == 0x90000000u) /* ADRP */
        {
            int rd = insn & 31;
            int64_t imm = ((int64_t)((insn >> 5) & 0x7FFFF) << 2) | ((insn >> 29) & 3);
            if (imm & (1 << 20))
                imm -= 1 << 21;
            page[rd] = (((uint64_t)i - DHTB_HDR_SIZE) & ~0xFFFULL) + ((uint64_t)imm << 12);
            valid[rd] = 1;
        }
        else if ((insn & 0xFF800000u) == 0x91000000u) /* ADD (immediate, 64-bit) */
        {
            int rn = (insn >> 5) & 31;
            uint64_t imm12 = (insn >> 10) & 0xFFF;
            if (insn & (1u << 22))
                imm12 <<= 12;
            if (valid[rn] && page[rn] + imm12 == target)
                return i;
        }
    }
    return (size_t)-1;
}

/* Walk back from the xref to the enclosing function entry. */
static size_t find_func_start(const uint8_t *mem, size_t ref)
{
    size_t lo = DHTB_HDR_SIZE;
    for (size_t i = ref; i >= DHTB_HDR_SIZE + 4; i -= 4)
    {
        if (*(const uint32_t *)&mem[i - 4] == 0xD65F03C0u) /* ret */
        {
            lo = i;
            break;
        }
    }
    for (size_t i = lo; i < ref; i += 4)
        if (is_func_prologue(*(const uint32_t *)&mem[i]))
            return i;
    return lo;
}

/* Next function entry after the current one (a prologue following a ret). */
static size_t find_func_end(const uint8_t *mem, size_t size, size_t start)
{
    int seen_ret = 0;
    for (size_t i = start + 4; i + 4 <= size; i += 4)
    {
        uint32_t insn = *(const uint32_t *)&mem[i];
        if (insn == 0xD65F03C0u)
            seen_ret = 1;
        else if (seen_ret && is_func_prologue(insn))
            return i;
    }
    return size;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        ERR_EXIT("Usage: %s <filename>\n", argv[0]);

    char *filename = argv[1];
    uint8_t *mem;
    size_t size = 0;
    mem = loadfile(filename, &size, 0);
    if (!mem)
        ERR_EXIT("loadfile(\"%s\") failed\n", filename);
    if ((uint64_t)size >> 32)
        ERR_EXIT("file too big\n");

    if (*(uint32_t *)mem != 0x42544844)
        ERR_EXIT("The file is not sprd trusted firmware\n");
    else if (!(*(uint32_t *)&mem[0x30]))
        ERR_EXIT("broken sprd trusted firmware\n");
    size = dhtb_data_size(mem, *(uint32_t *)&mem[0x30], size);
    printf("0x%zx\n", size);

    FILE *file = fopen("temp", "wb");
    if (file == NULL)
        ERR_EXIT("Failed to create the file.\n");
    size_t bytes_written = fwrite(mem, sizeof(unsigned char), size, file);
    if (bytes_written != size)
        ERR_EXIT("Failed to write the file.\n");
    fclose(file);

    if (remove(filename))
        ERR_EXIT("Failed to delete the file.\n");
    if (rename("temp", filename))
        ERR_EXIT("Failed to rename the file.\n");

    size_t start_pos = 0;
    size_t patch_pos = 0;

    /* Locate the "enter avb_slot_verify" wrapper by its log string, then
     * restrict the mov/epilogue scan to the function that references it. */
    size_t marker = find_bytes(mem, size, "### enter avb_slot_verify. ###");
    if (marker == (size_t)-1)
        marker = find_bytes(mem, size, "enter avb_slot_verify");
    if (marker == (size_t)-1)
    {
        printf("avb marker not found, skip saving!!!\n");
        free(mem);
        return 0;
    }
    size_t ref = find_marker_ref(mem, size, marker);
    if (ref == (size_t)-1)
    {
        printf("avb marker at 0x%zx has no xref (not an AArch64 TOS?), skip saving!!!\n", marker);
        free(mem);
        return 0;
    }
    size_t scan_lo = find_func_start(mem, ref);
    size_t scan_hi = find_func_end(mem, size, scan_lo);
    printf("avb marker at 0x%zx, xref at 0x%zx, function 0x%zx-0x%zx\n",
           marker, ref, scan_lo, scan_hi);

    for (size_t i = scan_lo; i + 32 <= scan_hi; i += 4)
    {
        uint32_t current = *(uint32_t *)&mem[i];
        if (current == 0xD65F03C0 || (current & 0xFF00FFFF) == 0xA8007BFD)
        {
            start_pos = 0;
        }
        else if ((current & 0xFF00FFFF) == 0xA9007BFD)
        {
            start_pos = i;
        }
        else if (start_pos)
        {
            int count1 = 0, count2 = 0, ldp_count = 0, ret_flag = 0;
            for (int m = 0; m < 20; m += 4)
            {
                if (*(uint8_t *)&mem[i + m + 3] == 0xA9)
                    ldp_count++;
                else
                    break;
            }
            if (ldp_count == 5)
            {
                for (int m = 24; m < 32; m += 4)
                {
                    if (*(uint32_t *)&mem[i + m] == 0xD65F03C0)
                        ret_flag++;
                }
                if (ret_flag)
                {
                    if (*(uint16_t *)&mem[i - 4] == 0x3E0 && *(uint8_t *)&mem[i - 7] == 0x3)
                    {
                        for (int m = start_pos; m < i; m += 4)
                        {
                            if (*(uint32_t *)&mem[m] >> 16 == 0x9400)
                            {
                                count1++;
                            }
                            else if (*(uint32_t *)&mem[m] >> 16 == 0xb400)
                            {
                                count2++;
                            }
                        }
                        if (count1 && count2 && count1 + count2 > 2)
                        {
                            printf("detected mov at 0x%zx\n", i - 4);
                            patch_pos = i - 4;
                        }
                    }
                }
            }
        }
    }
    if (!patch_pos)
    {
        printf("dis_avb: skip saving!!!\n");
        free(mem);
        return 0;
    }
    printf("patch mov at 0x%zx\n", patch_pos);
    *(uint32_t *)&mem[patch_pos] = 0x52800000;
    file = fopen("tos-noavb.bin", "wb");
    if (file == NULL)
        ERR_EXIT("Failed to create the file.\n");
    bytes_written = fwrite(mem, sizeof(unsigned char), size, file);
    if (bytes_written != size)
        ERR_EXIT("Failed to write the file.\n");
    fclose(file);

    free(mem);

    return 0;
}
