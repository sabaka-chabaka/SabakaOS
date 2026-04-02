#include "ata.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" :: "a"(val), "Nd"(port));
}

static bool s_disk_present = false;
static uint32_t s_sector_count = 0;

static void ata_delay400ns() {
    for (int i = 0; i < 4; i++)
        inb(ATA_PRIMARY_ALT_STATUS);
}

static bool ata_wait_not_busy(uint32_t timeout_cycles = 100000) {
    for (uint32_t i = 0; i < timeout_cycles; i++) {
        uint8_t st = inb(ATA_PRIMARY_STATUS);
        if (!(st & ATA_STATUS_BSY)) return true;
    }
    return false;
}

static bool ata_wait_drq() {
    for (uint32_t i = 0; i < 100000; i++) {
        uint8_t st = inb(ATA_PRIMARY_STATUS);
        if (st & (ATA_STATUS_ERR | ATA_STATUS_DF)) return false;
        if (st & ATA_STATUS_DRQ) return true;
    }
    return false;
}

bool ata_init() {
    s_disk_present  = false;
    s_sector_count  = 0;

    outb(ATA_PRIMARY_DRIVE_HEAD, 0xA0);
    ata_delay400ns();

    if (!ata_wait_not_busy()) return false;

    outb(ATA_PRIMARY_SECTOR_CNT, 0);
    outb(ATA_PRIMARY_LBA_LO,     0);
    outb(ATA_PRIMARY_LBA_MID,    0);
    outb(ATA_PRIMARY_LBA_HI,     0);
    outb(ATA_PRIMARY_CMD, ATA_CMD_IDENTIFY);
    ata_delay400ns();

    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0x00) return false;

    if (!ata_wait_not_busy()) return false;

    if (inb(ATA_PRIMARY_LBA_MID) != 0 || inb(ATA_PRIMARY_LBA_HI) != 0)
        return false;

    if (!ata_wait_drq()) return false;

    uint16_t identify[256];
    for (int i = 0; i < 256; i++)
        identify[i] = inw(ATA_PRIMARY_DATA);

    s_sector_count = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
    if (s_sector_count == 0) return false;

    s_disk_present = true;
    return true;
}

bool ata_is_present() { return s_disk_present; }
uint32_t ata_sectors_count() { return s_sector_count; }

static bool ata_prepare(uint32_t lba, uint8_t count) {
    if (!s_disk_present) return false;
    if (!ata_wait_not_busy()) return false;

    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    ata_delay400ns();

    outb(ATA_PRIMARY_SECTOR_CNT, count);
    outb(ATA_PRIMARY_LBA_LO,  (uint8_t)(lba));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI,  (uint8_t)(lba >> 16));
    return true;
}

bool ata_read_sectors(uint32_t lba, uint8_t count, void* buf) {
    if (!ata_prepare(lba, count)) return false;
    outb(ATA_PRIMARY_CMD, ATA_CMD_READ_SECTORS);

    uint16_t* ptr = (uint16_t*)buf;
    for (uint8_t s = 0; s < count; s++) {
        if (!ata_wait_drq()) return false;
        for (int i = 0; i < 256; i++)
            ptr[s * 256 + i] = inw(ATA_PRIMARY_DATA);
        ata_delay400ns();
    }
    return true;
}

bool ata_write_sectors(uint32_t lba, uint8_t count, const void* buf) {
    if (!ata_prepare(lba, count)) return false;
    outb(ATA_PRIMARY_CMD, ATA_CMD_WRITE_SECTORS);

    const uint16_t* ptr = (const uint16_t*)buf;
    for (uint8_t s = 0; s < count; s++) {
        if (!ata_wait_drq()) return false;
        for (int i = 0; i < 256; i++)
            outw(ATA_PRIMARY_DATA, ptr[s * 256 + i]);
        ata_delay400ns();
    }

    outb(ATA_PRIMARY_CMD, ATA_CMD_FLUSH);
    ata_wait_not_busy();
    return true;
}