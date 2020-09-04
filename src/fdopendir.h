#ifndef __FDOPENDIR_H
#define __FDOPENDIR_H

#include <config.h>

#include <dirent.h>
#include "libfakechroot.h"

wrapper_proto(fdopendir, DIR *, (int));

#endif
