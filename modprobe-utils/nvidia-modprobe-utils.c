/*
 * Copyright (c) 2013, NVIDIA CORPORATION.
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

#if defined(NV_LINUX)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "nvidia-modprobe-utils.h"

#define NV_PROC_MODPROBE_PATH "/proc/sys/kernel/modprobe"
#define NV_PROC_MODULES_PATH "/proc/modules"

#define NV_PROC_MODPROBE_PATH_MAX        1024
#define NV_MAX_MODULE_NAME_SIZE          16
#define NV_MAX_PROC_REGISTRY_PATH_SIZE   NV_MAX_CHARACTER_DEVICE_FILE_STRLEN

#define NV_NVIDIA_MODULE_NAME "nvidia"
#define NV_PROC_REGISTRY_PATH "/proc/driver/nvidia/params"

#define NV_NMODULE_NVIDIA_MODULE_NAME "nvidia%d"
#define NV_NMODULE_PROC_REGISTRY_PATH "/proc/driver/nvidia/%d/params"

#define NV_DEVICE_FILE_MODE_MASK (S_IRWXU|S_IRWXG|S_IRWXO)
#define NV_DEVICE_FILE_MODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#define NV_DEVICE_FILE_UID 0
#define NV_DEVICE_FILE_GID 0

#define NV_MAKE_DEVICE(x,y) ((dev_t)((x) << 8 | (y)))

#define NV_MAJOR_DEVICE_NUMBER 195


/*
 * Construct the nvidia kernel module name based on the input 
 * module instance provided.  If an error occurs, the null 
 * terminator will be written to nv_module_name[0].
 */
static __inline__ void assign_nvidia_kernel_module_name
(
    char nv_module_name[NV_MAX_MODULE_NAME_SIZE],
    int module_instance
)
{
    int ret;

    if (is_multi_module(module_instance))
    {
        ret = snprintf(nv_module_name, NV_MAX_MODULE_NAME_SIZE, 
                       NV_NMODULE_NVIDIA_MODULE_NAME, module_instance);
    }
    else
    {
        ret = snprintf(nv_module_name, NV_MAX_MODULE_NAME_SIZE, 
                       NV_NVIDIA_MODULE_NAME);
    }

    if (ret <= 0)
    {
        goto fail;
    }

    nv_module_name[NV_MAX_MODULE_NAME_SIZE - 1] = '\0';

    return;

fail:

    nv_module_name[0] = '\0';
}


/*
 * Construct the proc registry path name based on the input 
 * module instance provided.  If an error occurs, the null 
 * terminator will be written to proc_path[0].
 */
static __inline__ void assign_proc_registry_path
(
    char proc_path[NV_MAX_PROC_REGISTRY_PATH_SIZE],
    int module_instance
)
{
    int ret;

    if (is_multi_module(module_instance))
    {
        ret = snprintf(proc_path, NV_MAX_PROC_REGISTRY_PATH_SIZE, 
                       NV_NMODULE_PROC_REGISTRY_PATH, module_instance);
    }
    else
    {
        ret = snprintf(proc_path, NV_MAX_PROC_REGISTRY_PATH_SIZE, 
                       NV_PROC_REGISTRY_PATH);
    }

    if (ret <= 0)
    {
        goto fail;
    }

    proc_path[NV_MAX_PROC_REGISTRY_PATH_SIZE - 1] = '\0';

    return;

fail:

    proc_path[0] = '\0';
}


/*
 * Check whether the NVIDIA kernel module is loaded by reading
 * NV_PROC_MODULES_PATH; returns 1 if the kernel module is loaded.
 * Otherwise, it returns 0.
 */
static int is_kernel_module_loaded(int module_instance)
{
    FILE *fp;
    char module_name[NV_MAX_MODULE_NAME_SIZE];
    char nv_module_name[NV_MAX_MODULE_NAME_SIZE];
    int module_loaded = 0;

    fp = fopen(NV_PROC_MODULES_PATH, "r");

    if (fp == NULL)
    {
        return 0;
    }
    assign_nvidia_kernel_module_name(nv_module_name, module_instance);

    if (nv_module_name[0] == '\0')
    {
        return 0;
    }

    while (fscanf(fp, "%15s%*[^\n]\n", module_name) == 1)
    {
        module_name[15] = '\0';
        if (strcmp(module_name, nv_module_name) == 0)
        {
            module_loaded = 1;
            break;
        }
    }

    fclose(fp);

    return module_loaded;
}


/*
 * Attempt to load the kernel module; returns 1 if kernel module is
 * successfully loaded.  Returns 0 if the kernel module could not be
 * loaded.
 *
 * If any error is encountered and print_errors is non-0, then print the
 * error to stderr.
 */
