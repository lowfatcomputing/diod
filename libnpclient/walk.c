/*
 * Copyright (C) 2006 by Latchesar Ionkov <lucho@ionkov.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include "9p.h"
#include "npfs.h"
#include "npclient.h"
#include "npcimpl.h"

Npcfid *
npc_walk(Npcfsys *fs, char *path)
{
	int n;
	u32 nfid = P9_NOFID;
	char *fname, *s, *t = NULL;
	char *wnames[P9_MAXWELEM];
	Npfcall *tc = NULL, *rc = NULL;
	Npcfid *fid = NULL;

	errno = 0;
	while (*path == '/')
		path++;

	fname = strdup(path);
	if (!fname) {
		np_uerror(ENOMEM);
		goto error;
	}
	fid = npc_fid_alloc(fs);
	if (!fid)
		goto error;
	s = fname;
	nfid = fs->root->fid;
	while (1) {
		n = 0;
		while (n<P9_MAXWELEM && *s!='\0') {
			if (*s == '/') {
				s++;
				continue;
			}

			wnames[n++] = s;
			t = strchr(s, '/');
			if (!t)
				break;

			*t = '\0';
			s = t + 1;
		}

		if (!(tc = np_create_twalk(nfid, fid->fid, n, wnames)))
			goto error;
		if (npc_rpc(fs, tc, &rc) < 0)
			goto error;

		nfid = fid->fid;
		if (rc->u.rwalk.nwqid != n) {
			np_uerror(ENOENT);
			goto error;
		}
		if (tc)
			free(tc);
		if (rc)
			free(rc);
		if (!t || *s=='\0')
			break;

	}

	free(fname);
	return fid;

error:
	errno = np_rerror ();
	if (rc)
		free(rc);
	if (tc)
		free(tc);
	if (nfid == fid->fid) {
		int saved_errno = errno;
		(void)npc_clunk (fid);
		errno = saved_errno;
	}
	if (fname)
		free(fname);
	return NULL;
}
