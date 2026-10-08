#include <Library/BaseLib.h>
#include <Library/PlatformMemoryMapLib.h>

/*
 * Samsung Galaxy A14 4G (SM-A145F), Exynos 850 / s5e3830.
 *
 * Derived from the device tree the stock bootloader hands to the kernel
 * (uniLoader: live-dtb/stock_live.dtb). Only RAM that is known to be free is
 * handed to UEFI; everything TrustZone or the bootloader owns is left out:
 *
 *   0x8fe00000 +1M   ramoops            0x8ffff000 +4K   kaslr seed
 *   0x90000000 +300K ect_binary         0x91200000 +2M   sec_debug_next
 *   0xbba00000-0xbfffffff               TrustZone protected (not in DTB RAM)
 *   0xbff80000 +512K el3mon            0xc3000000 +512K seclog_mem
 *   0xd0000000 +136M cp_rmem (modem)   0xdb900000 +4M    camera_uncached
 *
 * The rest of the 4 GB (above 0xdbd00000 and the 2 GB at 0x880000000) is
 * peppered with reserved nodes; extend this table once it is needed.
 */
static ARM_MEMORY_REGION_DESCRIPTOR_EX gDeviceMemoryDescriptorEx[] = {
/*                                                    EFI_RESOURCE_ EFI_RESOURCE_ATTRIBUTE_ EFI_MEMORY_TYPE ARM_REGION_ATTRIBUTE_
     MemLabel(32 Char.),  MemBase,    MemSize, BuildHob, ResourceType, ResourceAttribute, MemoryType, CacheAttributes
--------------------- Register ---------------------*/
    {"Periphs",           0x00000000, 0x20000000,  AddMem, MEM_RES, UNCACHEABLE,  RtCode,   NS_DEVICE},

//--------------------- DDR --------------------- */

    /* 0x80000000-0x80c00000: uniLoader payload entry (0x80200000), DTB, ramdisk */
    {"HLOS 0",            0x80000000, 0x00C00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},
    {"UEFI Stack",        0x80C00000, 0x00040000, AddMem, SYS_MEM, SYS_MEM_CAP,  BsData, WRITE_BACK},
    {"CPU Vectors",       0x80C40000, 0x00010000, AddMem, SYS_MEM, SYS_MEM_CAP,  BsCode, WRITE_BACK},
    /* ends at 0x8fe00000 (ramoops) */
    {"HLOS 0 Split",      0x80C50000, 0x0F1B0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},
    /* 0x8fe00000-0x91400000 reserved: ramoops, kaslr, ect_binary, sec_debug_next */
    {"HLOS 0 Split 2",    0x91400000, 0x0FC00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},
    {"UEFI FD",           0xA1000000, 0x00700000, AddMem, SYS_MEM, SYS_MEM_CAP, BsCode, WRITE_BACK},
    /* up to 0xbb9fffff, the end of the first RAM bank in the live DTB */
    {"HLOS 0 Split 3",    0xA1700000, 0x1A300000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
    {"HLOS 1",            0xC0000000, 0x03000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
    {"HLOS 1 Split",      0xC3080000, 0x0CF80000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
    {"HLOS 2",            0xD8800000, 0x03100000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
    {"HLOS 2 Split",      0xDBD00000, 0x04300000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK},
    /* bootloader framebuffer 13 MiB; 20 MiB mapped because PlatformInitialize clears 0x1400000 bytes */
    {"Display Reserved",  0xFA000000, 0x01400000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH_XN},

//------------------- Terminator for MMU ---------------------
{"Terminator", 0, 0, 0, 0, 0, 0, 0}};

ARM_MEMORY_REGION_DESCRIPTOR_EX *GetPlatformMemoryMap()
{
  return gDeviceMemoryDescriptorEx;
}
