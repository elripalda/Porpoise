/* Porpoise - reading files out of GameCube and Wii disc images (see
 * porpoise_disc.hpp). Read-only, small, and slow enough not to matter: it
 * reads a few hundred kilobytes once per game, in the background.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_disc.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb/stb_image.h"
#pragma clang diagnostic pop

extern "C" size_t porpoise_zstd_decompress(void *dst, size_t dst_len, const void *src, size_t src_len);

namespace porpoise::disc
{
namespace
{
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

u16 be16(const u8 *p) { return u16(p[0] << 8 | p[1]); }
u32 be32(const u8 *p) { return u32(p[0]) << 24 | u32(p[1]) << 16 | u32(p[2]) << 8 | p[3]; }
u64 be64(const u8 *p) { return u64(be32(p)) << 32 | be32(p + 4); }
u32 le32(const u8 *p) { return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24; }
u64 le64(const u8 *p) { return u64(le32(p)) | u64(le32(p + 4)) << 32; }

constexpr u64 kWiiSector = 0x8000, kWiiSectorData = 0x7C00, kWiiSectorHashes = 0x400;
constexpr u64 kWiiGroupData = kWiiSectorData * 64; /* the data of 64 sectors: one hash group */

/* ---- AES-128 (decryption) --------------------------------------------------------------- */

const u8 kSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9,
    0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f,
    0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07,
    0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3,
    0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58,
    0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3,
    0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f,
    0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac,
    0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a,
    0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70,
    0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
    0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42,
    0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

u8 xtime(u8 x) { return u8((x << 1) ^ ((x & 0x80) ? 0x1b : 0)); }
u8 mul(u8 a, u8 b)
{
    u8 r = 0;
    while (b)
    {
        if (b & 1)
            r ^= a;
        a = xtime(a);
        b >>= 1;
    }
    return r;
}

struct Aes
{
    u8 round_keys[176];
    u8 inv_sbox[256];
    explicit Aes(const u8 key[16])
    {
        for (int i = 0; i < 256; ++i)
            inv_sbox[kSbox[i]] = u8(i);
        std::memcpy(round_keys, key, 16);
        u8 rcon = 1;
        for (int i = 16; i < 176; i += 4)
        {
            u8 t[4] = {round_keys[i - 4], round_keys[i - 3], round_keys[i - 2], round_keys[i - 1]};
            if (i % 16 == 0)
            {
                const u8 first = t[0];
                t[0] = u8(kSbox[t[1]] ^ rcon);
                t[1] = kSbox[t[2]];
                t[2] = kSbox[t[3]];
                t[3] = kSbox[first];
                rcon = xtime(rcon);
            }
            for (int j = 0; j < 4; ++j)
                round_keys[i + j] = u8(round_keys[i - 16 + j] ^ t[j]);
        }
    }
    void decrypt_block(u8 s[16]) const
    {
        auto add = [&](int round) {
            for (int i = 0; i < 16; ++i)
                s[i] ^= round_keys[round * 16 + i];
        };
        auto inv_shift_sub = [&]() {
            u8 t[16];
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    t[((c + r) % 4) * 4 + r] = inv_sbox[s[c * 4 + r]];
            std::memcpy(s, t, 16);
        };
        auto inv_mix = [&]() {
            for (int c = 0; c < 4; ++c)
            {
                u8 *col = s + c * 4;
                const u8 a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                col[0] = u8(mul(a0, 14) ^ mul(a1, 11) ^ mul(a2, 13) ^ mul(a3, 9));
                col[1] = u8(mul(a0, 9) ^ mul(a1, 14) ^ mul(a2, 11) ^ mul(a3, 13));
                col[2] = u8(mul(a0, 13) ^ mul(a1, 9) ^ mul(a2, 14) ^ mul(a3, 11));
                col[3] = u8(mul(a0, 11) ^ mul(a1, 13) ^ mul(a2, 9) ^ mul(a3, 14));
            }
        };
        add(10);
        for (int round = 9; round >= 1; --round)
        {
            inv_shift_sub();
            add(round);
            inv_mix();
        }
        inv_shift_sub();
        add(0);
    }
};

/* The Wii's common keys (as Dolphin carries them): the title key of each
 * disc's data partition is encrypted with one of them. 0: the usual one, 1:
 * Korean consoles'. */
