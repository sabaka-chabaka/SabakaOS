#include "fat32.h"
#include "ata.h"
#include "heap.h"
#include "kstring.h"

struct __attribute__((packed)) Fat32BPB {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors16;
    uint8_t  media_type;
    uint16_t fat_size16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
    uint32_t fat_size32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
};

struct __attribute__((packed)) Fat32DirEntry {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t cluster_hi;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_lo;
    uint32_t size;
};

struct __attribute__((packed)) Fat32LFNEntry {
    uint8_t  order;
    uint16_t name1[5];
    uint8_t  attr;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t cluster;
    uint16_t name3[2];
};

static bool     s_mounted           = false;
static uint32_t s_fat_start_lba     = 0;
static uint32_t s_fat_size_sectors  = 0;
static uint32_t s_data_start_lba    = 0;
static uint32_t s_root_cluster      = 0;
static uint32_t s_sectors_per_clust = 0;
static uint32_t s_bytes_per_clust   = 0;

static uint8_t  s_fat_cache[512];
static uint32_t s_fat_cache_lba = 0xFFFFFFFF;
static bool     s_fat_cache_dirty = false;

static uint32_t cluster_to_lba(uint32_t cluster) {
    return s_data_start_lba + (cluster - 2) * s_sectors_per_clust;
}

static bool fat_flush_cache() {
    if (!s_fat_cache_dirty) return true;
    if (!ata_write_sectors(s_fat_cache_lba, 1, s_fat_cache)) return false;
    s_fat_cache_dirty = false;
    return true;
}

static bool fat_read_sector(uint32_t lba) {
    if (s_fat_cache_lba == lba) return true;
    if (!fat_flush_cache()) return false;
    if (!ata_read_sectors(lba, 1, s_fat_cache)) return false;
    s_fat_cache_lba = lba;
    return true;
}

static uint32_t fat_get(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t lba = s_fat_start_lba + fat_offset / 512;
    uint32_t off = fat_offset % 512;
    if (!fat_read_sector(lba)) return 0;
    uint32_t val;
    val  = (uint32_t)s_fat_cache[off];
    val |= (uint32_t)s_fat_cache[off+1] << 8;
    val |= (uint32_t)s_fat_cache[off+2] << 16;
    val |= (uint32_t)s_fat_cache[off+3] << 24;
    return val & 0x0FFFFFFF;
}

static bool fat_set(uint32_t cluster, uint32_t value) {
    value &= 0x0FFFFFFF;
    for (uint8_t fat_num = 0; fat_num < 2; fat_num++) {
        uint32_t fat_offset = cluster * 4;
        uint32_t lba = s_fat_start_lba + fat_num * s_fat_size_sectors + fat_offset / 512;
        uint32_t off = fat_offset % 512;
        if (!fat_read_sector(lba)) return false;
        uint32_t old_hi = ((uint32_t)s_fat_cache[off+3] & 0xF0) << 24;
        s_fat_cache[off]   = (uint8_t)(value);
        s_fat_cache[off+1] = (uint8_t)(value >> 8);
        s_fat_cache[off+2] = (uint8_t)(value >> 16);
        s_fat_cache[off+3] = (uint8_t)((value >> 24) | (old_hi >> 24));
        s_fat_cache_dirty = true;
        if (!fat_flush_cache()) return false;
    }
    return true;
}

static uint32_t fat_alloc_cluster() {
    uint32_t max = ata_sectors_count() / s_sectors_per_clust;
    for (uint32_t c = 2; c < max; c++) {
        if (fat_get(c) == 0x00000000) {
            fat_set(c, FAT32_EOF);
            return c;
        }
    }
    return 0;
}

static void fat_free_chain(uint32_t start) {
    while (start >= 2 && start < FAT32_EOF) {
        uint32_t next = fat_get(start);
        fat_set(start, 0);
        start = next;
    }
}

static bool cluster_read(uint32_t cluster, void* buf) {
    uint32_t lba = cluster_to_lba(cluster);
    for (uint32_t i = 0; i < s_sectors_per_clust; i++) {
        if (!ata_read_sectors(lba + i, 1, (uint8_t*)buf + i * 512))
            return false;
    }
    return true;
}

static bool cluster_write(uint32_t cluster, const void* buf) {
    uint32_t lba = cluster_to_lba(cluster);
    for (uint32_t i = 0; i < s_sectors_per_clust; i++) {
        if (!ata_write_sectors(lba + i, 1, (const uint8_t*)buf + i * 512))
            return false;
    }
    return true;
}

