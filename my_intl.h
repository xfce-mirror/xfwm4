#ifndef __MY_INTL_H__
#define __MY_INTL_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef ENABLE_NLS
#ifdef HAVE_GETTEXT
#include <libintl.h>
#else
#include "intl/libintl.h"
#endif

#ifdef N_
#undef N_
#endif

#ifdef _
#undef _
#endif

#define _(String) dgettext(PACKAGE,String)
#define N_(String) String

#else /* NLS is disabled */

#define _(String) String
#define N_(String) String
#define textdomain(String) String
#define gettext(String) String
#define dgettext(Domain,String) String
#define dcgettext(Domain,String,Type) String
#define bindtextdomain(Domain,Directory) (Domain)

#endif

#endif