int nvidia_modprobe(const int print_errors, int module_instance)
{
    char modprobe_path[NV_PROC_MODPROBE_PATH_MAX];
    char nv_module_name[NV_MAX_MODULE_NAME_SIZE];
    int status = 1;
    pid_t pid;
    const char *envp[] = { "PATH=/sbin", NULL };
    FILE *fp;

    modprobe_path[0] = '\0';

    /* If the kernel module is already loaded, nothing more to do: success. */

    if (is_kernel_module_loaded(module_instance))
    {
        return 1;
    }

    /* Only attempt to load the kernel module if root. */

    if (geteuid() != 0)
    {
        return 0;
    }

    /* Attempt to read the full path to the modprobe executable from /proc. */

    fp = fopen(NV_PROC_MODPROBE_PATH, "r");
    if (fp != NULL)
    {
        char *str;
        size_t n;

        n = fread(modprobe_path, 1, sizeof(modprobe_path), fp);
        if (n >= 1)
        {
            modprobe_path[n - 1] = '\0';
        }

        /*
         * If str was longer than a line, we might still have a
         * newline in modprobe_path: if so, overwrite it with the nul
         * terminator.
         */
        str = strchr(modprobe_path, '\n');
        if (str != NULL)
        {
            *str = '\0';
        }

        fclose(fp);
    }

    /* If we couldn't read it from /proc, pick a reasonable default. */

    if (modprobe_path[0] == '\0')
    {
        sprintf(modprobe_path, "/sbin/modprobe");
    }

    assign_nvidia_kernel_module_name(nv_module_name, module_instance);

    if (nv_module_name[0] == '\0')
    {
        return 0;
    }

    /* Fork and exec modprobe from the child process. */

    switch (pid = fork())
    {
        case 0:

            execle(modprobe_path, "modprobe",
                   nv_module_name, NULL, envp);

            /* If execl(3) returned, then an error has occurred. */

            if (print_errors)
            {
                fprintf(stderr,
                        "NVIDIA: failed to execute `%s`: %s.\n",
                        modprobe_path, strerror(errno));
            }
            exit(1);

        case -1:
            return 0;

        default:
            if (waitpid(pid, &status, 0) < 0)
            {
                return 0;
            }
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            {
                return 1;
            }
            else
            {
                return 0;
            }
    }

    return 1;
}


/*
 * Determine the requested device file parameters: allow users to
 * override the default UID/GID and/or mode of the NVIDIA device
 * files, or even whether device file modification should be allowed;
 * the attributes are managed globally, and can be adjusted via the
 * appropriate kernel module parameters.
 */
static void init_device_file_parameters(uid_t *uid, gid_t *gid, mode_t *mode, 
                                        int *modify, int module_instance)
{
    FILE *fp;
    char name[32];
    unsigned int value;
    char proc_path[NV_MAX_PROC_REGISTRY_PATH_SIZE];

    *mode = NV_DEVICE_FILE_MODE;
    *uid = NV_DEVICE_FILE_UID;
    *gid = NV_DEVICE_FILE_GID;
    *modify = 1;

    assign_proc_registry_path(proc_path, module_instance);

    if (proc_path[0] == '\0')
    {
        return;
    }

    fp = fopen(proc_path, "r");

    if (fp == NULL)
    {
        return;
    }

    while (fscanf(fp, "%31[^:]: %u\n", name, &value) == 2)
    {
        name[31] = '\0';
        if (strcmp(name, "DeviceFileUID") == 0)
        {
            *uid = value;
        }
        if (strcmp(name, "DeviceFileGID") == 0)
        {
            *gid = value;
        }
        if (strcmp(name, "DeviceFileMode") == 0)
        {
            *mode = value;
        }
        if (strcmp(name, "ModifyDeviceFiles") == 0)
        {
            *modify = value;
        }
    }

    fclose(fp);
}


/*
 * Attempt to create the NVIDIA device file with the specified minor
 * number.  Returns 1 if the file is successfully created; returns 0
 * if the file could not be created.
 */
int nvidia_mknod(int minor, int module_instance)
{
    dev_t dev = NV_MAKE_DEVICE(NV_MAJOR_DEVICE_NUMBER, minor);
    char path[NV_MAX_CHARACTER_DEVICE_FILE_STRLEN];
    mode_t mode;
    uid_t uid;
    gid_t gid;
    int modification_allowed;
    int ret;
    struct stat stat_buf;
    int do_mknod;

    assign_device_file_name(path, minor, module_instance);
    if (path[0] == '\0')
    {
        return 0;
    }

    init_device_file_parameters(&uid, &gid, &mode, &modification_allowed, 
                                module_instance);

    /* If device file modification is not allowed, nothing to do: success. */

    if (modification_allowed != 1)
    {
        return 1;
    }

    /*
     * If the device file already exists with correct properties,
     * nothing to do: success.
     */
    ret = stat(path, &stat_buf);

    if ((ret == 0) &&
        (S_ISCHR(stat_buf.st_mode)) &&
        (stat_buf.st_rdev == dev) &&
        ((stat_buf.st_mode & NV_DEVICE_FILE_MODE_MASK) == mode) &&
        (stat_buf.st_uid == uid) &&
        (stat_buf.st_gid == gid))
    {
        return 1;
    }

    /* If the stat(2) above failed, we need to create the device file. */

    do_mknod = 0;

    if (ret != 0)
    {
        do_mknod = 1;
    }

    /*
     * If the stat(2) above succeeded but the file is either not a
     * character device or has the wrong major/minor character device
     * number, then we need to delete it and recreate it.
     */
    if ((ret == 0) &&
        (!S_ISCHR(stat_buf.st_mode) ||
         (stat_buf.st_rdev != dev)))
    {
        ret = remove(path);
        if (ret != 0)
        {
            return 0;
        }
        do_mknod = 1;
    }

    if (do_mknod)
    {
        ret = mknod(path, S_IFCHR | mode, dev);
        if (ret != 0)
        {
            return 0;
        }
    }

    /*
     * Make sure the permissions and ownership are set correctly; if
     * we created the device above and either of the below fails, then
     * also delete the device file.
     */
    if ((chmod(path, mode) != 0) ||
        (chown(path, uid, gid) != 0))
    {
        if (do_mknod)
        {
            remove(path);
        }
        return 0;
    }

    return 1;
}

#endif /* NV_LINUX */