static char ucs2_to_ascii(uint16_t c) {
    if (c >= 0x20 && c <= 0x7E) return (char)(c & 0xFF);
    return '_';
}

static void lfn_collect(Fat32LFNEntry* lfn_entries, int cnt, char* out, uint32_t out_size) {
    uint32_t pos = 0;
    for (int e = cnt - 1; e >= 0 && pos + 1 < out_size; e--) {
        Fat32LFNEntry* le = &lfn_entries[e];
        for (int i = 0; i < 5 && pos + 1 < out_size; i++) {
            uint16_t c = le->name1[i];
            if (c == 0xFFFF || c == 0x0000) goto done;
            out[pos++] = ucs2_to_ascii(c);
        }
        for (int i = 0; i < 6 && pos + 1 < out_size; i++) {
            uint16_t c = le->name2[i];
            if (c == 0xFFFF || c == 0x0000) goto done;
            out[pos++] = ucs2_to_ascii(c);
        }
        for (int i = 0; i < 2 && pos + 1 < out_size; i++) {
            uint16_t c = le->name3[i];
            if (c == 0xFFFF || c == 0x0000) goto done;
            out[pos++] = ucs2_to_ascii(c);
        }
    }
done:
    out[pos] = 0;
}

static void parse_83_name(const Fat32DirEntry* e, char* out) {
    int pos = 0;
    for (int i = 0; i < 8; i++) {
        if (e->name[i] == ' ') break;
        out[pos++] = e->name[i];
    }
    if (e->ext[0] != ' ') {
        out[pos++] = '.';
        for (int i = 0; i < 3; i++) {
            if (e->ext[i] == ' ') break;
            out[pos++] = e->ext[i];
        }
    }
    out[pos] = 0;
}

static void make_83(const char* name, char short_name[8], char short_ext[3]) {
    int i = 0, j = 0;
    for (i = 0; i < 8; i++) short_name[i] = ' ';
    for (i = 0; i < 3; i++) short_ext[i]  = ' ';

    i = 0; j = 0;
    const char* dot = nullptr;
    for (const char* p = name; *p; p++) if (*p == '.') dot = p;

    const char* end = dot ? dot : (name + kstrlen(name));
    for (const char* p = name; p < end && i < 8; p++, i++) {
        char c = *p;
        if (c >= 'a' && c <= 'z') c -= 32;
        short_name[i] = c;
    }
    if (dot) {
        for (const char* p = dot + 1; *p && j < 3; p++, j++) {
            char c = *p;
            if (c >= 'a' && c <= 'z') c -= 32;
            short_ext[j] = c;
        }
    }
}

static int kstricmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a >= 'a' && *a <= 'z' ? *a - 32 : *a;
        char cb = *b >= 'a' && *b <= 'z' ? *b - 32 : *b;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

bool fat32_init() {
    s_mounted = false;
    if (!ata_is_present()) return false;

    uint8_t sector[512];
    if (!ata_read_sectors(0, 1, sector)) return false;

    if (sector[510] != 0x55 || sector[511] != 0xAA) return false;

    uint32_t partition_lba = 0;

    uint8_t part_type = sector[446 + 4];
    if (part_type == 0x0B || part_type == 0x0C || part_type == 0x1B || part_type == 0x1C) {
        partition_lba  = (uint32_t)sector[446 + 8];
        partition_lba |= (uint32_t)sector[446 + 9]  << 8;
        partition_lba |= (uint32_t)sector[446 + 10] << 16;
        partition_lba |= (uint32_t)sector[446 + 11] << 24;
        if (!ata_read_sectors(partition_lba, 1, sector)) return false;
    }

    Fat32BPB* bpb = (Fat32BPB*)sector;

    if (bpb->bytes_per_sector != 512) return false;
    if (bpb->num_fats == 0)           return false;
    if (bpb->fat_size32 == 0)         return false;

    s_sectors_per_clust = bpb->sectors_per_cluster;
    s_bytes_per_clust   = s_sectors_per_clust * 512;
    s_fat_start_lba     = partition_lba + bpb->reserved_sectors;
    s_fat_size_sectors  = bpb->fat_size32;
    s_data_start_lba    = s_fat_start_lba + bpb->num_fats * s_fat_size_sectors;
    s_root_cluster      = bpb->root_cluster;

    s_fat_cache_lba   = 0xFFFFFFFF;
    s_fat_cache_dirty = false;

    s_mounted = true;
    return true;
}

