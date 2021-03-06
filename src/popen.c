/*      $OpenBSD: popen.c,v 1.18 2007/11/26 19:26:46 kurt Exp $ */
/*
 * Copyright (c) 1988, 1993
 *      The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2010-2015 Piotr Roszatycki <dexter@debian.org>
 *
 * This code is derived from software written by Ken Arnold and
 * published in UNIX Review, Vol. 6, No. 8.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#ifdef __GNUC__

#define _BSD_SOURCE
#define _POSIX_SOURCE
#define _DEFAULT_SOURCE
#include <sys/param.h>
#include <sys/wait.h>

#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>

#include "libfakechroot.h"

#define _MUTEX_LOCK(a)
#define _MUTEX_UNLOCK(a)

static struct pid {
        struct pid *next;
        FILE *fp;
        pid_t pid;
} *pidlist;


#if 0
static void *pidlist_lock = NULL;
#endif

FILE *
popen(const char *program, const char *type)
{
        int errsv = errno;
        struct pid * volatile cur;
        FILE *iop;
        int pdes[2];
        pid_t pid;

        debug("popen(\"%s\", \"%s\")", program, type);

        if ((*type != 'r' && *type != 'w') || type[1] != '\0') {
                errno = EINVAL;
                return (NULL);
        }

        if ((cur = malloc(sizeof(struct pid))) == NULL)
                return (NULL);

        if (pipe(pdes) < 0) {
                free(cur);
                return (NULL);
        }

        _MUTEX_LOCK(&pidlist_lock);
        switch (pid = vfork()) {
        case -1:                        /* Error. */
                _MUTEX_UNLOCK(&pidlist_lock);
                (void)close(pdes[0]);
                (void)close(pdes[1]);
                free(cur);
                return (NULL);
                /* NOTREACHED */
        case 0:                         /* Child. */
            {
                struct pid *pcur;
                /*
                 * because vfork() instead of fork(), must leak FILE *,
                 * but luckily we are terminally headed for an execl()
                 */
                for (pcur = pidlist; pcur; pcur = pcur->next)
                        close(fileno(pcur->fp));

                if (*type == 'r') {
                        int tpdes1 = pdes[1];

                        (void) close(pdes[0]);
                        /*
                         * We must NOT modify pdes, due to the
                         * semantics of vfork.
                         */
                        if (tpdes1 != STDOUT_FILENO) {
                                (void)dup2(tpdes1, STDOUT_FILENO);
                                (void)close(tpdes1);
                                tpdes1 = STDOUT_FILENO;
                        }
                } else {
                        (void)close(pdes[1]);
                        if (pdes[0] != STDIN_FILENO) {
                                (void)dup2(pdes[0], STDIN_FILENO);
                                (void)close(pdes[0]);
                        }
                }
                execl(_PATH_BSHELL, "sh", "-c", program, (char *)NULL);
                _exit(127);
                /* NOTREACHED */
            }
                break;
        }
        _MUTEX_UNLOCK(&pidlist_lock);

        /* Parent; assume fdopen can't fail. */
        if (*type == 'r') {
                iop = fdopen(pdes[0], type);
                (void)close(pdes[1]);
        } else {
                iop = fdopen(pdes[1], type);
                (void)close(pdes[0]);
        }

        /* Link into list of file descriptors. */
        cur->fp = iop;
        cur->pid =  pid;
        _MUTEX_LOCK(&pidlist_lock);
        cur->next = pidlist;
        pidlist = cur;
        _MUTEX_UNLOCK(&pidlist_lock);

        errno = errsv;
        return (iop);
}

/*
 * pclose --
 *      Pclose returns -1 if stream is not associated with a `popened' command,
 *      if already `pclosed', or waitpid returns an error.
 */
int
pclose(FILE *iop)
{
        int errsv = errno;
        struct pid *cur, *last;
        int pstat;
        pid_t pid;

        debug("popen(iop)");

        /* Find the appropriate file pointer. */
        _MUTEX_LOCK(&pidlist_lock);
        for (last = NULL, cur = pidlist; cur; last = cur, cur = cur->next)
                if (cur->fp == iop)
                        break;

        if (cur == NULL) {
                _MUTEX_UNLOCK(&pidlist_lock);
                return (-1);
        }

        /* Remove the entry from the linked list. */
        if (last == NULL)
                pidlist = cur->next;
        else
                last->next = cur->next;
        _MUTEX_UNLOCK(&pidlist_lock);

        (void)fclose(iop);

        do {
                pid = waitpid(cur->pid, &pstat, 0);
        } while (pid == -1 && errno == EINTR);

        free(cur);

        errno = errsv;
        return (pid == -1 ? -1 : pstat);
}

#else
typedef int empty_translation_unit;
#endif
