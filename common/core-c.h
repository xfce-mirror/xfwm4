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
#else
#define FALLTHROUGH
#endif

#endif /* __XFWM_CORE_C_H */
