/*
 * Copyright (c) 2013-2023, NVIDIA CORPORATION.
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
 * This file provides utility functions on Linux for loading the
 * NVIDIA kernel module and creating NVIDIA device files.
 */

#ifndef __NVIDIA_MODPROBE_UTILS_H__
#define __NVIDIA_MODPROBE_UTILS_H__

#include <stdio.h>

#define NV_MAX_CHARACTER_DEVICE_FILE_STRLEN  128
#define NV_CTL_DEVICE_NUM                    255
#define NV_MODESET_MINOR_DEVICE_NUM          254
#define NV_NVSWITCH_CTL_MINOR                255

#define NV_DEVICE_FILE_PATH "/dev/nvidia%d"
#define NV_CTRL_DEVICE_FILE_PATH "/dev/nvidiactl"
#define NV_MODESET_DEVICE_NAME "/dev/nvidia-modeset"
#define NV_VGPU_VFIO_DEVICE_NAME "/dev/nvidia-vgpu%d"
#define NV_NVLINK_DEVICE_NAME "/dev/nvidia-nvlink"
#define NV_NVSWITCH_CTL_NAME "/dev/nvidia-nvswitchctl"
#define NV_NVSWITCH_DEVICE_NAME "/dev/nvidia-nvswitch%d"

#define NV_CAPS_MODULE_NAME "nvidia-caps"
#define NV_CAP_DEVICE_NAME "/dev/" NV_CAPS_MODULE_NAME "/nvidia-cap%d"

#define NV_CAPS_IMEX_CHANNELS_MODULE_NAME "nvidia-caps-imex-channels"
#define NV_CAPS_IMEX_CHANNEL_DEVICE_NAME \
        "/dev/" NV_CAPS_IMEX_CHANNELS_MODULE_NAME "/channel%d"

#define NV_CHAR_DEVICE_NAME "/dev/char/%d:%d"

#if defined(NV_LINUX)

typedef enum
{
    NvDeviceFileStateFileExists = 0,
    NvDeviceFileStateChrDevOk,
    NvDeviceFileStatePermissionsOk
} NvDeviceFileState;

static __inline__ void nvidia_update_file_state(int *state,
                                                NvDeviceFileState value)
{
    *state |= (1 << value);
}

static __inline__ int nvidia_test_file_state(int state,
                                             NvDeviceFileState value)
{
    return !!(state & (1 << value));
}

int nvidia_get_file_state(int minor);
int nvidia_modprobe(const int print_errors);
int nvidia_mknod(int minor);
int nvidia_uvm_modprobe(void);
int nvidia_uvm_mknod(int base_minor);
int nvidia_modeset_modprobe(void);
int nvidia_modeset_mknod(void);
int nvidia_vgpu_vfio_mknod(int minor_num);
int nvidia_nvlink_mknod(void);
int nvidia_nvlink_get_file_state(void);
int nvidia_nvswitch_mknod(int minor);
int nvidia_nvswitch_get_file_state(int minor);
int nvidia_cap_mknod(const char* cap_file_path, int *minor);
int nvidia_cap_get_file_state(const char* cap_file_path);
int nvidia_cap_imex_channel_mknod(int minor);
int nvidia_cap_imex_channel_file_state(int minor);
int nvidia_get_chardev_major(const char *name);
int nvidia_msr_modprobe(void);
int nvidia_enable_auto_online_movable(const int print_errors);

#endif /* NV_LINUX */

/*
 * Construct the device file name, based on 'minor'.  If an error
 * occurs, the nul terminator will be written to name[0].
 */
static __inline__ void assign_device_file_name
(
    char name[NV_MAX_CHARACTER_DEVICE_FILE_STRLEN],
    int minor
)
{
    int ret;

    if ((minor < 0) || (minor > NV_CTL_DEVICE_NUM))
    {
        goto fail;
    }

    if (minor == NV_CTL_DEVICE_NUM)
    {
        ret = snprintf(name,
                       NV_MAX_CHARACTER_DEVICE_FILE_STRLEN,
                       NV_CTRL_DEVICE_FILE_PATH);
    }
    else
    {
        ret = snprintf(name,
                       NV_MAX_CHARACTER_DEVICE_FILE_STRLEN,
                       NV_DEVICE_FILE_PATH, minor);
    }

    if (ret <= 0)
    {
        goto fail;
    }

    name[NV_MAX_CHARACTER_DEVICE_FILE_STRLEN - 1] = '\0';

    return;

fail:

    name[0] = '\0';
}

#endif /* __NVIDIA_MODPROBE_UTILS_H__ */
