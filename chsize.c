#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
    size_t sizewithPostrom = 0;
    sys_img_header *header = (sys_img_header *)mem;
    if (header->mPostromOffset && header->mPostromOffset + 0x200 < size)
    {
        postrom_main_header *postrom_header = (postrom_main_header *)(mem + header->mPostromOffset);
        uint32_t pmagic = *(uint32_t *)postrom_header;
        if ((pmagic == 0x42544844 || pmagic == 0x50534844) &&
            postrom_header->mImgSize && (header->mPostromOffset + 0x200 + postrom_header->mImgSize <= size))
        {
            sizewithPostrom = header->mPostromOffset + dhtb_data_size(
                (uint8_t *)postrom_header, postrom_header->mImgSize, size - header->mPostromOffset);
        }
    }
    if (!header->mImgSize)
        ERR_EXIT("broken sprd trusted firmware\n");
    size = dhtb_data_size(mem, header->mImgSize, size);
    size = max_size(size, sizewithPostrom);
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
    free(mem);

    return 0;
}