bool fat32_is_mounted()     { return s_mounted; }
uint32_t fat32_root_cluster() { return s_root_cluster; }

int fat32_readdir(uint32_t dir_cluster, uint32_t index, Fat32Entry* out) {
    if (!s_mounted) return FAT32_ERR_INVAL;

    uint8_t* clust_buf = (uint8_t*)kmalloc(s_bytes_per_clust);
    if (!clust_buf) return FAT32_ERR_IO;

    uint32_t entry_idx = 0;
    uint32_t raw_idx   = 0;

    Fat32LFNEntry lfn_buf[20];
    int lfn_count = 0;
    char lfn_name[FAT32_MAX_NAME];
    lfn_name[0] = 0;

    uint32_t cur_cluster = dir_cluster ? dir_cluster : s_root_cluster;
    uint32_t entries_per_clust = s_bytes_per_clust / 32;

    while (cur_cluster < FAT32_EOF && cur_cluster >= 2) {
        if (!cluster_read(cur_cluster, clust_buf)) {
            kfree(clust_buf);
            return FAT32_ERR_IO;
        }

        for (uint32_t i = 0; i < entries_per_clust; i++, raw_idx++) {
            Fat32DirEntry* de = (Fat32DirEntry*)(clust_buf + i * 32);

            if ((uint8_t)de->name[0] == 0x00) {
                kfree(clust_buf);
                return FAT32_ERR_NOENT;
            }
            if ((uint8_t)de->name[0] == 0xE5) {
                lfn_count = 0;
                lfn_name[0] = 0;
                continue;
            }
            if (de->attr == FAT32_ATTR_LFN) {
                if (lfn_count < 20) {
                    lfn_buf[lfn_count++] = *(Fat32LFNEntry*)de;
                }
                continue;
            }
            if (de->attr & FAT32_ATTR_VOLUME_ID) {
                lfn_count = 0;
                lfn_name[0] = 0;
                continue;
            }

            if (entry_idx == index) {
                if (lfn_count > 0) {
                    lfn_collect(lfn_buf, lfn_count, lfn_name, FAT32_MAX_NAME);
                    kstrcpy(out->name, lfn_name);
                } else {
                    parse_83_name(de, out->name);
                }
                out->attr        = de->attr;
                out->cluster     = ((uint32_t)de->cluster_hi << 16) | de->cluster_lo;
                out->size        = de->size;
                out->dir_cluster = dir_cluster;
                out->dir_index   = raw_idx;

                kfree(clust_buf);
                return FAT32_OK;
            }

            entry_idx++;
            lfn_count = 0;
            lfn_name[0] = 0;
        }

        cur_cluster = fat_get(cur_cluster);
    }

    kfree(clust_buf);
    return FAT32_ERR_NOENT;
}

int fat32_find(uint32_t dir_cluster, const char* name, Fat32Entry* out) {
    Fat32Entry e;
    for (uint32_t i = 0; ; i++) {
        int r = fat32_readdir(dir_cluster, i, &e);
        if (r == FAT32_ERR_NOENT) return FAT32_ERR_NOENT;
        if (r != FAT32_OK) return r;
        // Пропускаем . и ..
        if (kstrcmp(e.name, ".") == 0 || kstrcmp(e.name, "..") == 0) continue;
        if (kstricmp(e.name, name) == 0) {
            *out = e;
            return FAT32_OK;
        }
    }
}

int fat32_resolve(const char* path, Fat32Entry* out) {
    if (!path || !path[0]) return FAT32_ERR_INVAL;

    uint32_t cur = s_root_cluster;
    uint32_t i   = 0;

    if (path[0] == '/') {
        i = 1;
        if (!path[1]) {
            // Это сам корень
            out->cluster     = s_root_cluster;
            out->attr        = FAT32_ATTR_DIRECTORY;
            out->size        = 0;
            out->dir_cluster = 0;
            out->dir_index   = 0;
            kstrcpy(out->name, "/");
            return FAT32_OK;
        }
    }

    Fat32Entry e;
    char component[256];
    while (path[i]) {
        while (path[i] == '/') i++;
        if (!path[i]) break;

        uint32_t j = 0;
        while (path[i] && path[i] != '/' && j < 255)
            component[j++] = path[i++];
        component[j] = 0;

        if (kstrcmp(component, ".") == 0) continue;
        if (kstrcmp(component, "..") == 0) {
            cur = s_root_cluster;
            continue;
        }

        int r = fat32_find(cur, component, &e);
        if (r != FAT32_OK) return r;

        cur = e.cluster ? e.cluster : s_root_cluster;
    }

    *out = e;
    return FAT32_OK;
}

