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
#include <fcntl.h>

#include "nvidia-modprobe-utils.h"
#include "pci-enum.h"

#define NV_PROC_MODPROBE_PATH "/proc/sys/kernel/modprobe"
#define NV_PROC_MODULES_PATH "/proc/modules"
#define NV_PROC_DEVICES_PATH "/proc/devices"

#define NV_PROC_MODPROBE_PATH_MAX        1024
#define NV_MAX_MODULE_NAME_SIZE          16
#define NV_MAX_PROC_REGISTRY_PATH_SIZE   NV_MAX_CHARACTER_DEVICE_FILE_STRLEN
#define NV_MAX_LINE_LENGTH               256

#define NV_NVIDIA_MODULE_NAME "nvidia"
#define NV_PROC_REGISTRY_PATH "/proc/driver/nvidia/params"

#define NV_UVM_MODULE_NAME "nvidia-uvm"
#define NV_UVM_DEVICE_NAME "/dev/nvidia-uvm"
#define NV_UVM_TOOLS_DEVICE_NAME "/dev/nvidia-uvm-tools"

#define NV_MODESET_MODULE_NAME "nvidia-modeset"

#define NV_VGPU_VFIO_MODULE_NAME "nvidia-vgpu-vfio"

#define NV_NVLINK_MODULE_NAME "nvidia-nvlink"
#define NV_NVLINK_PROC_PERM_PATH "/proc/driver/nvidia-nvlink/permissions"

#define NV_NVSWITCH_MODULE_NAME "nvidia-nvswitch"
#define NV_NVSWITCH_PROC_PERM_PATH "/proc/driver/nvidia-nvswitch/permissions"

#define NV_DEVICE_FILE_MODE_MASK (S_IRWXU|S_IRWXG|S_IRWXO)
#define NV_DEVICE_FILE_MODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#define NV_DEVICE_FILE_UID 0
#define NV_DEVICE_FILE_GID 0

#define NV_MAKE_DEVICE(x,y) ((dev_t)((x) << 8 | (y)))

#define NV_MAJOR_DEVICE_NUMBER 195

#define NV_PCI_VENDOR_ID    0x10DE

#define NV_MIN(a, b) (((a) < (b)) ? (a) : (b))

/*
 * Just like strcmp(3), except that differences between '-' and '_' are
 * ignored. This is useful for comparing module names, where '-' and '_'
 * are supposed to be treated interchangeably.
 */
static int modcmp(const char *a, const char *b)
{
    int i;

    /* Walk both strings and compare each character */
    for (i = 0; a[i] && b[i]; i++)
    {
        if (a[i] != b[i])
        {
            /* ignore differences between '-' and '_' */
            if (((a[i] == '-') || (a[i] == '_')) &&
                ((b[i] == '-') || (b[i] == '_')))
            {
                continue;
            }

            break;
        }
    }

    /*
     * If the strings are of unequal length, only one of a[i] or b[i] == '\0'.
     * If they are the same length, both will be '\0', and the strings match.
     */
    return a[i] - b[i];
}


/*
 * Check whether the specified module is loaded by reading
 * NV_PROC_MODULES_PATH; returns 1 if the kernel module is loaded.
 * Otherwise, it returns 0.
 */
static int is_kernel_module_loaded(const char *nv_module_name)
{
    FILE *fp;
    char module_name[NV_MAX_MODULE_NAME_SIZE];
    int module_loaded = 0;

    fp = fopen(NV_PROC_MODULES_PATH, "r");

    if (fp == NULL)
    {
        return 0;
    }

    while (fscanf(fp, "%15s%*[^\n]\n", module_name) == 1)
    {
        module_name[15] = '\0';
        if (modcmp(module_name, nv_module_name) == 0)
        {
            module_loaded = 1;
            break;
        }
    }

    fclose(fp);

    return module_loaded;
}

/*
 * Attempt to redirect STDOUT and STDERR to /dev/null.
 *
 * This is only for the cosmetics of silencing warnings, so do not
 * treat any errors here as fatal.
 */
static void silence_current_process(void)
{
    int dev_null_fd = open("/dev/null", O_RDWR);
    if (dev_null_fd < 0)
    {
        return;
    }

    dup2(dev_null_fd, STDOUT_FILENO);
    dup2(dev_null_fd, STDERR_FILENO);
    close(dev_null_fd);
}

