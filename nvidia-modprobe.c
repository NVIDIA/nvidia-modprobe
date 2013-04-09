/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This is a small setuid utility for loading the NVIDIA kernel module
 * and creating NVIDIA device files.  Different Linux distributions
 * handle automatic module loading and device file creation in
 * different ways.  When the NVIDIA driver is packaged for a specific
 * distribution, it is recommended to use distribution-specific management
 * of module loading and device file creation rather than this utility.
 *
 * This utility makes sure, in a distribution-independent way, that
 * the kernel module is loaded and the device files created on behalf
 * of user-space NVIDIA driver components who run without sufficient
 * privileges (e.g., the CUDA driver run within the permissions of a
 * non-privileged user).
 */

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>
#include <sys/prctl.h>

#include "nvidia-modprobe-utils.h"

#include "nvgetopt.h"
#include "option-table.h"
#include "common-utils.h"

extern const char *pNV_ID;


static void print_version(void)
{
    fmtout("");
    fmtout(pNV_ID);
    fmtout("");
    fmtoutp(TAB, "Copyright (C) 2013 NVIDIA Corporation.");
    fmtout("");
}


static void print_summary(void)
{
    fmtout("");
    fmtoutp(TAB, "This setuid program is used to create, in a Linux "
            "distribution-independent way, NVIDIA Linux device "
            "files and load the NVIDIA kernel module, on behalf of NVIDIA "
            "Linux driver components which may not have sufficient "
            "privileges to perform these actions on their own.");
    fmtout("");
}


static void print_help_helper(const char *name, const char *description)
{
    fmtoutp(TAB, name);
    fmtoutp(BIGTAB, description);
    fmtout("");
}


static void print_help(void)
{
    print_version();
    print_summary();

    fmtout("");
    fmtout("nvidia-modprobe [options]");
    fmtout("");

    nvgetopt_print_help(__options, 0, print_help_helper);
}


int main(int argc, char *argv[])
{
    int minors[64];
    int num_minors = 0;
    int i, ret = 1;

    while (1)
    {
        int c, intval;

        c = nvgetopt(argc,
                     argv,
                     __options,
                     NULL, /* strval */
                     NULL, /* boolval */
                     &intval,
                     NULL, /* doubleval */
                     NULL); /* disable */

        if (c == -1) break;

        switch (c)
        {
            case 'v':
                print_version();
                exit(0);
            case 'h':
                print_help();
                exit(0);
            case 'c':
                if (num_minors < ARRAY_LEN(minors))
                {
                    minors[num_minors++] = intval;
                }
                else
                {
                    fmterr("Too many NVIDIA character device files requested.");
                    exit(1);
                }
                break;
            default:
                fmterr("Invalid commandline, please run `%s --help` "
                       "for usage information.\n", argv[0]);
                exit(1);
        }
    }

    /* Load the kernel module. */

    ret = nvidia_modprobe(0);
    if (!ret)
    {
        goto done;
    }

    /* Create any device files requested. */

    for (i = 0; i < num_minors; i++)
    {
        ret = nvidia_mknod(minors[i]);
        if (!ret)
        {
            goto done;
        }
    }

 done:

    return !ret;
}