int fat32_read(const Fat32Entry* entry, uint8_t* buf, uint32_t offset, uint32_t size) {
    if (!s_mounted || !entry) return FAT32_ERR_INVAL;
    if (entry->attr & FAT32_ATTR_DIRECTORY) return FAT32_ERR_ISDIR;
    if (offset >= entry->size) return 0;
    if (offset + size > entry->size) size = entry->size - offset;
    if (size == 0) return 0;

    uint8_t* clust_buf = (uint8_t*)kmalloc(s_bytes_per_clust);
    if (!clust_buf) return FAT32_ERR_IO;

    uint32_t cluster = entry->cluster;
    uint32_t clust_idx_start = offset / s_bytes_per_clust;
    uint32_t clust_off       = offset % s_bytes_per_clust;

    for (uint32_t k = 0; k < clust_idx_start && cluster < FAT32_EOF; k++)
        cluster = fat_get(cluster);

    uint32_t bytes_read = 0;
    while (size > 0 && cluster >= 2 && cluster < FAT32_EOF) {
        if (!cluster_read(cluster, clust_buf)) {
            kfree(clust_buf);
            return FAT32_ERR_IO;
        }
        uint32_t avail = s_bytes_per_clust - clust_off;
        uint32_t to_copy = size < avail ? size : avail;
        for (uint32_t k = 0; k < to_copy; k++)
            buf[bytes_read + k] = clust_buf[clust_off + k];
        bytes_read += to_copy;
        size       -= to_copy;
        clust_off   = 0;
        cluster = fat_get(cluster);
    }

    kfree(clust_buf);
    return (int)bytes_read;
}

int fat32_write(Fat32Entry* entry, const uint8_t* buf, uint32_t offset, uint32_t size) {
    if (!s_mounted || !entry) return FAT32_ERR_INVAL;
    if (entry->attr & FAT32_ATTR_DIRECTORY) return FAT32_ERR_ISDIR;

    uint8_t* clust_buf = (uint8_t*)kmalloc(s_bytes_per_clust);
    if (!clust_buf) return FAT32_ERR_IO;

    if (entry->cluster == 0) {
        uint32_t c = fat_alloc_cluster();
        if (!c) { kfree(clust_buf); return FAT32_ERR_NOSPACE; }
        entry->cluster = c;
        // TODO: dot update by dir_cluster/dir_index
    }

    uint32_t cluster = entry->cluster;
    uint32_t clust_idx_start = offset / s_bytes_per_clust;
    uint32_t clust_off       = offset % s_bytes_per_clust;
    uint32_t prev_cluster    = 0;

    uint32_t cur = cluster;
    for (uint32_t k = 0; k < clust_idx_start; k++) {
        uint32_t next = fat_get(cur);
        if (next >= FAT32_EOF || next < 2) {
            uint32_t new_c = fat_alloc_cluster();
            if (!new_c) { kfree(clust_buf); return FAT32_ERR_NOSPACE; }
            fat_set(cur, new_c);
            next = new_c;
        }
        prev_cluster = cur;
        cur = next;
    }
    cluster = cur;

    uint32_t bytes_written = 0;
    uint32_t remaining = size;

    while (remaining > 0) {
        if (!cluster_read(cluster, clust_buf)) {
            kfree(clust_buf);
            return FAT32_ERR_IO;
        }
        uint32_t avail = s_bytes_per_clust - clust_off;
        uint32_t to_write = remaining < avail ? remaining : avail;
        for (uint32_t k = 0; k < to_write; k++)
            clust_buf[clust_off + k] = buf[bytes_written + k];
        if (!cluster_write(cluster, clust_buf)) {
            kfree(clust_buf);
            return FAT32_ERR_IO;
        }
        bytes_written += to_write;
        remaining     -= to_write;
        clust_off      = 0;

        if (remaining > 0) {
            uint32_t next = fat_get(cluster);
            if (next >= FAT32_EOF || next < 2) {
                uint32_t new_c = fat_alloc_cluster();
                if (!new_c) break;
                fat_set(cluster, new_c);
                next = new_c;
            }
            cluster = next;
        }
    }

    uint32_t new_end = offset + bytes_written;
    if (new_end > entry->size) entry->size = new_end;

    kfree(clust_buf);
    return (int)bytes_written;
}

