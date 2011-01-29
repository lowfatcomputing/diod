/*
 * Copyright (C) 2005 by Latchesar Ionkov <lucho@ionkov.net>
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
 * LATCHESAR IONKOV AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
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
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include "9p.h"
#include "npfs.h"
#include "npfsimpl.h"

static int
np_printperm(char *s, int len, int perm)
{
	int n;
	char b[10];

	n = 0;
	if (perm & P9_DMDIR)
		b[n++] = 'd';
	if (perm & P9_DMAPPEND)
		b[n++] = 'a';
	if (perm & P9_DMAUTH)
		b[n++] = 'A';
	if (perm & P9_DMEXCL)
		b[n++] = 'l';
	if (perm & P9_DMTMP)
		b[n++] = 't';
	if (perm & P9_DMDEVICE)
		b[n++] = 'D';
	if (perm & P9_DMSOCKET)
		b[n++] = 'S';
	if (perm & P9_DMNAMEDPIPE)
		b[n++] = 'P';
        if (perm & P9_DMSYMLINK)
                b[n++] = 'L';
        b[n] = '\0';

        return snprintf(s, len, "%s%03o", b, perm&0777);
}             

static int
np_printqid(char *s, int len, Npqid *q)
{
	int n;
	char buf[10];

	n = 0;
	if (q->type & P9_QTDIR)
		buf[n++] = 'd';
	if (q->type & P9_QTAPPEND)
		buf[n++] = 'a';
	if (q->type & P9_QTAUTH)
		buf[n++] = 'A';
	if (q->type & P9_QTEXCL)
		buf[n++] = 'l';
	if (q->type & P9_QTTMP)
		buf[n++] = 't';
	if (q->type & P9_QTSYMLINK)
		buf[n++] = 'L';
	buf[n] = '\0';

	return snprintf(s, len, " (%.16llx %x '%s')", (unsigned long long)q->path, q->version, buf);
}

int
np_snprintstat(char *s, int len, Npstat *st, int dotu)
{
	int n;

	n = snprintf(s, len, "'%.*s' '%.*s' '%.*s' '%.*s' q ", 
		st->name.len, st->name.str, st->uid.len, st->uid.str,
		st->gid.len, st->gid.str, st->muid.len, st->muid.str);

	n += np_printqid(s + n, len - n, &st->qid);
	n += snprintf(s + n, len - n, " m ");
	n += np_printperm(s + n, len - n, st->mode);
	n += snprintf(s + n, len - n, " at %d mt %d l %llu t %d d %d",
		st->atime, st->mtime, (unsigned long long)st->length, st->type, st->dev);
	if (dotu)
		n += snprintf(s + n, len - n, " ext '%.*s'", st->extension.len, 
			st->extension.str);

	return n;
}

int
np_printstat(FILE *f, Npstat *st, int dotu)
{
	char s[256];

	np_snprintstat(s, sizeof(s), st, dotu);
	return fprintf (f, "%s", s);
}

int
np_sndump(char *s, int len, u8 *data, int datalen)
{
	int i, n;

	i = n = 0;
	while (i < datalen) {
		n += snprintf(s + n, len - n, "%02x", data[i]);
		if (i%4 == 3)
			n += snprintf(s + n, len - n, " ");
		if (i%32 == 31 && i + 1 < datalen)
			n += snprintf(s + n, len - n, "\n");

		i++;
	}
	//n += snprintf(s + n, len - n, "\n");

	return n;
}

int
np_dump(FILE *f, u8 *data, int datalen)
{
	char s[256];
	
	np_sndump(s, sizeof(s), data, datalen);
	return fprintf (f, "%s", s);
}

static int
np_printdata(char *s, int len, u8 *buf, int buflen)
{
	return np_sndump(s, len, buf, buflen<64?buflen:64);
}

int
np_dumpdata(char *s, int len, u8 *buf, int buflen)
{
	return np_sndump(s, len, buf, buflen);
}

int
np_snprintfcall(char *s, int len, Npfcall *fc, int dotu) 
{
	int i, n, fid, tag;
	enum p9_msg_t type;

	if (!fc)
		return snprintf(s, len, "NULL");

	type = fc->type;
	fid = fc->fid;
	tag = fc->tag;

	n = 0;
	switch (type) {
#if HAVE_DOTL
	case P9_TLERROR:
		n += snprintf(s+n,len-n, "P9_TLERROR tag %u", tag);
		break;
	case P9_RLERROR:
		n += snprintf(s+n,len-n, "P9_RLERROR tag %u ecode %d", tag,
			fc->u.rlerror.ecode);
		break;
	case P9_TSTATFS:
		n += snprintf(s+n,len-n, "P9_TSTATFS tag %u", tag);
		break;
	case P9_RSTATFS:
		n += snprintf(s+n,len-n, "P9_RSTATFS tag %u", tag);
		break;
	case P9_TLOPEN:
		n += snprintf(s+n,len-n, "P9_TLOPEN tag %u "
			"fid %"PRIu32" mode %"PRIu32, tag,
			fc->u.tlopen.fid,
			fc->u.tlopen.mode);
		break;
	case P9_RLOPEN:
		n += snprintf(s+n,len-n, "P9_RLOPEN tag %u "
			"qid", tag);
		n += np_printqid(s+n,len-n, &fc->u.rlopen.qid);
		n += snprintf(s+n,len-n, " iounit %"PRIu32,
			fc->u.rlopen.iounit);
		break;
	case P9_TLCREATE:
		n += snprintf(s+n,len-n, "P9_TLCREATE tag %u", tag);
		break;
	case P9_RLCREATE:
		n += snprintf(s+n,len-n, "P9_RLCREATE tag %u", tag);
		break;
	case P9_TSYMLINK:
		n += snprintf(s+n,len-n, "P9_TSYMLINK tag %u", tag);
		break;
	case P9_RSYMLINK:
		n += snprintf(s+n,len-n, "P9_RSYMLINK tag %u", tag);
		break;
	case P9_TMKNOD:
		n += snprintf(s+n,len-n, "P9_TMKNOD tag %u", tag);
		break;
	case P9_RMKNOD:
		n += snprintf(s+n,len-n, "P9_RMKNOD tag %u", tag);
		break;
	case P9_TRENAME:
		n += snprintf(s+n,len-n, "P9_TRENAME tag %u "
			"fid %"PRIu32" newdirfid %"PRIu32" name %.*s", tag,
			fc->u.trename.fid,
			fc->u.trename.newdirfid,
			fc->u.trename.name.len,
			fc->u.trename.name.str);
		break;
	case P9_RRENAME:
		n += snprintf(s+n,len-n, "P9_RRENAME tag %u", tag);
		break;
	case P9_TREADLINK:
		n += snprintf(s+n,len-n, "P9_TREADLINK tag %u", tag);
		break;
	case P9_RREADLINK:
		n += snprintf(s+n,len-n, "P9_RREADLINK tag %u", tag);
		break;
	case P9_TGETATTR:
		n += snprintf(s+n,len-n, "P9_TGETATTR tag %u "
			"fid %"PRIu32" request_mask %"PRIu64, tag,
			fc->u.tgetattr.fid,
			fc->u.tgetattr.request_mask);
		break;
	case P9_RGETATTR:
		n += snprintf(s+n,len-n, "P9_RGETATTR tag %u "
			"response_mask %"PRIu64, tag,
			fc->u.rgetattr.response_mask);
		break;
	case P9_TSETATTR:
		n += snprintf(s+n,len-n, "P9_TSETATTR tag %u", tag);
		break;
	case P9_RSETATTR:
		n += snprintf(s+n,len-n, "P9_RSETATTR tag %u", tag);
		break;
	case P9_TXATTRWALK:
		n += snprintf(s+n,len-n, "P9_TXATTRWALK tag %u", tag);
		break;
	case P9_RXATTRWALK:
		n += snprintf(s+n,len-n, "P9_RXATTRWALK tag %u", tag);
		break;
	case P9_TXATTRCREATE:
		n += snprintf(s+n,len-n, "P9_TXATTRWALKCREATE tag %u", tag);
		break;
	case P9_RXATTRCREATE:
		n += snprintf(s+n,len-n, "P9_RXATTRWALKCREATE tag %u", tag);
		break;
	case P9_TREADDIR:
		n += snprintf(s+n,len-n, "P9_TREADDIR tag %u "
			"fid %"PRIu32" offset %"PRIu64" count %"PRIu32, tag,
			fc->u.treaddir.fid,
			fc->u.treaddir.offset,
			fc->u.treaddir.count);
		break;
	case P9_RREADDIR:
		n += snprintf(s+n,len-n, "P9_RREADDIR tag %u count %"PRIu32,
			tag,
			fc->u.rreaddir.count);
		break;
	case P9_TFSYNC:
		n += snprintf(s+n,len-n, "P9_TFSYNC tag %u", tag);
		break;
	case P9_RFSYNC:
		n += snprintf(s+n,len-n, "P9_RFSYNC tag %u", tag);
		break;
	case P9_TLOCK:
		n += snprintf(s+n,len-n, "P9_TLOCK tag %u", tag);
		break;
	case P9_RLOCK:
		n += snprintf(s+n,len-n, "P9_RLOCK tag %u", tag);
		break;
	case P9_TGETLOCK:
		n += snprintf(s+n,len-n, "P9_TGETLOCK tag %u", tag);
		break;
	case P9_RGETLOCK:
		n += snprintf(s+n,len-n, "P9_RGETLOCK tag %u", tag);
		break;
	case P9_TLINK:
		n += snprintf(s+n,len-n, "P9_TLINK tag %u", tag);
		break;
	case P9_RLINK:
		n += snprintf(s+n,len-n, "P9_RLINK tag %u", tag);
		break;
	case P9_TMKDIR:
		n += snprintf(s+n,len-n, "P9_TMKDIR tag %u", tag);
		break;
	case P9_RMKDIR:
		n += snprintf(s+n,len-n, "P9_RMKDIR tag %u", tag);
		break;
#endif
	case P9_TVERSION:
		n += snprintf(s+n,len-n, "P9_TVERSION tag %u msize %u version '%.*s'", 
			tag, fc->msize, fc->version.len, fc->version.str);
		break;

	case P9_RVERSION:
		n += snprintf(s+n,len-n, "P9_RVERSION tag %u msize %u version '%.*s'", 
			tag, fc->msize, fc->version.len, fc->version.str);
		break;

	case P9_TAUTH:
		n += snprintf(s+n,len-n, "P9_TAUTH tag %u afid %d uname %.*s aname %.*s",
			tag, fc->afid, fc->uname.len, fc->uname.str, 
			fc->aname.len, fc->aname.str);
		break;

	case P9_RAUTH:
		n += snprintf(s+n,len-n, "P9_RAUTH tag %u qid ", tag); 
		n += np_printqid(s+n, len-n, &fc->qid);
		break;

	case P9_TATTACH:
		n += snprintf(s+n,len-n, "P9_TATTACH tag %u fid %d afid %d uname %.*s aname %.*s n_uname %u",
			tag, fid, fc->afid, fc->uname.len, fc->uname.str, 
			fc->aname.len, fc->aname.str, fc->n_uname);
		break;

	case P9_RATTACH:
		n += snprintf(s+n,len-n, "P9_RATTACH tag %u qid ", tag); 
		n += np_printqid(s+n,len-n, &fc->qid);
		break;

	case P9_RERROR:
		n += snprintf(s+n,len-n, "P9_RERROR tag %u ename %.*s", tag, 
			fc->ename.len, fc->ename.str);
		if (dotu)
			n += snprintf(s+n,len-n, " ecode %d", fc->ecode);
		break;

	case P9_TFLUSH:
		n += snprintf(s+n,len-n, "P9_TFLUSH tag %u oldtag %u", tag, fc->oldtag);
		break;

	case P9_RFLUSH:
		n += snprintf(s+n,len-n, "P9_RFLUSH tag %u", tag);
		break;

	case P9_TWALK:
		n += snprintf(s+n,len-n, "P9_TWALK tag %u fid %d newfid %d nwname %d", 
			tag, fid, fc->newfid, fc->nwname);
		for(i = 0; i < fc->nwname; i++)
			n += snprintf(s+n,len-n, " '%.*s'", fc->wnames[i].len, 
				fc->wnames[i].str);
		break;
		
	case P9_RWALK:
		n += snprintf(s+n,len-n, "P9_RWALK tag %u nwqid %d", tag, fc->nwqid);
		for(i = 0; i < fc->nwqid; i++)
			n += np_printqid(s+n,len-n, &fc->wqids[i]);
		break;
		
	case P9_TOPEN:
		n += snprintf(s+n,len-n, "P9_TOPEN tag %u fid %d mode %d", tag, fid, 
			fc->mode);
		break;
		
	case P9_ROPEN:
		n += snprintf(s+n,len-n, "P9_ROPEN tag %u", tag);
		n += np_printqid(s+n,len-n, &fc->qid);
		n += snprintf(s+n,len-n, " iounit %d", fc->iounit);
		break;
		
	case P9_TCREATE:
		n += snprintf(s+n,len-n, "P9_TCREATE tag %u fid %d name %.*s perm ",
			tag, fid, fc->name.len, fc->name.str);
		n += np_printperm(s+n,len-n, fc->perm);
		n += snprintf(s+n,len-n, " mode %d", fc->mode);
		if (dotu)
			n += snprintf(s+n,len-n, " ext %.*s", fc->extension.len,
				fc->extension.str);
		break;
		
	case P9_RCREATE:
		n += snprintf(s+n,len-n, "P9_RCREATE tag %u", tag);
		n += np_printqid(s+n,len-n, &fc->qid);
		n += snprintf(s+n,len-n, " iounit %d", fc->iounit);
		break;
		
	case P9_TREAD:
		n += snprintf(s+n,len-n, "P9_TREAD tag %u fid %d offset %llu count %u", 
			tag, fid, (unsigned long long)fc->offset, fc->count);
		break;
		
	case P9_RREAD:
		n += snprintf(s+n,len-n, "P9_RREAD tag %u count %u data ", tag, fc->count);
		n += np_printdata(s+n,len-n, fc->data, fc->count);
		break;
		
	case P9_TWRITE:
		n += snprintf(s+n,len-n, "P9_TWRITE tag %u fid %d offset %llu count %u data ",
			tag, fid, (unsigned long long)fc->offset, fc->count);
		n += np_printdata(s+n,len-n, fc->data, fc->count);
		break;
		
	case P9_RWRITE:
		n += snprintf(s+n,len-n, "P9_RWRITE tag %u count %u", tag, fc->count);
		break;
		
	case P9_TCLUNK:
		n += snprintf(s+n,len-n, "P9_TCLUNK tag %u fid %d", tag, fid);
		break;
		
	case P9_RCLUNK:
		n += snprintf(s+n,len-n, "P9_RCLUNK tag %u", tag);
		break;
		
	case P9_TREMOVE:
		n += snprintf(s+n,len-n, "P9_TREMOVE tag %u fid %d", tag, fid);
		break;
		
	case P9_RREMOVE:
		n += snprintf(s+n,len-n, "P9_RREMOVE tag %u", tag);
		break;
		
	case P9_TSTAT:
		n += snprintf(s+n,len-n, "P9_TSTAT tag %u fid %d", tag, fid);
		break;
		
	case P9_RSTAT:
		n += snprintf(s+n,len-n, "P9_RSTAT tag %u ", tag);
		n += np_snprintstat(s+n,len-n, &fc->stat, dotu);
		break;
		
	case P9_TWSTAT:
		n += snprintf(s+n,len-n, "P9_TWSTAT tag %u fid %d ", tag, fid);
		n += np_snprintstat(s+n,len-n, &fc->stat, dotu);
		break;
		
	case P9_RWSTAT:
		n += snprintf(s+n,len-n, "P9_RWSTAT tag %u", tag);
		break;
#if HAVE_LARGEIO
	case P9_TAREAD:
		n += snprintf(s+n,len-n, "P9_TAREAD.diod tag %u fid %d datacheck %d offset %llu count %u rsize %u", 
			tag, fid, fc->datacheck, (unsigned long long)fc->offset,
			fc->count, fc->rsize);
		break;
	case P9_RAREAD:
		n += snprintf(s+n,len-n, "P9_RAREAD.diod tag %u count %u data ", tag, fc->count);
		n += np_printdata(s+n,len-n, fc->data, fc->count);
		break;
		
	case P9_TAWRITE:
		n += snprintf(s+n,len-n, "P9_TAWRITE.diod tag %u fid %d datacheck %d offset %llu count %u rsize %u data ",
			tag, fid, fc->datacheck, (unsigned long long)fc->offset,
			fc->count, fc->rsize);
		n += np_printdata(s+n,len-n, fc->data, fc->rsize);
		break;
	case P9_RAWRITE:
		n += snprintf(s+n,len-n, "P9_RAWRITE.diod tag %u count %u", tag, fc->count);
		break;
#endif
	default:
		n += snprintf(s+n,len-n, "unknown type %d", type);
		break;
	}

	return n;
}

int
np_printfcall(FILE *f, Npfcall *fc, int dotu)
{
	char s[256];

	np_snprintfcall (s, sizeof(s), fc, dotu);

	return fprintf (f, "%s", s);
}