/*
 * Attempt to load a kernel module; returns 1 if kernel module is
 * successfully loaded.  Returns 0 if the kernel module could not be
 * loaded.
 *
 * If any error is encountered and print_errors is non-0, then print the
 * error to stderr.
 */
static int modprobe_helper(const int print_errors, const char *module_name)
{
    char modprobe_path[NV_PROC_MODPROBE_PATH_MAX];
    int status = 1;
    struct stat file_status;
    pid_t pid;
    const char *envp[] = { "PATH=/sbin", NULL };
    FILE *fp;

    /*
     * Use PCI_BASE_CLASS_MASK to cover both types of DISPLAY controllers that
     * NVIDIA ships (VGA = 0x300 and 3D = 0x302).
     */
    struct pci_id_match id_match = {
        NV_PCI_VENDOR_ID,       /* Vendor ID    = 0x10DE                 */
        PCI_MATCH_ANY,          /* Device ID    = any                    */
        PCI_MATCH_ANY,          /* Subvendor ID = any                    */
        PCI_MATCH_ANY,          /* Subdevice ID = any                    */
        0x0300,                 /* Device Class = PCI_BASE_CLASS_DISPLAY */
        PCI_BASE_CLASS_MASK,    /* Display Mask = base class only        */
        0                       /* Initial number of matches             */
    };

    modprobe_path[0] = '\0';

    if (module_name == NULL || module_name[0] == '\0') {
        return 0;
    }

    /* If the kernel module is already loaded, nothing more to do: success. */

    if (is_kernel_module_loaded(module_name))
    {
        return 1;
    }

    /*
     * Before attempting to load the module, look for any NVIDIA PCI devices.
     * If none exist, exit instead of attempting the modprobe, because doing so
     * would issue error messages that are really irrelevant if there are no
     * NVIDIA PCI devices present.
     *
     * If our check fails, for whatever reason, continue with the modprobe just
     * in case.
     */
    status = pci_enum_match_id(&id_match);
    if (status == 0 && id_match.num_matches == 0)
    {
        if (print_errors)
        {
            fprintf(stderr,
                    "NVIDIA: no NVIDIA devices found\n");
        }

        return 0;
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

        /*
         * Null terminate the string, but make sure 'n' is in the range
         * [0, sizeof(modprobe_path)-1].
         */
        n = NV_MIN(n, sizeof(modprobe_path) - 1);
        modprobe_path[n] = '\0';

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

    /* Do not attempt to exec(3) modprobe if it does not exist. */

    if (stat(modprobe_path, &file_status) != 0 ||
        !S_ISREG(file_status.st_mode) ||
        (file_status.st_mode & S_IXUSR) != S_IXUSR)
    {
        return 0;
    }

    /* Fork and exec modprobe from the child process. */

    switch (pid = fork())
    {
        case 0:

            /*
             * modprobe might complain in expected scenarios.  E.g.,
             * `modprobe nvidia` on a Tegra system with dGPU where no nvidia.ko is
             * present will complain:
             *
             *  "modprobe: FATAL: Module nvidia not found."
             *
             * Silence the current process to avoid such unwanted messages.
             */
            silence_current_process();

            execle(modprobe_path, "modprobe",
                   module_name, NULL, envp);

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
            /*
             * waitpid(2) is not always guaranteed to return success even if
             * the child terminated normally.  For example, if the process
             * explicitly configured the handling of the SIGCHLD signal
             * to SIG_IGN, then waitpid(2) will instead block until all
             * children terminate and return the error ECHILD, regardless
             * of the child's exit codes.
             *
             * Hence, ignore waitpid(2) error codes and instead check
             * whether the desired kernel module is loaded.
             */
            waitpid(pid, NULL, 0);

            return is_kernel_module_loaded(module_name);
    }

    return 1;
}


/*
 * Attempt to load an NVIDIA kernel module
 */
int nvidia_modprobe(const int print_errors)
{
    return modprobe_helper(print_errors, NV_NVIDIA_MODULE_NAME);
}


/*
 * Determine the requested device file parameters: allow users to
 * override the default UID/GID and/or mode of the NVIDIA device
 * files, or even whether device file modification should be allowed;
 * the attributes are managed globally, and can be adjusted via the
 * appropriate kernel module parameters.
 */
static void init_device_file_parameters(uid_t *uid, gid_t *gid, mode_t *mode,
                                        int *modify, const char *proc_path)
{
    FILE *fp;
    char name[32];
    unsigned int value;

    *mode = NV_DEVICE_FILE_MODE;
    *uid = NV_DEVICE_FILE_UID;
    *gid = NV_DEVICE_FILE_GID;
    *modify = 1;

    if (proc_path == NULL || proc_path[0] == '\0')
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
 * A helper to query device file states.
 */
static int get_file_state_helper(
    const char *path,
    int major,
    int minor,
    const char *proc_path,
    uid_t uid,
    gid_t gid,
    mode_t mode)
{
    dev_t dev = NV_MAKE_DEVICE(major, minor);
    struct stat stat_buf;
    int ret;
    int state = 0;

    ret = stat(path, &stat_buf);
    if (ret == 0)
    {
        nvidia_update_file_state(&state, NvDeviceFileStateFileExists);

        if (S_ISCHR(stat_buf.st_mode) && (stat_buf.st_rdev == dev))
        {
            nvidia_update_file_state(&state, NvDeviceFileStateChrDevOk);
        }

        if (((stat_buf.st_mode & NV_DEVICE_FILE_MODE_MASK) == mode) &&
            (stat_buf.st_uid == uid) &&
            (stat_buf.st_gid == gid))
        {
            nvidia_update_file_state(&state, NvDeviceFileStatePermissionsOk);
        }
    }

    return state;
}

int nvidia_get_file_state(int minor)
{
    char path[NV_MAX_CHARACTER_DEVICE_FILE_STRLEN];
    mode_t mode;
    uid_t uid;
    gid_t gid;
    int modification_allowed;
    int state = 0;

    assign_device_file_name(path, minor);

    init_device_file_parameters(&uid, &gid, &mode, &modification_allowed,
                                NV_PROC_REGISTRY_PATH);

    state = get_file_state_helper(path, NV_MAJOR_DEVICE_NUMBER, minor,
                                  NV_PROC_REGISTRY_PATH, uid, gid, mode);

    return state;
}

/*
 * Attempt to create the specified device file with the specified major
 * and minor number.  If proc_path is specified, scan it for custom file
 * permissions.  Returns 1 if the file is successfully created; returns 0
 * if the file could not be created.
 */
static int mknod_helper(int major, int minor, const char *path,
                        const char *proc_path)
{
    dev_t dev = NV_MAKE_DEVICE(major, minor);
    mode_t mode;
    uid_t uid;
    gid_t gid;
    int modification_allowed;
    int ret;
    int state;
    int do_mknod;

    if (path == NULL || path[0] == '\0')
    {
        return 0;
    }

    init_device_file_parameters(&uid, &gid, &mode, &modification_allowed,
                                proc_path);

    /* If device file modification is not allowed, nothing to do: success. */

    if (modification_allowed != 1)
    {
        return 1;
    }

    state = get_file_state_helper(path, major, minor,
                                  proc_path, uid, gid, mode);

    if (nvidia_test_file_state(state, NvDeviceFileStateFileExists) &&
        nvidia_test_file_state(state, NvDeviceFileStateChrDevOk) &&
        nvidia_test_file_state(state, NvDeviceFileStatePermissionsOk))
    {
        return 1;
    }

    /* If the stat(2) above failed, we need to create the device file. */

    do_mknod = 0;

    if (!nvidia_test_file_state(state, NvDeviceFileStateFileExists))
    {
        do_mknod = 1;
    }

    /*
     * If the file exists but the file is either not a character device or has
     * the wrong major/minor character device number, then we need to delete it
     * and recreate it.
     */
    if (!do_mknod &&
        !nvidia_test_file_state(state, NvDeviceFileStateChrDevOk))
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


/*
 * Attempt to create a device file with the specified minor number for
 * the specified NVIDIA module instance.
 */
int nvidia_mknod(int minor)
{
    char path[NV_MAX_CHARACTER_DEVICE_FILE_STRLEN];

    assign_device_file_name(path, minor);

    return mknod_helper(NV_MAJOR_DEVICE_NUMBER, minor, path, NV_PROC_REGISTRY_PATH);
}


/*
 * Scan NV_PROC_DEVICES_PATH to find the major number of the character
 * device with the specified name.  Returns the major number on success,
 * or -1 on failure.
 */
static int get_chardev_major(const char *name)
{
    int ret = -1;
    char line[NV_MAX_LINE_LENGTH];
    FILE *fp;

    line[NV_MAX_LINE_LENGTH - 1] = '\0';

    fp = fopen(NV_PROC_DEVICES_PATH, "r");
    if (!fp)
    {
        goto done;
    }

    /* Find the beginning of the 'Character devices:' section */

    while (fgets(line, NV_MAX_LINE_LENGTH - 1, fp))
    {
        if (strcmp(line, "Character devices:\n") == 0)
        {
            break;
        }
    }

    if (ferror(fp)) {
        goto done;
    }

    /* Search for the given module name */

    while (fgets(line, NV_MAX_LINE_LENGTH - 1, fp))
    {
        char *found;

        if (strcmp(line, "\n") == 0 )
        {
            /* we've reached the end of the 'Character devices:' section */
            break;
        }

        found = strstr(line, name);

        /* Check for a newline to avoid partial matches */

        if (found && found[strlen(name)] == '\n')
        {
            int major;

            /* Read the device major number */

            if (sscanf(line, " %d %*s", &major) == 1)
            {
                ret = major;
            }

            break;
        }
    }

done:

    if (fp)
    {
        fclose(fp);
    }

    return ret;
}


/*
 * Attempt to create the NVIDIA Unified Memory device file
 */
int nvidia_uvm_mknod(int base_minor)
{
    int major = get_chardev_major(NV_UVM_MODULE_NAME);

    if (major < 0)
    {
        return 0;
    }

    return mknod_helper(major, base_minor, NV_UVM_DEVICE_NAME, NULL) &&
           mknod_helper(major, base_minor + 1, NV_UVM_TOOLS_DEVICE_NAME, NULL);
}


/*
 * Attempt to load the NVIDIA Unified Memory kernel module
 */
int nvidia_uvm_modprobe(void)
{
    return modprobe_helper(0, NV_UVM_MODULE_NAME);
}


/*
 * Attempt to load the NVIDIA modeset driver.
 */
int nvidia_modeset_modprobe(void)
{
    return modprobe_helper(0, NV_MODESET_MODULE_NAME);
}


/*
 * Attempt to create the NVIDIA modeset driver device file.
 */
int nvidia_modeset_mknod(void)
{
    return mknod_helper(NV_MAJOR_DEVICE_NUMBER,
                        NV_MODESET_MINOR_DEVICE_NUM,
                        NV_MODESET_DEVICE_NAME, NV_PROC_REGISTRY_PATH);
}

/*
 * Attempt to create the NVIDIA NVLink driver device file.
 */
int nvidia_nvlink_mknod(void)
{
    int major = get_chardev_major(NV_NVLINK_MODULE_NAME);

    if (major < 0)
    {
        return 0;
    }

    return mknod_helper(major,
                        0,
                        NV_NVLINK_DEVICE_NAME,
                        NV_NVLINK_PROC_PERM_PATH);
}

/*
 * Attempt to create the NVIDIA NVSwitch driver device files.
 */
int nvidia_nvswitch_mknod(int minor)
{
    int major = 0;
    char name[NV_MAX_CHARACTER_DEVICE_FILE_STRLEN];
    int ret;

    major = get_chardev_major(NV_NVSWITCH_MODULE_NAME);

    if (major < 0)
    {
        return 0;
    }

    if (minor == NV_NVSWITCH_CTL_MINOR)
    {
        ret = snprintf(name, NV_MAX_CHARACTER_DEVICE_FILE_STRLEN,
                       NV_NVSWITCH_CTL_NAME);
    }
    else
    {
        ret = snprintf(name, NV_MAX_CHARACTER_DEVICE_FILE_STRLEN,
                       NV_NVSWITCH_DEVICE_NAME, minor);
    }

    if (ret <= 0)
    {
        return 0;
    }

    return mknod_helper(major, minor, name, NV_NVSWITCH_PROC_PERM_PATH);
}

int nvidia_vgpu_vfio_mknod(int minor_num)
{
    int major = get_chardev_major(NV_VGPU_VFIO_MODULE_NAME);
    char vgpu_dev_name[NV_MAX_CHARACTER_DEVICE_FILE_STRLEN];
    int ret;

    if (major < 0)
    {
        return 0;
    }

    ret = snprintf(vgpu_dev_name, NV_MAX_CHARACTER_DEVICE_FILE_STRLEN,
                   NV_VGPU_VFIO_DEVICE_NAME, minor_num);
    if (ret <= 0)
    {
        return 0;
    }

    vgpu_dev_name[NV_MAX_CHARACTER_DEVICE_FILE_STRLEN - 1] = '\0';

    return mknod_helper(major, minor_num, vgpu_dev_name, NV_PROC_REGISTRY_PATH);
}

#endif /* NV_LINUX */
