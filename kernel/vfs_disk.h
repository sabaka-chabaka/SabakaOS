#pragma once
#include <stdint.h>

bool vfs_mount_fat32(const char* mount_point);

bool vfs_fat32_mounted();