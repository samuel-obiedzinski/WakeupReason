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
    WAKEUP_IR = 0,
    WAKEUP_KEY,
    WAKEUP_TIME,
    WAKEUP_ETH,
    WAKEUP_USB,
    WAKEUP_GPIO,
    WAKEUP_CEC,
    WAKEUP_BUTT
} WAKEUP_E;

int main(int argc, char *argv[])
{
    if (argc > 1 && 0 != strcmp("clear", argv[1])) {
        printf("WakeupReason v0.2\n");
        printf("\tUsage: %s [clear]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (0 != access("/dev/hi_pm", R_OK|W_OK)) {
        perror("Platform not supported!\n");
        exit(EXIT_FAILURE);
    }

    off_t offset = 0xf8407ff0;     // PM registers address
    size_t len = 0x10;

    // Truncate offset to a multiple of the page size, or mmap will fail.
    size_t pagesize = sysconf(_SC_PAGE_SIZE);
    off_t page_base = (offset / pagesize) * pagesize;
    off_t page_offset = offset - page_base;

    int fd = open("/dev/mem", O_RDWR);
    if (fd < 0) {
        printf("open %d %s\n", errno, strerror(errno));
        return -1;
    }

    uint32_t *pWakeupReasonVirtualAddres = mmap(NULL, page_offset + len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, page_base);
    if (pWakeupReasonVirtualAddres == MAP_FAILED) {
        printf("mmap %d %s\n", errno, strerror(errno));
        close(fd);
        return -1;
    }

    uint32_t enWakeUpMode = 0;
    uint64_t powerIRKeyCode = 0;

    REG_READ32(pWakeupReasonVirtualAddres + 2 + page_offset / 4, enWakeUpMode);

    const char *name = "unknown";
    switch(enWakeUpMode) {
    case WAKEUP_IR:
    {
        uint32_t keyHigh = 0;
        uint32_t keyLow = 0;

        /* get power key */
        REG_READ32(pWakeupReasonVirtualAddres + page_offset / 4, keyLow);
        REG_READ32(pWakeupReasonVirtualAddres + 1 + page_offset / 4, keyHigh);

        powerIRKeyCode = keyHigh;
        powerIRKeyCode = (keyLow | (powerIRKeyCode << 32));

        name = "ir";
        break;
    }
    case WAKEUP_KEY:
        name = "key";
        break;
    case WAKEUP_TIME:
        name = "time";
        break;
    case WAKEUP_ETH:
        name = "eth";
        break;
    case WAKEUP_USB:
        name = "usb";
        break;
    case WAKEUP_GPIO:
        name = "gpio";
        break;
    case WAKEUP_CEC:
        name = "cec";
        break;
    case WAKEUP_BUTT:
    default:
        name = "unknown (master power)";
        break;
    }

    printf("Wakeup reason: %s (0x%x)\n", name, enWakeUpMode);
    if (enWakeUpMode == WAKEUP_IR)
        printf("Power key: 0x%llx\n", powerIRKeyCode);

    if (argc > 1) {
        // reset wake up reason, 0xFFFFFFFF => unknow => (master power)
        REG_WRITE32(pWakeupReasonVirtualAddres + page_offset / 4, 0xFFFFFFFF);
        REG_WRITE32(pWakeupReasonVirtualAddres + 1 + page_offset / 4, 0xFFFFFFFF);
        REG_WRITE32(pWakeupReasonVirtualAddres + 2 + page_offset / 4, 0xFFFFFFFF);
    }

    munmap(pWakeupReasonVirtualAddres, page_offset + len);

    close(fd);

    return 0;
}
