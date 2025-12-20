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
        if (n)
        {
            fseek(fi, 0, SEEK_SET);
            buf = (uint8_t *)malloc(n + extra);
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
        if (postrom_header->mImgSize && (header->mPostromOffset + 0x200 + postrom_header->mImgSize <= size))
        {
            sizewithPostrom = header->mPostromOffset + 0x200 + postrom_header->mImgSize;
        }
    }
    if (!header->mImgSize)
        ERR_EXIT("broken sprd trusted firmware\n");
    sprdsignedimageheader *footer = (sprdsignedimageheader *)&mem[header->mImgSize + 0x200];
    if (header->mImgSize + 0x200 + sizeof(sprdsignedimageheader) >= size)
    {
        printf("0x%zx\n", size);
        free(mem);
        return 0;
    }
    if (footer->cert_dbg_developer_size && footer->cert_dbg_developer_offset)
        size = max_size(size, footer->cert_dbg_developer_size + footer->cert_dbg_developer_offset);
    if (footer->priv_size && footer->priv_offset)
        size = max_size(size, footer->priv_size + footer->priv_offset);
    if (footer->cert_size && footer->cert_offset)
        size = max_size(size, footer->cert_size + footer->cert_offset);
    if (footer->payload_size && footer->payload_offset)
        size = max_size(size, footer->payload_size + footer->payload_offset);
    else
        size = max_size(size, header->mImgSize + 0x200);
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
