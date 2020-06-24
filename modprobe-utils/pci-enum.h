/*
 * (C) Copyright IBM Corporation 2006
 *
 * Copyright (c) 2007 Paulo R. Zanoni, Tiago Vignatti
 *
 * Copyright 2009 Red Hat, Inc.
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
 * pci-enum.h
 *
 * Based on libpciaccess/include/pciaccess.h from libpciaccess-0.12.1, which
 * can be found here:
 *
 * http://cgit.freedesktop.org/xorg/lib/libpciaccess
 *
 * Original authors: Ian Romanick <idr@us.ibm.com>, Paulo R. Zanoni,
 *                   Tiago Vignatti
 */

#ifndef PCI_ENUM_H
#define PCI_ENUM_H

#include <inttypes.h>

struct pci_id_match;

#ifdef __cplusplus
extern "C" {
#endif

int pci_enum_match_id(struct pci_id_match *);

#ifdef __cplusplus
}
#endif

#define PCI_MATCH_ANY  (~0U)

#define PCI_BASE_CLASS_MASK 0xff00
#define PCI_SUB_CLASS_MASK  0x00ff
#define PCI_FULL_CLASS_MASK PCI_BASE_CLASS_MASK | PCI_SUB_CLASS_MASK

/**
 * Compare two PCI ID values (either vendor or device).  This is used
 * internally to compare the fields of pci_id_match to the fields of
 * pci_device.
 */
#define PCI_ID_COMPARE(a, b) \
    (((a) == PCI_MATCH_ANY) || ((a) == (b)))

/**
 */
struct pci_id_match {
    /**
     * Device/vendor matching controls
     *
     * Control the search based on the device, vendor, subdevice, or subvendor
     * IDs.  Setting any of these fields to PCI_MATCH_ANY will cause the field
     * to not be used in the comparison.
     */
    /*@{*/
    uint32_t    vendor_id;
    uint32_t    device_id;
    uint32_t    subvendor_id;
    uint32_t    subdevice_id;
    /*@}*/


    /**
     * Device class matching controls
     *
     * Device's class and subclass. The class is at bits [15:8], subclass is at
     * bits [7:0].
     */
    /*@{*/
    uint16_t    device_class;
    uint16_t    device_class_mask;
    /*@}*/

    /**
     * Match results
     *
     * Specifies the number of devices found that match this criteria.
     */
    /*@{*/
    uint16_t    num_matches;
};

#endif /* PCI_ENUM_H */