const u8 kCommonKeys[2][16] = {
    {0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7},
    {0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e, 0x13, 0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e}};

/* ---- the containers ------------------------------------------------------------------------- */

struct File
{
    std::FILE *f = nullptr;
    u64 size = 0;
    ~File()
    {
        if (f)
            std::fclose(f);
    }
    bool open(const std::string &path)
    {
        f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;
        std::fseek(f, 0, SEEK_END);
        size = u64(std::ftell(f));
        return true;
    }
    bool read(u64 offset, void *out, u64 n)
    {
        if (offset + n > size)
            return false;
        return std::fseek(f, long(offset), SEEK_SET) == 0 && std::fread(out, 1, n, f) == n;
    }
};

class Image
{
public:
    virtual ~Image() = default;
    /* Bytes as they are on the disc (a Wii partition's still encrypted). */
    virtual bool read(u64 offset, u64 size, u8 *out) = 0;
    /* A Wii partition's data, decrypted, when the container keeps it so (RVZ). */
    virtual bool read_decrypted(u64 /*partition_data_offset*/, u64 /*offset*/, u64 /*size*/, u8 * /*out*/,
                                const u8 * /*key*/)
    {
        return false;
    }
    virtual bool stores_decrypted() const { return false; }
    /* The title key for a partition whose data starts at this offset, when
     * the container knows it (RVZ). */
    virtual bool partition_key(u64 /*partition_data_offset*/, u8 /*key*/[16]) { return false; }
    File file;
};

class Plain : public Image
{
public:
    bool read(u64 offset, u64 size, u8 *out) override { return file.read(offset, out, size); }
};

class Wbfs : public Image
{
public:
    u64 wbfs_sector = 0;
    std::vector<u16> table;
    bool init()
    {
        u8 h[12];
        if (!file.read(0, h, sizeof h) || std::memcmp(h, "WBFS", 4) != 0)
            return false;
        const u64 hd_sector = u64(1) << h[8];
        wbfs_sector = u64(1) << h[9];
        if (wbfs_sector < kWiiSector)
            return false;
        const u64 blocks = (143432ull * 2 * kWiiSector + wbfs_sector - 1) / wbfs_sector;
        std::vector<u8> raw(blocks * 2);
        if (!file.read(hd_sector + 0x100, raw.data(), raw.size()))
            return false;
        table.resize(blocks);
        for (u64 i = 0; i < blocks; ++i)
            table[i] = be16(&raw[i * 2]);
        return true;
    }
    bool read(u64 offset, u64 size, u8 *out) override
    {
        while (size)
        {
            const u64 cluster = offset / wbfs_sector, within = offset % wbfs_sector;
            if (cluster >= table.size())
                return false;
            const u64 n = std::min(size, wbfs_sector - within);
            if (table[cluster] == 0)
                std::memset(out, 0, n);
            else if (!file.read(u64(table[cluster]) * wbfs_sector + within, out, n))
                return false;
            offset += n;
            size -= n;
            out += n;
        }
        return true;
    }
};

class Ciso : public Image
{
public:
    u64 block = 0;
    std::vector<u64> where; /* file offset of each block, or ~0 when not stored */
    bool init()
    {
        std::vector<u8> h(0x8000);
        if (!file.read(0, h.data(), h.size()) || std::memcmp(h.data(), "CISO", 4) != 0)
            return false;
        block = le32(&h[4]);
        if (block == 0)
            return false;
        u64 next = 0x8000;
        for (std::size_t i = 8; i < h.size(); ++i)
        {
            where.push_back(h[i] ? next : ~u64(0));
            if (h[i])
                next += block;
        }
        return true;
    }
    bool read(u64 offset, u64 size, u8 *out) override
    {
        while (size)
        {
            const u64 b = offset / block, within = offset % block;
            if (b >= where.size())
                return false;
            const u64 n = std::min(size, block - within);
            if (where[b] == ~u64(0))
                std::memset(out, 0, n);
            else if (!file.read(where[b] + within, out, n))
                return false;
            offset += n;
            size -= n;
            out += n;
        }
        return true;
    }
};

