/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This file provides utility functions on Linux for interfacing
 * with the sysfs/PCI kernel facility.
 */

#ifndef __PCI_SYSFS_H__
#define __PCI_SYSFS_H__

#if defined(NV_LINUX)

#include <stdint.h>
#include <linux/pci.h>

#if !defined(PCI_STD_HEADER_SIZEOF)
#define PCI_STD_HEADER_SIZEOF   64
#endif
#if !defined(PCI_CAP_ID_EXP)
#define  PCI_CAP_ID_EXP         0x10    /* PCI Express */
#endif
#if !defined(PCI_EXP_LNKCAP)
#define PCI_EXP_LNKCAP          12          /* Link Capabilities */
#endif
#if !defined(PCI_EXP_LNKCAP_DLLLARC)
#define PCI_EXP_LNKCAP_DLLLARC  0x00100000  /* Data Link Layer Link Active Reporting Capable */
#endif
#if !defined(PCI_EXP_LNKCTL)
#define PCI_EXP_LNKCTL          16      /* Link Control */
#endif
#if !defined(PCI_EXP_LNKCTL_LD)
#define  PCI_EXP_LNKCTL_LD      0x0010  /* Link Disable */
#endif
#if !defined(PCI_EXP_LNKSTA)
#define PCI_EXP_LNKSTA          18      /* Link Status */
#endif
#if !defined(PCI_EXP_LNKSTA_DLLLA)
#define  PCI_EXP_LNKSTA_DLLLA   0x2000  /* Data Link Layer Link Active */
#endif

#define PCI_LINK_WAIT_US                 200000      /* 200 ms, must be less than 1000000 (1s) */
#define PCI_LINK_DELAY_NS                100000000   /* 100 ms */
#define PCI_LINK_DLLLAR_DISABLE_DELAY_NS 30000000    /* 30ms */

#if (_POSIX_C_SOURCE >= 199309L)
#define PCI_NANOSLEEP(ts, rem)  nanosleep(ts, rem)
#elif !(_POSIX_C_SOURCE >= 200809L)
#define PCI_NANOSLEEP(ts, rem)  usleep((ts)->tv_sec * 1000000 + ((ts)->tv_nsec + 999) / 1000)
#else
#define PCI_NANOSLEEP(ts, rem)  sleep((ts)->tv_sec + ((ts)->tv_nsec + 999999999) / 1000000000)
#endif

typedef struct  {
    unsigned    domain;
    unsigned    bus;
    unsigned    dev;
    unsigned    ftn;
}   pci_info_t;

int pci_rescan(uint32_t domain, uint8_t bus, uint8_t slot, uint8_t function);
int pci_find_parent_bridge(pci_info_t *p_gpu_info, pci_info_t *p_bridge_info);
int pci_bridge_link_set_enable(uint32_t domain, uint8_t bus, uint8_t device, uint8_t ftn, int enable);

#endif /* NV_LINUX */

#endif /* __PCI_SYSFS_H__ */
