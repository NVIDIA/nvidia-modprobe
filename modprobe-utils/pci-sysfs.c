/*
 * (C) Copyright IBM Corporation 2006
 *
 * Copyright (c) 2014-2018 NVIDIA Corporation
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
#include <sys/time.h>
#include <time.h>
#include <limits.h>

#include "pci-enum.h"
#include "pci-sysfs.h"

#define SYS_BUS_PCI                     "/sys/bus/pci/"
#define SYS_BUS_PCI_DEVICES SYS_BUS_PCI "devices"
#define SYS_BUS_PCI_RESCAN  SYS_BUS_PCI "rescan"
#define PCI_DBDF_FORMAT                 "%04x:%02x:%02x.%1u"
#define SYSFS_PCI_BRIDGE_RESCAN_FMT     SYS_BUS_PCI_DEVICES "/" PCI_DBDF_FORMAT "/rescan"
#define SYSFS_RESCAN_STRING             "1\n"
#define SYSFS_RESCAN_STRING_SIZE        2
#define PCI_CAP_TTL_MAX                 20
#define SYSFS_PATH_SIZE                 256

#define BAIL_ON_IO_ERR(buf, err, cnt, action)   \
do  {                                           \
    if (((err) != 0) || ((cnt) < sizeof(buf)))  \
    {                                           \
        (err) = ((err) == 0) ? EIO : (err);     \
        action;                                 \
    }                                           \
} while (0)

static int pci_sysfs_read_cfg(uint32_t, uint16_t, uint16_t, uint16_t, uint16_t, void *,
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
        unsigned dom, bus, dev, func;
        uint16_t vendor_id, device_id, subvendor_id, subdevice_id;
        uint16_t device_class;

        /* Ignore the . and .. dirents */
        if ((strcmp(d->d_name, ".") == 0) || (strcmp(d->d_name, "..") == 0))
        {
            continue;
        }

        sscanf(d->d_name, PCI_DBDF_FORMAT,
               & dom, & bus, & dev, & func);

        err = pci_sysfs_read_cfg(dom, bus, dev, func, 0, config, 48, & bytes);
        if ((bytes == 48) && !err)
        {
            vendor_id = (uint16_t)config[0] + ((uint16_t)config[1] << 8);
            device_id = (uint16_t)config[2] + ((uint16_t)config[3] << 8);
            device_class = (uint16_t)config[10] +
                ((uint16_t)config[11] << 8);
            subvendor_id = (uint16_t)config[44] +
                ((uint16_t)config[45] << 8);
            subdevice_id = (uint16_t)config[46] +
                ((uint16_t)config[47] << 8);

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
pci_sysfs_read_cfg(uint32_t domain, uint16_t bus, uint16_t device,
                   uint16_t function, uint16_t off, void *data,
                   uint16_t size, uint16_t *bytes_read)
{
    char name[SYSFS_PATH_SIZE];
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
    snprintf(name, SYSFS_PATH_SIZE - 1, "%s/" PCI_DBDF_FORMAT "/config",
             SYS_BUS_PCI_DEVICES, domain, bus, device, function);

    fd = open(name, O_RDONLY);
    if (fd < 0)
    {
        return errno;
    }

    if (off != 0)
    {
        if (lseek(fd, (off_t) off, SEEK_SET) < 0)
        {
            close(fd);
            return errno;
        }
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

        temp_size -= bytes;
        data_bytes += bytes;
    }

    if (bytes_read != NULL)
    {
        *bytes_read = size - temp_size;
    }

    close(fd);
    return err;
}

static int
pci_sysfs_write_cfg(uint32_t domain, uint16_t bus, uint16_t device,
                   uint16_t function, uint16_t off, void *data,
                   uint16_t size, uint16_t *bytes_written)
{
    char name[SYSFS_PATH_SIZE];
    uint16_t temp_size = size;
    int err = 0;
    int fd;
    char *data_bytes = data;

    if (bytes_written != NULL)
    {
        *bytes_written = 0;
    }

    /*
     * Each device has a directory under sysfs.  Within that directory there
     * is a file named "config".  This file used to access the PCI config
     * space.
     */
    snprintf(name, SYSFS_PATH_SIZE - 1, "%s/" PCI_DBDF_FORMAT "/config",
             SYS_BUS_PCI_DEVICES, domain, bus, device, function);

    fd = open(name, O_WRONLY);
    if (fd < 0)
    {
        return errno;
    }

    if (off != 0)
    {
        if (lseek(fd, (off_t) off, SEEK_SET) < 0)
        {
            close(fd);
            return errno;
        }
    }

    while (temp_size > 0)
    {
        const ssize_t bytes = write(fd, data_bytes, temp_size);

        if (bytes < 0)
        {
            err = errno;
            break;
        }
        /*
         * If zero bytes were written, then we assume it's the end of the
         * config file.
         */
        if (bytes == 0)
        {
            break;
        }

        temp_size -= bytes;
        data_bytes += bytes;
    }

    if (bytes_written != NULL)
    {
        *bytes_written = size - temp_size;
    }

    close(fd);
    return err;
}

int
pci_rescan(uint32_t domain, uint8_t bus, uint8_t slot, uint8_t function)
{
    char const                      *node;
    char                            node_buf[SYSFS_PATH_SIZE];
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

int
pci_find_parent_bridge(pci_info_t *p_gpu_info, pci_info_t *p_bridge_info)
{
    char    gpu_path[SYSFS_PATH_SIZE];
    char    bridge_path[PATH_MAX];
    char    *p_node;

    snprintf(gpu_path, SYSFS_PATH_SIZE - 1, "%s/" PCI_DBDF_FORMAT "/..", SYS_BUS_PCI_DEVICES,
            p_gpu_info->domain, p_gpu_info->bus,
            p_gpu_info->dev, p_gpu_info->ftn);

    if (realpath(gpu_path, bridge_path) == NULL)
    {
        return errno;
    }

    p_node = strrchr(bridge_path, '/');

    if (p_node == NULL)
    {
        return ENOENT;
    }

    ++p_node;

    if (sscanf(p_node, PCI_DBDF_FORMAT,
                &p_bridge_info->domain, &p_bridge_info->bus,
                &p_bridge_info->dev, &p_bridge_info->ftn) != 4)
    {
        return ENOENT;
    }

    return 0;
}

static int
pci_find_pcie_caps(uint32_t domain, uint8_t bus, uint8_t device, uint8_t ftn, uint8_t *p_caps)
{
    unsigned    ttl;
    uint8_t     off;
    uint8_t     cap_id;
    int         err = ENXIO;
    uint16_t    cnt;

    for (off = PCI_CAPABILITY_LIST, ttl = PCI_CAP_TTL_MAX; ttl; --ttl)
    {
        err = pci_sysfs_read_cfg(domain, bus, device, ftn, off,
                                &off, sizeof(off), &cnt);
        BAIL_ON_IO_ERR(off, err, cnt, break);

        /* Capabilities must reside above the std config header */
        if ((off < PCI_STD_HEADER_SIZEOF) || (off == 0xff))
        {
            break;
        }

        /* Clear the reserved bits */
        off &= ~3;

        err = pci_sysfs_read_cfg(domain, bus, device, ftn, off + PCI_CAP_LIST_ID,
                                &cap_id, sizeof(cap_id), &cnt);
        BAIL_ON_IO_ERR(cap_id, err, cnt, break);

        if (cap_id == PCI_CAP_ID_EXP)
        {
            goto found;
        }

        if (cap_id == 0xff)
        {
            break;
        }

        off += PCI_CAP_LIST_NEXT;
    }
    return err;
found:
    *p_caps = off;
    return 0;
}

int
pci_bridge_link_set_enable(uint32_t domain, uint8_t bus, uint8_t device, uint8_t ftn, int enable)
{
    uint8_t         pcie_caps = 0;
    uint16_t        reg;
    uint32_t        cap_reg;
    uint16_t        cnt;
    int             err;
    struct timeval  start;
    struct timeval  curr;
    struct timeval  diff;
    struct timespec delay = {0, PCI_LINK_DELAY_NS};
    struct timespec dlllar_disable_delay = {0, PCI_LINK_DLLLAR_DISABLE_DELAY_NS};

    err = pci_find_pcie_caps(domain, bus, device, ftn, &pcie_caps);

    if (err != 0)
    {
        return err;
    }

    err = pci_sysfs_read_cfg(domain, bus, device, ftn, pcie_caps + PCI_EXP_LNKCTL,
                            &reg, sizeof(reg), &cnt);
    BAIL_ON_IO_ERR(reg, err, cnt, return err);

    if (enable)
    {
        reg &= ~PCI_EXP_LNKCTL_LD;
    }
    else
    {
        reg |= PCI_EXP_LNKCTL_LD;
    }

    err = pci_sysfs_write_cfg(domain, bus, device, ftn, pcie_caps + PCI_EXP_LNKCTL,
                            &reg, sizeof(reg), &cnt);
    BAIL_ON_IO_ERR(reg, err, cnt, return err);

    if (enable)
    {
        /*
         * Data Link Layer Link Active Reporting must be capable for
         * zero power capable downstream port. But old controller might
         * not implement it. In this case, we wait for 30 ms.
         */
        err = pci_sysfs_read_cfg(domain, bus, device, ftn, pcie_caps + PCI_EXP_LNKCAP,
                                &cap_reg, sizeof(cap_reg), &cnt);
        BAIL_ON_IO_ERR(cap_reg, err, cnt, return err);

        if (cap_reg & PCI_EXP_LNKCAP_DLLLARC)
        {
            /* wait for the link to go up and then sleep for 100 ms */

            gettimeofday(&start, NULL);

            for (;;)
            {
                err = pci_sysfs_read_cfg(domain, bus, device, ftn, pcie_caps + PCI_EXP_LNKSTA,
                                    &reg, sizeof(reg), &cnt);
                BAIL_ON_IO_ERR(reg, err, cnt, return err);

                if ((reg & PCI_EXP_LNKSTA_DLLLA) != 0)
                {
                    break;
                }

                gettimeofday(&curr, NULL);
                timersub(&curr, &start, &diff);

                if ((diff.tv_sec > 0) || (diff.tv_usec >= PCI_LINK_WAIT_US))
                {
                    return ETIME;
                }
            }
        }
        else
        {
            /*
             * Measured the time on DGX1 for link to become established in a bridge,
             * where the DLLLA reporting is supported and its approximately ~9ms,
             * so wait for 30ms where DLLLA reporting is not supported.
             */
            PCI_NANOSLEEP(&dlllar_disable_delay, NULL);
        }

        PCI_NANOSLEEP(&delay, NULL);
    }

    return err;
}

#endif /* defined(NV_LINUX) */