class Gcz : public Image
{
public:
    u64 compressed = 0, block = 0, data_offset = 0;
    std::vector<u64> pointers;
    u64 cached = ~u64(0);
    std::vector<u8> cache;
    bool init()
    {
        u8 h[32];
        if (!file.read(0, h, sizeof h) || le32(h) != 0xB10BC001)
            return false;
        compressed = le64(h + 8);
        block = le32(h + 24);
        const u32 blocks = le32(h + 28);
        if (block == 0 || blocks == 0)
            return false;
        std::vector<u8> raw(std::size_t(blocks) * 8);
        if (!file.read(32, raw.data(), raw.size()))
            return false;
        pointers.resize(blocks);
        for (u32 i = 0; i < blocks; ++i)
            pointers[i] = le64(&raw[i * 8]);
        data_offset = 32 + u64(blocks) * 8 + u64(blocks) * 4;
        return true;
    }
    bool load(u64 b)
    {
        if (b == cached)
            return true;
        constexpr u64 kRaw = u64(1) << 63;
        const u64 start = pointers[b] & ~kRaw;
        const u64 end = b + 1 < pointers.size() ? pointers[b + 1] & ~kRaw : compressed;
        if (end < start)
            return false;
        std::vector<u8> in(end - start);
        if (!file.read(data_offset + start, in.data(), in.size()))
            return false;
        cache.assign(block, 0);
        if (pointers[b] & kRaw)
            std::memcpy(cache.data(), in.data(), std::min<u64>(in.size(), block));
        else if (stbi_zlib_decode_buffer(reinterpret_cast<char *>(cache.data()), int(block),
                                         reinterpret_cast<const char *>(in.data()), int(in.size())) < 0)
            return false;
        cached = b;
        return true;
    }
    bool read(u64 offset, u64 size, u8 *out) override
    {
        while (size)
        {
            const u64 b = offset / block, within = offset % block;
            if (b >= pointers.size() || !load(b))
                return false;
            const u64 n = std::min(size, block - within);
            std::memcpy(out, cache.data() + within, n);
            offset += n;
            size -= n;
            out += n;
        }
        return true;
    }
};

/* WIA and RVZ: the disc in groups ("chunks"), each compressed on its own;
 * a Wii partition's data kept decrypted, its hashes left out. */
class Wia : public Image
{
public:
    bool rvz = false;
    u32 compression = 0, chunk = 0;
    u8 disc_header[0x80];
    struct PartData
    {
        u32 first_sector, sectors, group, groups;
    };
    struct Partition
    {
        u8 key[16];
        PartData data[2];
    };
    struct Raw
    {
        u64 offset, size;
        u32 group, groups;
    };
    struct Group
    {
        u64 offset;
        u32 size;
        bool compressed;
        u32 packed;
    };
    std::vector<Partition> partitions;
    std::vector<Raw> raws;
    std::vector<Group> groups;
    u64 cached_offset = ~u64(0);
    std::vector<u8> cached;

