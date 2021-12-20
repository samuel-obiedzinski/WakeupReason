#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

#define REG_READ32(addr,result)  ((result) = *(volatile unsigned int *)(addr))
#define REG_WRITE32(addr,result) (*(volatile unsigned int *)(addr) = (result))

typedef enum
{
    WAKEUP_CEC = 0x1,
    WAKEUP_IRR = 0x2,
    WAKEUP_KPD = 0x4,
    WAKEUP_TIMER = 0x8,
    WAKEUP_NMI = 0x10,
    WAKEUP_GPIO = 0x20,
    WAKEUP_WOL = 0x40,
    WAKEUP_SDS0 = 0x80,
    WAKEUP_XPT = 0x1000,
} WAKEUP_E;

int main(int argc, char *argv[])
{
    if (argc > 1) {
        printf("WakeupReason v0.2\n");
        printf("\tUsage: %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    off_t offset = 0x10408000;     // PM registers address
    size_t len = 0x800;

    // Truncate offset to a multiple of the page size, or mmap will fail.
    size_t pagesize = sysconf(_SC_PAGE_SIZE);
    off_t page_base = (offset / pagesize) * pagesize;
    off_t page_offset = offset - page_base;

    int fd = open("/dev/mem", O_SYNC);
    if (fd < 0) {
        printf("open %d %s\n", errno, strerror(errno));
        return -1;
    }

    uint8_t *pWakeupReasonVirtualAddres = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, offset);
    if (pWakeupReasonVirtualAddres == MAP_FAILED) {
        printf("mmap %d %s\n", errno, strerror(errno));
        close(fd);
        return -1;
    }

    uint32_t enWakeUpModeMask = 0;
    uint32_t enWakeUpMode = 0;

    REG_READ32(pWakeupReasonVirtualAddres + 0x440, enWakeUpMode);
    REG_READ32(pWakeupReasonVirtualAddres + 0x44c, enWakeUpModeMask);

    const char *name = "unknown (master power)";

    if (WAKEUP_TIMER & enWakeUpMode) {
        name = "time";
    } else {
        uint32_t mode = enWakeUpMode & (~enWakeUpModeMask);

        if (WAKEUP_IRR & mode)
            name = "ir";
        else if (WAKEUP_CEC & mode)
            name = "kpd";
        else if (WAKEUP_KPD & mode)
            name = "kpd";
        else if (WAKEUP_GPIO & mode)
            name = "gpio";
        else if (WAKEUP_WOL & mode)
            name = "wol";
        else if (WAKEUP_NMI & mode)
            name = "nmi";
        else if (WAKEUP_SDS0 & mode)
            name = "sds0";
        else if (WAKEUP_XPT & mode)
            name = "xpt";
    }

    printf("Wakeup reason: %s (0x%x & 0x%x)\n", name, enWakeUpMode, enWakeUpModeMask);

    munmap(pWakeupReasonVirtualAddres, len);

    close(fd);

    return 0;
}
