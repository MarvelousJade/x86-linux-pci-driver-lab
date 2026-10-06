/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
#ifndef EDU_LAB_ABI_H
#define EDU_LAB_ABI_H
#include <linux/ioctl.h>
#include <linux/types.h>

/* No pointers or native-width fields. Output is valid only on success. */
struct edu_value {
	__u32 input;
	__u32 result;
};
#define EDU_IOC_LIVE _IOWR('E', 1, struct edu_value)
#endif