    bool decompress(const std::vector<u8> &in, bool compressed, std::vector<u8> &out, u64 max_out)
    {
        if (!compressed || compression == 0)
        {
            out = in;
            return true;
        }
        if (compression != 5)
            return false; /* bzip2 / LZMA: not read here */
        out.resize(max_out);
        const std::size_t n = porpoise_zstd_decompress(out.data(), out.size(), in.data(), in.size());
        if (n == std::size_t(-1))
            return false;
        out.resize(n);
        return true;
    }
    bool init()
    {
        u8 h1[0x48];
        if (!file.read(0, h1, sizeof h1))
            return false;
        rvz = std::memcmp(h1, "RVZ\x01", 4) == 0;
        if (!rvz && std::memcmp(h1, "WIA\x01", 4) != 0)
            return false;
        const u32 h2_size = be32(h1 + 0x0C);
        if (h2_size < 0xD4 || h2_size > 0x1000)
            return false;
        std::vector<u8> h2(h2_size);
        if (!file.read(0x48, h2.data(), h2.size()))
            return false;
        compression = be32(&h2[4]);
        chunk = be32(&h2[0x0C]);
        std::memcpy(disc_header, &h2[0x10], 0x80);
        const u32 n_parts = be32(&h2[0x90]), part_size = be32(&h2[0x94]);
        const u64 parts_off = be64(&h2[0x98]);
        const u32 n_raw = be32(&h2[0xB4]);
        const u64 raw_off = be64(&h2[0xB8]);
        const u32 raw_size = be32(&h2[0xC0]);
        const u32 n_groups = be32(&h2[0xC4]);
        const u64 groups_off = be64(&h2[0xC8]);
        const u32 groups_size = be32(&h2[0xD0]);
        if (chunk == 0 || part_size < 0x30 || n_parts > 64)
            return false;
        std::vector<u8> pe(std::size_t(n_parts) * part_size);
        if (!file.read(parts_off, pe.data(), pe.size()))
            return false;
        for (u32 i = 0; i < n_parts; ++i)
        {
            const u8 *p = &pe[std::size_t(i) * part_size];
            Partition part;
            std::memcpy(part.key, p, 16);
            for (int j = 0; j < 2; ++j)
                part.data[j] = {be32(p + 16 + j * 16), be32(p + 20 + j * 16), be32(p + 24 + j * 16),
                                be32(p + 28 + j * 16)};
            partitions.push_back(part);
        }
        std::vector<u8> in, out;
        in.resize(raw_size);
        if (!file.read(raw_off, in.data(), in.size()) || !decompress(in, true, out, u64(n_raw) * 0x18 + 64) ||
            out.size() < u64(n_raw) * 0x18)
            return false;
        for (u32 i = 0; i < n_raw; ++i)
        {
            const u8 *p = &out[std::size_t(i) * 0x18];
            raws.push_back({be64(p), be64(p + 8), be32(p + 16), be32(p + 20)});
        }
        const u32 entry = rvz ? 12 : 8;
        in.resize(groups_size);
        if (!file.read(groups_off, in.data(), in.size()) || !decompress(in, true, out, u64(n_groups) * entry + 64) ||
            out.size() < u64(n_groups) * entry)
            return false;
        for (u32 i = 0; i < n_groups; ++i)
        {
            const u8 *p = &out[std::size_t(i) * entry];
            const u32 size = be32(p + 4);
            groups.push_back({u64(be32(p)) << 2, rvz ? size & 0x7FFFFFFFu : size, rvz ? (size & 0x80000000u) != 0 : true,
                              rvz ? be32(p + 8) : 0});
        }
        return true;
    }

    /* One group's data, decompressed, its hash exceptions skipped, packing undone. */
    bool load_group(u32 index, u64 data_size, u32 exception_lists, std::vector<u8> &data)
    {
        if (index >= groups.size())
            return false;
        const Group &g = groups[index];
        if (g.offset == cached_offset && cached.size() == data_size)
        {
            data = cached;
            return true;
        }
        if (g.size == 0)
        {
            data.assign(data_size, 0);
            return true;
        }
        std::vector<u8> in(g.size), out;
        if (!file.read(g.offset, in.data(), in.size()))
            return false;
        const bool comp = g.compressed && compression > 1;
        if (!decompress(in, comp, out, data_size + g.packed + u64(exception_lists) * 0x10000 + 0x1000))
            return false;
        /* Hash exceptions first: u16 count, then 22 bytes each; padded to four
         * bytes when the group isn't compressed. */
        std::size_t at = 0;
        for (u32 l = 0; l < exception_lists; ++l)
        {
            if (at + 2 > out.size())
                return false;
            at += 2 + std::size_t(be16(&out[at])) * 22;
        }
        if (exception_lists && !comp)
            at = (at + 3) & ~std::size_t(3);
        if (at > out.size())
            return false;
        data.assign(data_size, 0);
        if (g.packed == 0)
        {
            std::memcpy(data.data(), out.data() + at, std::min<u64>(data_size, out.size() - at));
        }
        else
        {
            /* RVZ packing: runs of real data and of junk (made from a seed;
             * file data never lies in junk, so zeros stand in for it). */
            u64 w = 0;
            while (at + 4 <= out.size() && w < data_size)
            {
                const u32 size = be32(&out[at]);
                at += 4;
                const u64 n = size & 0x7FFFFFFFu;
                if (size & 0x80000000u)
                {
                    at += 17 * 4;
                    w += n;
                    continue;
                }
                const u64 copy = std::min<u64>({n, data_size - w, out.size() - std::min<u64>(at, out.size())});
                std::memcpy(data.data() + w, out.data() + at, copy);
                at += n;
                w += n;
            }
        }
        cached_offset = g.offset;
        cached = data;
        return true;
    }

