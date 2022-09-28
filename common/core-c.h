/* X-SPDX-License-Identifier: GPL-2.0+ */
/* Copyright Enrico Weigelt, metux IT consult <info@metux.net> */

/*
 * Core C programming utilities
 *
 * The things in here would be better placed in libc or some cross-platform
 * and layer directly above libc.
 */

#ifndef __XFWM_CORE_C_H
#define __XFWM_CORE_C_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined (__GNUC__) && __GNUC__ >= 7
#define FALLTHROUGH __attribute__ ((fallthrough))
#define NONNULL(...) __attribute__ ((nonnull(__VA_ARGS__)))
#else
#define FALLTHROUGH
#define NONNULL(...)
#endif

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({          \
    char *__mptr = (char *)(ptr);                   \
    ((type *)(__mptr - offsetof(type, member))); })

#endif /* __XFWM_CORE_C_H */
