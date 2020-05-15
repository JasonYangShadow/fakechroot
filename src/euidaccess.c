/*
    libfakechroot -- fake chroot environment
    Copyright (c) 2010, 2013 Piotr Roszatycki <dexter@debian.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/


#include <config.h>

#ifdef HAVE_EUIDACCESS

#include "libfakechroot.h"
#include "unionfs.h"


wrapper(euidaccess, int, (const char * pathname, int mode))
{
    debug("euidaccess(\"%s\", %d)", pathname, mode);
    expand_chroot_path(pathname);
    if(lxstat(pathname) && is_file_type(pathname, TYPE_LINK)){
        debug("euidaccess encounters symlink: %s, is working on resolving it", pathname);
        char link[MAX_PATH];
        iterResolveSymlink(pathname, link);
        return nextcall(euidaccess)(link, mode);
    }
    return nextcall(euidaccess)(pathname, mode);
}

#else
typedef int empty_translation_unit;
#endif
