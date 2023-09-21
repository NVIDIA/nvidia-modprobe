dnl This file is to be preprocessed by m4.
changequote([[[, ]]])dnl
define(__OPTIONS__, [[[include([[[options.1.inc]]])dnl]]])dnl
dnl Solaris man chokes on three-letter macros.
ifelse(__BUILD_OS__,SunOS,[[[define(__URL__,UR)]]],[[[define(__URL__,URL)]]])dnl
.\" Copyright (C) 2005-2010 NVIDIA Corporation.
.\"
__HEADER__
.\" Define the .__URL__ macro and then override it with the www.tmac package if it
.\" exists.
.de __URL__
\\$2 \(la \\$1 \(ra\\$3
..
.if \n[.g] .mso www.tmac
.TH nvidia\-modprobe 1 "__DATE__" "nvidia\-modprobe __VERSION__"

.SH NAME
nvidia\-modprobe \- Load the NVIDIA kernel module and create NVIDIA character device files.

.SH SYNOPSIS
.BI "nvidia\-modprobe [" "options" "]"
.br
.BI "nvidia\-modprobe [" "options" "] \-\-create\-nvidia\-device\-file=" MINOR-NUMBER "
.br
.BI "nvidia\-modprobe [" "options" "] \-\-module\-instance=" MODULE-INSTANCE "
.br

.SH DESCRIPTION
The
.B nvidia\-modprobe
utility is used by user-space NVIDIA driver components to make sure the NVIDIA kernel module is loaded, the NVIDIA character device files are present and configure certain runtime settings in the kernel.  These facilities are normally provided by Linux distribution configuration systems such as udev.  When possible, it is recommended to use your Linux distribution's native mechanisms for managing kernel module loading, device file creation and kernel runtime config settings.  This utility is provided as a fallback to work out-of-the-box in a distribution-independent way.
.PP
When installed
.B by nvidia\-installer
,
.B nvidia\-modprobe
is installed setuid root.
.PP
Users should not normally need to run
.B nvidia\-modprobe
by hand: the NVIDIA user-space driver components will fork(2)/exec(3) it when needed.
.PP
When run without any options,
.B nvidia\-modprobe
will load the NVIDIA kernel module and then exit. When the
.B \-\-module\-instance
option is specified on systems with multiple NVIDIA kernel modules,
.B nvidia\-modprobe
will load the corresponding NVIDIA kernel module and then exit. When the
.B \-\-create\-nvidia\-device\-file
option is specified,
.B nvidia\-modprobe
will also create the requested device file.

The source code to nvidia-modprobe is available here:
.URL https://download.nvidia.com/XFree86/nvidia-modprobe/

__OPTIONS__

.SH EXAMPLES
.TP
.B nvidia\-modprobe
Run `/sbin/modprobe nvidia`
.TP
.B nvidia\-modprobe -c 0 -c 1
Run `/sbin/modprobe nvidia` and create
.I
/dev/nvidia0
and
.I
/dev/nvidia1
.TP
.B nvidia\-modprobe -i 0
Run `/sbin/modprobe nvidia0`
.

.SH AUTHOR
Andy Ritger
.br
NVIDIA Corporation

.SH COPYRIGHT
Copyright \(co 2013 NVIDIA Corporation.
