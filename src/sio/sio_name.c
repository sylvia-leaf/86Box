/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          The name of the chip a Super I/O device is, from the parameters
 *          it is added with: one device covers a family of chips.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/sio.h>

/* The device's name without what only says it is a Super I/O. */
static void
sio_plain_name(const device_t *dev, char *name, size_t len)
{
    static const char *suffixes[] = { " Integrated Super I/O", " Super I/O", " Combination I/O",
                                      " Multifunction Controller" };
    char              *found;

    snprintf(name, len, "%s", dev->name);
    for (size_t i = 0; i < (sizeof(suffixes) / sizeof(suffixes[0])); i++) {
        if ((found = strstr(name, suffixes[i])) != NULL) {
            memmove(found, found + strlen(suffixes[i]), strlen(found + strlen(suffixes[i])) + 1);
            break;
        }
    }
}

void
sio_get_chip_name(const device_t *dev, uintptr_t params, char *name, size_t len)
{
    name[0] = '\0';

    if (dev == NULL)
        return;

    if (dev == &fdc37c6xx_device)
        snprintf(name, len, "SMC FDC37C6%02X", (unsigned) (params & 0xff));
    else if (dev == &fdc37c93x_device) {
        switch (params & FDC37C93X_CHIP_ID) {
            case FDC37C93X_FR:
                snprintf(name, len, "SMC FDC37C932FR");
                break;
            case FDC37C93X_APM:
                snprintf(name, len, "SMC FDC37C931APM");
                break;
            default:
                /* The 932 and the 935 answer with the same device ID (02h)
                   and are the same chip here: the name cannot pick one. */
                snprintf(name, len, "SMC FDC37C932/935");
                break;
        }
    } else if (dev == &fdc37mx0x_device)
        snprintf(name, len, "SMC %s", (params & FDC37XXXX_B80X) ? "FDC37B80x" :
                                      ((params & FDC37XXXX_CHIP_ID) == FDC37M70X) ? "FDC37M70x" : "FDC37M60x");
    else if (dev == &w83977_device) {
        switch (params & W83977_TYPE) {
            case W83977TF:
                snprintf(name, len, "Winbond W83977TF");
                break;
            case W83977EF:
                snprintf(name, len, "Winbond W83977EF");
                break;
            default:
                snprintf(name, len, "Winbond W83977F");
                break;
        }
    } else if (dev == &w83877_device)
        snprintf(name, len, "Winbond %s", ((params & 0x0f00) == W83877TF) ? "W83877TF" : "W83877F");
    else if (dev == &w837x7_device) {
        switch (params & 0x0f) {
            case W83777F:
                snprintf(name, len, "Winbond W83777F");
                break;
            case W83787IF:
                snprintf(name, len, "Winbond W83787IF");
                break;
            default:
                snprintf(name, len, "Winbond W83787F");
                break;
        }
    } else if (dev == &um866x_device) {
        switch (params & 0xff00) {
            case UM82C863F:
                snprintf(name, len, "UMC UM82C863F");
                break;
            case UM8663AF:
                snprintf(name, len, "UMC UM8663AF");
                break;
            case UM8663BF:
                snprintf(name, len, "UMC UM8663BF");
                break;
            default:
                snprintf(name, len, "UMC UM82C862F");
                break;
        }
    } else if (dev == &pc873xx_device)
        snprintf(name, len, "National Semiconductor %s", (params & PC87332) ? "PC87332" : "PC87311");
    else if (dev == &pc87307_device)
        snprintf(name, len, "National Semiconductor %s", ((params & PCX730X_CHIP_ID) == PCX7307_PC97307) ? "PC97307" : "PC87307");
    else
        sio_plain_name(dev, name, len);
}