    /* Reads [*offset, +size) out of a run of groups covering data_offset..+data_size. */
    bool from_groups(u64 &offset, u64 &size, u8 *&out, u64 chunk_size, u64 data_offset, u64 data_size, u32 group,
                     u32 n_groups, u32 exception_lists, u64 sector)
    {
        if (data_offset + data_size <= offset)
            return true;
        if (offset < data_offset)
            return false;
        const u64 skipped = data_offset % sector;
        data_offset -= skipped;
        data_size += skipped;
        for (u64 i = (offset - data_offset) / chunk_size; i < n_groups && size > 0; ++i)
        {
            const u64 in_data = i * chunk_size;
            const u64 this_chunk = std::min(chunk_size, data_size - in_data);
            const u64 within = offset - in_data - data_offset;
            const u64 n = std::min(this_chunk - within, size);
            std::vector<u8> g;
            if (!load_group(u32(group + i), this_chunk, exception_lists, g))
                return false;
            std::memcpy(out, g.data() + within, n);
            offset += n;
            size -= n;
            out += n;
        }
        return true;
    }

    bool read(u64 offset, u64 size, u8 *out) override
    {
        if (offset < 0x80)
        {
            const u64 n = std::min<u64>(0x80 - offset, size);
            std::memcpy(out, disc_header + offset, n);
            offset += n;
            size -= n;
            out += n;
        }
        /* Outside the partitions only (the partition table, a ticket, a
         * GameCube disc): the raw data runs. */
        for (const Raw &r : raws)
        {
            if (size == 0)
                break;
            if (r.size == 0 || offset >= r.offset + r.size || offset < r.offset - r.offset % kWiiSector)
                continue;
            if (!from_groups(offset, size, out, chunk, r.offset, r.size, r.group, r.groups, 0, kWiiSector))
                return false;
        }
        return size == 0;
    }
    bool stores_decrypted() const override { return true; }
    const Partition *find(u64 partition_data_offset) const
    {
        for (const Partition &p : partitions)
            if (u64(p.data[0].first_sector) * kWiiSector == partition_data_offset)
                return &p;
        return nullptr;
    }
    bool partition_key(u64 partition_data_offset, u8 key[16]) override
    {
        const Partition *p = find(partition_data_offset);
        if (!p)
            return false;
        std::memcpy(key, p->key, 16);
        return true;
    }
    bool read_decrypted(u64 partition_data_offset, u64 offset, u64 size, u8 *out, const u8 *) override
    {
        const Partition *p = find(partition_data_offset);
        if (!p)
            return false;
        const u64 chunk_data = u64(chunk) * kWiiSectorData / kWiiSector;
        const u32 lists = std::max<u32>(1, u32(chunk_data / kWiiGroupData));
        for (const PartData &d : p->data)
        {
            if (size == 0)
                break;
            if (d.sectors == 0)
                continue;
            const u64 data_offset = u64(d.first_sector - p->data[0].first_sector) * kWiiSectorData;
            const u64 data_size = u64(d.sectors) * kWiiSectorData;
            if (!from_groups(offset, size, out, chunk_data, data_offset, data_size, d.group, d.groups, lists,
                             kWiiSectorData))
                return false;
        }
        return size == 0;
    }
};

std::unique_ptr<Image> open_image(const std::string &path, std::string &error)
{
    std::FILE *probe = std::fopen(path.c_str(), "rb");
    if (!probe)
    {
        error = "can't open the file";
        return nullptr;
    }
    u8 magic[4] = {};
    const bool got = std::fread(magic, 1, 4, probe) == 4;
    std::fclose(probe);
    if (!got)
    {
        error = "the file is too short";
        return nullptr;
    }
    std::unique_ptr<Image> image;
    bool ok = false;
    if (std::memcmp(magic, "RVZ\x01", 4) == 0 || std::memcmp(magic, "WIA\x01", 4) == 0)
    {
        auto w = std::make_unique<Wia>();
        ok = w->file.open(path) && w->init();
        if (!ok)
            error = w->compression > 1 && w->compression != 5 ? "this .wia / .rvz uses bzip2 or LZMA, not read here"
                                                                 : "not a readable .rvz / .wia";
        image = std::move(w);
    }
    else if (std::memcmp(magic, "WBFS", 4) == 0)
    {
        auto w = std::make_unique<Wbfs>();
        ok = w->file.open(path) && w->init();
        error = "not a readable .wbfs";
        image = std::move(w);
    }
    else if (std::memcmp(magic, "CISO", 4) == 0)
    {
        auto c = std::make_unique<Ciso>();
        ok = c->file.open(path) && c->init();
        error = "not a readable .ciso";
        image = std::move(c);
    }
    else if (le32(magic) == 0xB10BC001)
    {
        auto c = std::make_unique<Gcz>();
        ok = c->file.open(path) && c->init();
        error = "not a readable .gcz";
        image = std::move(c);
    }
    else
    {
        auto p = std::make_unique<Plain>();
        ok = p->file.open(path);
        error = "can't open the file";
        image = std::move(p);
    }
    if (!ok)
        return nullptr;
    error.clear();
    return image;
}

