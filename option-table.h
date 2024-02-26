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

#include "nvgetopt.h"

static const NVGetoptOption __options[] = {

    { "version",
      'v',
      NVGETOPT_HELP_ALWAYS,
      NULL,
      "Print the utility version and exit." },

    { "help",
      'h',
      NVGETOPT_HELP_ALWAYS,
      NULL,
      "Print usage information for the command line options and exit." },

    { "create-nvidia-device-file",
      'c',
      NVGETOPT_INTEGER_ARGUMENT | NVGETOPT_HELP_ALWAYS,
      "MINOR-NUMBER",
      "Create the NVIDIA device file with the given minor number; "
      "this option can be specified multiple times to create multiple "
      "NVIDIA device files." },

    { "unified-memory",
      'u',
       0,
       NULL,
       "Load the NVIDIA Unified Memory kernel module or create device files "
       "for it, instead of the NVIDIA kernel module." },

    { "modeset",
      'm',
       0,
       NULL,
      "Load the NVIDIA modeset kernel module and create its device file." },

    { "nvswitch",
      's',
       0,
       NULL,
      "Load the NVIDIA kernel module and create the NVSwitch device files "
      "for each minor number specified using the -c flag."},

    { "nvlink",
      'l',
      0,
      NULL,
      "Load the NVIDIA kernel module and create the NVLink control device file." },

    { "nvidia-capability-device-file",
      'f',
       NVGETOPT_STRING_ARGUMENT,
       NULL,
      "Create the NVIDIA capability device files with the given capability "
      "/proc file path. This option can be specified multiple times to create "
      "multiple NVIDIA capability device files." },

    { "nvidia-imex-channel-device-file",
      'i',
       NVGETOPT_STRING_ARGUMENT,
       NULL,
      "Create the NVIDIA IMEX channel device files for the given minor number "
      "range format <start:number of minors>" },

    { "auto-online-movable",
      'a',
       0,
       NULL,
       "Enable the online_movable auto onlining kernel setting for "
       "platforms (like Grace Hopper) that add and online GPU memory "
       "to the kernel" },

    { NULL, 0, 0, NULL, NULL },
};