static int write_dir_entry(uint32_t dir_cluster, const char* name, uint8_t attr,
                            uint32_t cluster, uint32_t size, uint32_t* out_raw_idx)
{
    uint8_t* clust_buf = (uint8_t*)kmalloc(s_bytes_per_clust);
    if (!clust_buf) return FAT32_ERR_IO;

    uint32_t entries_per_clust = s_bytes_per_clust / 32;
    uint32_t raw_idx = 0;
    uint32_t cur = dir_cluster ? dir_cluster : s_root_cluster;

    while (cur >= 2 && cur < FAT32_EOF) {
        if (!cluster_read(cur, clust_buf)) {
            kfree(clust_buf);
            return FAT32_ERR_IO;
        }
        for (uint32_t i = 0; i < entries_per_clust; i++, raw_idx++) {
            Fat32DirEntry* de = (Fat32DirEntry*)(clust_buf + i * 32);
            if ((uint8_t)de->name[0] == 0x00 || (uint8_t)de->name[0] == 0xE5) {
                char sname[8], sext[3];
                make_83(name, sname, sext);
                for (int k = 0; k < 8; k++) de->name[k] = sname[k];
                for (int k = 0; k < 3; k++) de->ext[k]  = sext[k];
                de->attr        = attr;
                de->reserved    = 0;
                de->cluster_hi  = (uint16_t)(cluster >> 16);
                de->cluster_lo  = (uint16_t)(cluster);
                de->size        = size;
                de->create_date = 0x5821;  // zaglushka todo (02.04.2026)
                de->write_date  = 0x5821;
                de->last_access_date = 0x5821;
                de->create_time = 0;
                de->write_time  = 0;
                de->create_time_tenth = 0;

                if (!cluster_write(cur, clust_buf)) {
                    kfree(clust_buf);
                    return FAT32_ERR_IO;
                }
                if (out_raw_idx) *out_raw_idx = raw_idx;
                kfree(clust_buf);
                return FAT32_OK;
            }
        }
        uint32_t next = fat_get(cur);
        if (next >= FAT32_EOF || next < 2) {
            uint32_t new_c = fat_alloc_cluster();
            if (!new_c) { kfree(clust_buf); return FAT32_ERR_NOSPACE; }
            for (uint32_t k = 0; k < s_bytes_per_clust; k++) clust_buf[k] = 0;
            cluster_write(new_c, clust_buf);
            fat_set(cur, new_c);
            cur = new_c;
        } else {
            cur = next;
        }
    }

    kfree(clust_buf);
    return FAT32_ERR_NOSPACE;
}

int fat32_create(uint32_t dir_cluster, const char* name, Fat32Entry* out) {
    if (!s_mounted) return FAT32_ERR_INVAL;

    Fat32Entry existing;
    if (fat32_find(dir_cluster, name, &existing) == FAT32_OK)
        return FAT32_ERR_EXIST;

    uint32_t raw_idx = 0;
    int r = write_dir_entry(dir_cluster, name, FAT32_ATTR_ARCHIVE, 0, 0, &raw_idx);
    if (r != FAT32_OK) return r;

    kstrcpy(out->name, name);
    out->attr        = FAT32_ATTR_ARCHIVE;
    out->cluster     = 0;
    out->size        = 0;
    out->dir_cluster = dir_cluster;
    out->dir_index   = raw_idx;
    return FAT32_OK;
}

