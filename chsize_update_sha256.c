#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "sprdsec_header.h"
#include "mincrypt/sha256.h"

#define ror(value, bits) (((value) >> (bits)) | ((value) << (32 - (bits))))
#define shr(value, bits) ((value) >> (bits))

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static void SHA256_Transform(SHA256_CTX *ctx)
{
    uint32_t W[64];
    uint32_t A, B, C, D, E, F, G, H;
    uint8_t *p = ctx->buf;
    int t;

    for (t = 0; t < 16; ++t)
    {
        uint32_t tmp = *p++ << 24;
        tmp |= *p++ << 16;
        tmp |= *p++ << 8;
        tmp |= *p++;
        W[t] = tmp;
    }

    for (; t < 64; t++)
    {
        uint32_t s0 = ror(W[t - 15], 7) ^ ror(W[t - 15], 18) ^ shr(W[t - 15], 3);
        uint32_t s1 = ror(W[t - 2], 17) ^ ror(W[t - 2], 19) ^ shr(W[t - 2], 10);
        W[t] = W[t - 16] + s0 + W[t - 7] + s1;
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];
    F = ctx->state[5];
    G = ctx->state[6];
    H = ctx->state[7];

    for (t = 0; t < 64; t++)
    {
        uint32_t s0 = ror(A, 2) ^ ror(A, 13) ^ ror(A, 22);
        uint32_t maj = (A & B) ^ (A & C) ^ (B & C);
        uint32_t t2 = s0 + maj;
        uint32_t s1 = ror(E, 6) ^ ror(E, 11) ^ ror(E, 25);
        uint32_t ch = (E & F) ^ ((~E) & G);
        uint32_t t1 = H + s1 + ch + K[t] + W[t];

        H = G;
        G = F;
        F = E;
        E = D + t1;
        D = C;
        C = B;
        B = A;
        A = t1 + t2;
    }

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
    ctx->state[4] += E;
    ctx->state[5] += F;
    ctx->state[6] += G;
    ctx->state[7] += H;
}

static const HASH_VTAB SHA256_VTAB = {
    SHA256_init,
    SHA256_update,
    SHA256_final,
    SHA256_hash,
    SHA256_DIGEST_SIZE};

void SHA256_init(SHA256_CTX *ctx)
{
    ctx->f = &SHA256_VTAB;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

void SHA256_update(SHA256_CTX *ctx, const void *data, int len)
{
    int i = (int)(ctx->count & 63);
    const uint8_t *p = (const uint8_t *)data;

    ctx->count += len;

    while (len--)
    {
        ctx->buf[i++] = *p++;
        if (i == 64)
        {
            SHA256_Transform(ctx);
            i = 0;
        }
    }
}

const uint8_t *SHA256_final(SHA256_CTX *ctx)
{
    uint8_t *p = ctx->buf;
    uint64_t cnt = ctx->count * 8;
    int i;

    SHA256_update(ctx, (uint8_t *)"\x80", 1);
    while ((ctx->count & 63) != 56)
    {
        SHA256_update(ctx, (uint8_t *)"\0", 1);
    }
    for (i = 0; i < 8; ++i)
    {
        uint8_t tmp = (uint8_t)(cnt >> ((7 - i) * 8));
        SHA256_update(ctx, &tmp, 1);
    }

    for (i = 0; i < 8; i++)
    {
        uint32_t tmp = ctx->state[i];
        *p++ = tmp >> 24;
        *p++ = tmp >> 16;
        *p++ = tmp >> 8;
        *p++ = tmp >> 0;
    }

    return ctx->buf;
}

const uint8_t *SHA256_hash(const void *data, int len, uint8_t *digest)
{
    SHA256_CTX ctx;
    SHA256_init(&ctx);
    SHA256_update(&ctx, data, len);
    memcpy(digest, SHA256_final(&ctx), SHA256_DIGEST_SIZE);
    return digest;
}

void do_sha256(uint8_t *data, int bytes_num, unsigned char *hash)
{
    SHA256_CTX ctx;
    const uint8_t *sha;

    SHA256_init(&ctx);
    SHA256_update(&ctx, data, bytes_num);
    sha = SHA256_final(&ctx);

    memcpy(hash, sha, SHA256_DIGEST_SIZE);
}

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
        do_sha256((uint8_t *)postrom_header + 0x200, postrom_header->mImgSize, postrom_header->mPayloadHash);
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
    do_sha256(mem + 0x200, header->mImgSize, header->mPayloadHash);

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
