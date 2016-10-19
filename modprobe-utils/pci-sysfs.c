/*
 * (C) Copyright IBM Corporation 2006
 *
 * Copyright (c) 2014 NVIDIA Corporation
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * pcienum-sysfs.c
 *
 * Based on libpciaccess/src/linux_sysfs.c from libpciaccess-0.12.1, which was
 * found here:
 *
 * http://cgit.freedesktop.org/xorg/lib/libpciaccess
 *
 * Access PCI subsystem using Linux's sysfs interface.  This interface is
 * available starting somewhere in the late 2.5.x kernel phase, and is the
 * preferred method on all 2.6.x kernels.
 *
 * Original author: Ian Romanick <idr@us.ibm.com>
 */

#if defined(NV_LINUX)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>

#include "pci-enum.h"
#include "pci-sysfs.h"

#define SYS_BUS_PCI                     "/sys/bus/pci/"
#define SYS_BUS_PCI_DEVICES SYS_BUS_PCI "devices"
#define SYS_BUS_PCI_RESCAN  SYS_BUS_PCI "rescan"
#define SYSFS_PCI_BRIDGE_RESCAN_FMT     SYS_BUS_PCI_DEVICES "/%04x:%02x:%02x.%1x/rescan"
#define SYSFS_RESCAN_STRING             "1\n"
#define SYSFS_RESCAN_STRING_SIZE        2

static int pci_sysfs_read_cfg(uint16_t, uint16_t, uint16_t, uint16_t, void *,
                              uint16_t size, uint16_t *);

static int find_matches(struct pci_id_match *match);

/**
 * Attempt to access PCI subsystem using Linux's sysfs interface to enumerate
 * the matched devices.
 */
int
pci_enum_match_id(struct pci_id_match *match)
{
    int err = 0;
    struct stat st;


    /* 
     * If the directory "/sys/bus/pci/devices" exists, then the PCI subsystem
     * can be accessed using this interface.
     */
    match->num_matches = 0;
    if (stat(SYS_BUS_PCI_DEVICES, &st) == 0)
    {
        err = find_matches(match);
    }
    else
    {
        err = errno;
    }

    return err;
}


/**
 * The sysfs lookup method uses the directory entries in /sys/bus/pci/devices
 * to enumerate all PCI devices, and then uses a file in each that is mapped to
 * the device's PCI config space to extract the data to match against.
 */
static int
find_matches(struct pci_id_match *match)
{
    struct dirent *d;
    DIR *sysfs_pci_dir;
    int err = 0;

    sysfs_pci_dir = opendir(SYS_BUS_PCI_DEVICES);
    if (sysfs_pci_dir == NULL)
    {
        return errno;
    }

    while ((d = readdir(sysfs_pci_dir)) != NULL)
    {
        uint8_t config[48];
        uint16_t bytes;
        uint16_t dom, bus, dev, func;
        uint16_t vendor_id, device_id, subvendor_id, subdevice_id;
        uint16_t device_class;

        /* Ignore the . and .. dirents */
        if ((strcmp(d->d_name, ".") == 0) || (strcmp(d->d_name, "..") == 0))
        {
            continue;
        }

        sscanf(d->d_name, "%04"SCNx16":%02"SCNx16":%02"SCNx16".%1"SCNu16,
               & dom, & bus, & dev, & func);

        err = pci_sysfs_read_cfg(dom, bus, dev, func, config, 48, & bytes);
        if ((bytes == 48) && !err)
        {
            vendor_id = (uint16_t)(config[0] + (config[1] << 8));
            device_id = (uint16_t)(config[2] + (config[3] << 8));
            device_class = (uint16_t)(config[10] + (config[11] << 8));
            subvendor_id = (uint16_t)(config[44] + (config[45] << 8));
            subdevice_id = (uint16_t)(config[46] + (config[47] << 8));

            /*
             * This logic, originally in common_iterator.c, will tell if
             * this device is a match for the search criteria.
             */
            if (PCI_ID_COMPARE(match->vendor_id,    vendor_id)    &&
                PCI_ID_COMPARE(match->device_id,    device_id)    &&
                PCI_ID_COMPARE(match->subvendor_id, subvendor_id) &&
                PCI_ID_COMPARE(match->subdevice_id, subdevice_id) &&
                ((device_class & match->device_class_mask) ==
                    match->device_class))
            {
                match->num_matches++;
            }
        }

        if (err)
        {
            break;
        }
    }

    closedir(sysfs_pci_dir);
    return err;
}

static int
pci_sysfs_read_cfg(uint16_t domain, uint16_t bus, uint16_t device,
                   uint16_t function, void * data, uint16_t size,
                   uint16_t *bytes_read)
{
    char name[256];
    uint16_t temp_size = size;
    int err = 0;
    int fd;
    char *data_bytes = data;

    if (bytes_read != NULL)
    {
        *bytes_read = 0;
    }

    /*
     * Each device has a directory under sysfs.  Within that directory there
     * is a file named "config".  This file used to access the PCI config
     * space.  It is used here to obtain most of the information about the
     * device.
     */
    snprintf(name, 255, "%s/%04x:%02x:%02x.%1u/config",
             SYS_BUS_PCI_DEVICES, domain, bus, device, function);

    fd = open(name, O_RDONLY);
    if (fd < 0)
    {
        return errno;
    }

    while (temp_size > 0)
    {
        const ssize_t bytes = read(fd, data_bytes, temp_size);

        /*
         * If zero bytes were read, then we assume it's the end of the
         * config file.
         */
        if (bytes <= 0)
        {
            err = errno;
            break;
        }

        temp_size = (uint16_t)(temp_size - bytes);
        data_bytes += bytes;
    }
    
    if (bytes_read != NULL)
    {
        *bytes_read = (uint16_t)(size - temp_size);
    }

    close(fd);
    return err;
}

int
pci_rescan(uint16_t domain, uint8_t bus, uint8_t slot, uint8_t function)
{
    char const                      *node;
    char                            node_buf[256];
    int                             node_fd;
    ssize_t                         cnt;

    if ((domain | bus | slot | function) == 0)
    {
        /* rescan the entire PCI tree */
        node = SYS_BUS_PCI_RESCAN;
    }
    else
    {
        snprintf(node_buf, sizeof(node_buf) - 1, SYSFS_PCI_BRIDGE_RESCAN_FMT,
                domain, bus, slot, function);
        node = node_buf;
    }

    node_fd = open(node, O_WRONLY);

    if (node_fd < 0)
    {
        return errno;
    }

    cnt = write(node_fd, SYSFS_RESCAN_STRING, SYSFS_RESCAN_STRING_SIZE);

    close(node_fd);

    return cnt == SYSFS_RESCAN_STRING_SIZE ? 0 : EIO;
}

#endif /* defined(NV_LINUX) */