/* A Wii disc's data partition, decrypted as it is read. */
struct Partition
{
    Image *image = nullptr;
    u64 data_offset = 0; /* where its data starts on the disc */
    u8 key[16] = {};
    std::unique_ptr<Aes> aes;
    u64 cached_sector = ~u64(0);
    u8 sector[kWiiSector];

    bool read(u64 offset, u64 size, u8 *out)
    {
        if (image->stores_decrypted())
            return image->read_decrypted(data_offset, offset, size, out, key);
        while (size)
        {
            const u64 s = offset / kWiiSectorData, within = offset % kWiiSectorData;
            if (s != cached_sector)
            {
                if (!image->read(data_offset + s * kWiiSector, kWiiSector, sector))
                    return false;
                u8 iv[16];
                std::memcpy(iv, sector + 0x3D0, 16);
                for (u64 i = kWiiSectorHashes; i < kWiiSector; i += 16)
                {
                    u8 block[16], next_iv[16];
                    std::memcpy(block, sector + i, 16);
                    std::memcpy(next_iv, block, 16);
                    aes->decrypt_block(block);
                    for (int k = 0; k < 16; ++k)
                        sector[i + k] = u8(block[k] ^ iv[k]);
                    std::memcpy(iv, next_iv, 16);
                }
                cached_sector = s;
            }
            const u64 n = std::min(size, kWiiSectorData - within);
            std::memcpy(out, sector + kWiiSectorHashes + within, n);
            offset += n;
            size -= n;
            out += n;
        }
        return true;
    }
};

/* Finds name in a file system table (the root folder, or a path with '/'). */
bool find_in_fst(const std::vector<u8> &fst, const std::string &name, bool wii, u64 &offset, u64 &size)
{
    if (fst.size() < 12)
        return false;
    const u32 count = be32(&fst[8]);
    if (count == 0 || u64(count) * 12 > fst.size())
        return false;
    const std::size_t strings = std::size_t(count) * 12;
    auto entry_name = [&](u32 i) {
        const u32 off = be32(&fst[i * 12]) & 0xFFFFFF;
        std::string s;
        for (std::size_t k = strings + off; k < fst.size() && fst[k]; ++k)
            s += char(fst[k]);
        return s;
    };
    auto lower = [](std::string s) {
        for (char &c : s)
            if (c >= 'A' && c <= 'Z')
                c = char(c - 'A' + 'a');
        return s;
    };
    /* Walk the path one folder at a time. */
    std::string rest = lower(name);
    u32 first = 1, end = count;
    while (true)
    {
        const auto slash = rest.find('/');
        const std::string part = rest.substr(0, slash);
        bool found = false;
        for (u32 i = first; i < end;)
        {
            const bool dir = fst[i * 12] != 0;
            const u32 next = dir ? be32(&fst[i * 12 + 8]) : i + 1;
            if (lower(entry_name(i)) == part)
            {
                if (slash == std::string::npos && !dir)
                {
                    offset = u64(be32(&fst[i * 12 + 4])) << (wii ? 2 : 0);
                    size = be32(&fst[i * 12 + 8]);
                    return true;
                }
                if (slash != std::string::npos && dir)
                {
                    first = i + 1;
                    end = std::min(next, count);
                    rest = rest.substr(slash + 1);
                    found = true;
                    break;
                }
            }
            i = next > i ? next : i + 1;
        }
        if (!found)
            return false;
    }
}
} // namespace

