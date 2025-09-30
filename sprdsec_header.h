typedef struct
{
    uint32_t mMagicNum;
    uint32_t mVersion;
    uint8_t mPayloadHash[32]; // sha256
    uint8_t reserved1[8];
    uint32_t mImgSize;
    uint8_t reserved2[24];
    uint32_t mFirmwareSize; // value = raw_ImgSize, mImgSize = (raw_ImgSize + 0xF) & 0xFFFFFFF0
    uint32_t mUnknownSize1; // value = header + mImgSize + footer + key_cert - 0x20 // for key_cert
                            // value = header + mImgSize + footer + content_cert // for content_cert
    uint32_t mPostromFullSize;
    uint32_t mPostromOffset;
    uint8_t nickname[36];
    uint8_t reserved[384];
} sys_img_header;

typedef struct
{
    uint32_t mMagicNum;
    uint32_t mVersion;
    uint8_t mPayloadHash[32]; // sha256
    uint8_t reserved1[8];
    uint32_t mImgSize;
    uint8_t reserved2[24];
    uint32_t mFirmwareSize; // value = 0 in postrom
    uint32_t mUnknownSize1; // value = header + mImgSize
    uint8_t reserved[428];
} postrom_main_header;

typedef struct
{
    uint32_t mUnknownNum;
    uint32_t mOffset; // &postrom_main_header + mOffset = PostromPayload_addr
    uint32_t mPostromPayloadSize;
    uint8_t ROTPK_HASH[32]; // sha256, this is exactly the cpu fused value
} postrom_child_header;

typedef struct
{
    /* Magic number */
    uint8_t magic[8];
    /* Version of this header format */
    uint32_t header_version_major;
    /* Version of this header format */
    uint32_t header_version_minor;

    /*image body, plain or cipher text */
    uint64_t payload_size;
    uint64_t payload_offset;

    /*offset from itself start */
    /*content certification size,if 0,ignore */
    uint64_t cert_size;
    uint64_t cert_offset;

    /*(opt)private content size,if 0,ignore */
    uint64_t priv_size;
    uint64_t priv_offset;

    /*(opt)debug/rma certification primary size,if 0,ignore */
    uint64_t cert_dbg_prim_size;
    uint64_t cert_dbg_prim_offset;

    /*(opt)debug/rma certification second size,if 0,ignore */
    uint64_t cert_dbg_developer_size;
    uint64_t cert_dbg_developer_offset;

} sprdsignedimageheader;