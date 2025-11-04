#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
    if (*(uint32_t *)&mem[0x30] + 0x260 >= size)
    {
        printf("0x%zx\n", size);
        free(mem);
        return 0;
    }
    if (*(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x50] && *(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x58])
        size = *(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x50] + *(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x58];
    else if (*(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x30] && *(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x38])
        size = *(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x30] + *(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x38];
    else if (*(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x20] && *(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x28])
        size = *(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x20] + *(uint32_t *)&mem[(*(uint32_t *)&mem[0x30]) + 0x200 + 0x28];
    else
        size = *(uint32_t *)&mem[0x30] + 0x200;
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

    size_t start_pos = 0, end_pos = 0, sp_pos = 0, last_pos = 0;
    int mov_count = 0;
    for (size_t i = 0; i < size - 0x200; i += 4)
    {
        int count1 = 0, count2 = 0;
        uint32_t current = *(uint32_t *)(mem + 0x200 + i);
        current = current & 0xFF00FFFF;
        if (current == 0xA9007BFD)
            start_pos = i;
        else if (current == 0x910003BF)
            sp_pos = i;
        else if (start_pos && current == 0xA8007BFD)
        {
            end_pos = i;
            if (sp_pos)
            {
                for (int m = start_pos; m < end_pos; m += 4)
                {
                    if (*(uint32_t *)&mem[0x200 + m] >> 16 == 0x9400)
                    {
                        count1++;
                    }
                    else if (*(uint32_t *)&mem[0x200 + m] >> 16 == 0xb400)
                    {
                        count2++;
                    }
                }
                if (count1 && count2 && count1 + count2 > 2)
                {
                    for (int m = sp_pos + 4; m < end_pos; m += 4)
                    {
                        if (*(uint16_t *)&mem[0x200 + m] == 0x3E0)
                        {
                            //*(uint32_t *)&mem[0x200 + m] = 0x52800000;
                            if (*(uint32_t *)&mem[0x200 + m] == 0x52800000)
                            {
                                printf("dis_avb: patched!!!\n");
                                free(mem);
                                return 0;
                            }
                            printf("detected mov at 0x%zx\n", 0x200 + m);
                            last_pos = m;
                            mov_count++;
                        }
                    }
                }
            }
            start_pos = 0;
            sp_pos = 0;
        }
    }
    if (mov_count < 2)
    {
        printf("dis_avb: skip saving!!!\n");
        free(mem);
        return 0;
    }
    *(uint32_t *)&mem[0x200 + last_pos] = 0x52800000;
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