void aes_cbc_decrypt(const std::uint8_t key[16], std::uint8_t iv[16], std::uint8_t *data, std::size_t size)
{
    Aes aes(key);
    for (std::size_t i = 0; i + 16 <= size; i += 16)
    {
        u8 next[16];
        std::memcpy(next, data + i, 16);
        aes.decrypt_block(data + i);
        for (int k = 0; k < 16; ++k)
            data[i + k] ^= iv[k];
        std::memcpy(iv, next, 16);
    }
}

bool read_file(const std::string &path, const std::string &name, std::vector<std::uint8_t> &out, std::string &error)
{
    std::unique_ptr<Image> image = open_image(path, error);
    if (!image)
        return false;
    u8 header[0x440];
    if (!image->read(0, sizeof header, header))
    {
        error = "can't read the disc header";
        return false;
    }
    const bool wii = be32(header + 0x18) == 0x5D1C9EA3;
    const bool gc = be32(header + 0x1C) == 0xC2339F3D;
    if (!wii && !gc)
    {
        error = "not a GameCube or Wii disc";
        return false;
    }
    std::vector<u8> fst;
    if (gc)
    {
        const u64 fst_off = be32(header + 0x424), fst_size = be32(header + 0x428);
        if (fst_size == 0 || fst_size > (32u << 20))
        {
            error = "no file system";
            return false;
        }
        fst.resize(fst_size);
        u64 off = 0, size = 0;
        if (!image->read(fst_off, fst_size, fst.data()) || !find_in_fst(fst, name, false, off, size))
        {
            error = name + " is not on the disc";
            return false;
        }
        out.resize(size);
        if (!image->read(off, size, out.data()))
        {
            error = "can't read " + name;
            return false;
        }
        return true;
    }

    /* Wii: the partition table, the data partition (type 0), its ticket. */
    u8 info[0x20];
    if (!image->read(0x40000, sizeof info, info))
    {
        error = "can't read the partition table";
        return false;
    }
    u64 part = 0;
    for (int t = 0; t < 4 && !part; ++t)
    {
        const u32 n = be32(info + t * 8);
        const u64 table = u64(be32(info + t * 8 + 4)) << 2;
        if (n == 0 || n > 64)
            continue;
        std::vector<u8> entries(n * 8);
        if (!image->read(table, entries.size(), entries.data()))
            continue;
        for (u32 i = 0; i < n; ++i)
            if (be32(&entries[i * 8 + 4]) == 0)
            {
                part = u64(be32(&entries[i * 8])) << 2;
                break;
            }
    }
    if (!part)
    {
        error = "no data partition";
        return false;
    }
    u8 ph[0x2C0];
    if (!image->read(part, sizeof ph, ph))
    {
        error = "can't read the partition header";
        return false;
    }
    Partition p;
    p.image = image.get();
    p.data_offset = part + (u64(be32(ph + 0x2B8)) << 2);
    if (!image->partition_key(p.data_offset, p.key))
    {
        /* The title key, encrypted with the common key the ticket names; the
         * title ID is its IV. */
        const u8 index = ph[0x1F1] == 1 ? 1 : 0;
        std::memcpy(p.key, ph + 0x1BF, 16);
        u8 iv[16] = {};
        std::memcpy(iv, ph + 0x1DC, 8);
        aes_cbc_decrypt(kCommonKeys[index], iv, p.key, 16);
    }
    p.aes = std::make_unique<Aes>(p.key);
    u8 boot[0x440];
    if (!p.read(0, sizeof boot, boot))
    {
        error = "can't read the partition";
        return false;
    }
    const u64 fst_off = u64(be32(boot + 0x424)) << 2, fst_size = u64(be32(boot + 0x428)) << 2;
    if (fst_size == 0 || fst_size > (32u << 20))
    {
        error = "no file system in the partition (wrong key?)";
        return false;
    }
    fst.resize(fst_size);
    u64 off = 0, size = 0;
    if (!p.read(fst_off, fst_size, fst.data()) || !find_in_fst(fst, name, true, off, size))
    {
        error = name + " is not on the disc";
        return false;
    }
    if (size > (64u << 20))
    {
        error = name + " is too big";
        return false;
    }
    out.resize(size);
    if (!p.read(off, size, out.data()))
    {
        error = "can't read " + name;
        return false;
    }
    return true;
}
} // namespace porpoise::disc
