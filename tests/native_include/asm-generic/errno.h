/* SPDX-License-Identifier: GPL-2.0-only
 * Host-test compatibility only. Not part of the EFI build or target firmware.
 */
#pragma once
#include <errno.h>
#undef EPROTO
#define EPROTO 71
#undef EBADMSG
#define EBADMSG 74
#undef EOVERFLOW
#define EOVERFLOW 75
#undef EOPNOTSUPP
#define EOPNOTSUPP 95
#undef ETIMEDOUT
#define ETIMEDOUT 110
#undef EALREADY
#define EALREADY 114