int fat32_mkdir(uint32_t dir_cluster, const char* name, Fat32Entry* out) {
    if (!s_mounted) return FAT32_ERR_INVAL;

    Fat32Entry existing;
    if (fat32_find(dir_cluster, name, &existing) == FAT32_OK)
        return FAT32_ERR_EXIST;

    uint32_t new_c = fat_alloc_cluster();
    if (!new_c) return FAT32_ERR_NOSPACE;

    uint8_t* buf = (uint8_t*)kmalloc(s_bytes_per_clust);
    if (!buf) { fat_set(new_c, 0); return FAT32_ERR_IO; }
    for (uint32_t k = 0; k < s_bytes_per_clust; k++) buf[k] = 0;

    Fat32DirEntry* dot = (Fat32DirEntry*)buf;
    for (int k = 0; k < 8; k++) dot->name[k] = ' ';
    for (int k = 0; k < 3; k++) dot->ext[k]  = ' ';
    dot->name[0]   = '.';
    dot->attr      = FAT32_ATTR_DIRECTORY;
    dot->cluster_hi = (uint16_t)(new_c >> 16);
    dot->cluster_lo = (uint16_t)(new_c);

    Fat32DirEntry* dotdot = (Fat32DirEntry*)(buf + 32);
    for (int k = 0; k < 8; k++) dotdot->name[k] = ' ';
    for (int k = 0; k < 3; k++) dotdot->ext[k]  = ' ';
    dotdot->name[0]    = '.';
    dotdot->name[1]    = '.';
    dotdot->attr       = FAT32_ATTR_DIRECTORY;
    uint32_t parent_c  = dir_cluster ? dir_cluster : s_root_cluster;
    dotdot->cluster_hi = (uint16_t)(parent_c >> 16);
    dotdot->cluster_lo = (uint16_t)(parent_c);

    cluster_write(new_c, buf);
    kfree(buf);

    uint32_t raw_idx = 0;
    int r = write_dir_entry(dir_cluster, name, FAT32_ATTR_DIRECTORY, new_c, 0, &raw_idx);
    if (r != FAT32_OK) { fat_free_chain(new_c); return r; }

    kstrcpy(out->name, name);
    out->attr        = FAT32_ATTR_DIRECTORY;
    out->cluster     = new_c;
    out->size        = 0;
    out->dir_cluster = dir_cluster;
    out->dir_index   = raw_idx;
    return FAT32_OK;
}

static int mark_deleted(uint32_t dir_cluster, uint32_t raw_idx) {
    uint32_t entries_per_clust = s_bytes_per_clust / 32;
    uint32_t clust_n = raw_idx / entries_per_clust;
    uint32_t local_i = raw_idx % entries_per_clust;

    uint32_t cur = dir_cluster ? dir_cluster : s_root_cluster;
    for (uint32_t k = 0; k < clust_n; k++) {
        cur = fat_get(cur);
        if (cur < 2 || cur >= FAT32_EOF) return FAT32_ERR_NOENT;
    }

    uint8_t* buf = (uint8_t*)kmalloc(s_bytes_per_clust);
    if (!buf) return FAT32_ERR_IO;
    if (!cluster_read(cur, buf)) { kfree(buf); return FAT32_ERR_IO; }
    buf[local_i * 32] = 0xE5;
    bool ok = cluster_write(cur, buf);
    kfree(buf);
    return ok ? FAT32_OK : FAT32_ERR_IO;
}

int fat32_unlink(Fat32Entry* entry) {
    if (!s_mounted || !entry) return FAT32_ERR_INVAL;
    if (entry->attr & FAT32_ATTR_DIRECTORY) return FAT32_ERR_ISDIR;
    fat_free_chain(entry->cluster);
    return mark_deleted(entry->dir_cluster, entry->dir_index);
}

int fat32_rmdir(Fat32Entry* entry) {
    if (!s_mounted || !entry) return FAT32_ERR_INVAL;
    if (!(entry->attr & FAT32_ATTR_DIRECTORY)) return FAT32_ERR_NOTDIR;

    Fat32Entry child;
    for (uint32_t i = 0; ; i++) {
        int r = fat32_readdir(entry->cluster, i, &child);
        if (r == FAT32_ERR_NOENT) break;
        if (r != FAT32_OK) return r;
        if (kstrcmp(child.name, ".") == 0 || kstrcmp(child.name, "..") == 0) continue;
        return FAT32_ERR_EXIST;
    }

    fat_free_chain(entry->cluster);
    return mark_deleted(entry->dir_cluster, entry->dir_index);
}

int fat32_rename(Fat32Entry* entry, uint32_t new_dir_cluster, const char* new_name) {
    if (!s_mounted || !entry) return FAT32_ERR_INVAL;

    uint8_t attr = entry->attr;
    uint32_t cluster = entry->cluster;
    uint32_t size    = entry->size;

    uint32_t new_raw_idx = 0;
    int r = write_dir_entry(new_dir_cluster, new_name, attr, cluster, size, &new_raw_idx);
    if (r != FAT32_OK) return r;

    r = mark_deleted(entry->dir_cluster, entry->dir_index);
    if (r != FAT32_OK) return r;

    kstrcpy(entry->name, new_name);
    entry->dir_cluster = new_dir_cluster;
    entry->dir_index   = new_raw_idx;
    return FAT32_OK;
}