/* config.c: parsing of config files
 *
 * Copyright (C) 2006 Lars Hjemli
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#include "cgit.h"

/*
 * url syntax: [repo ['/' cmd [ '/' path]]]
 *   repo: any valid repo url, may contain '/'
 *   cmd:  log | commit | diff | tree | view | blob | snapshot
 *   path: any valid path, may contain '/'
 *
 */
void cgit_parse_url(const char *url)
{
	char *cmd, *p;

	ctx.repo = NULL;
	if (!url || url[0] == '\0')
		return;

	ctx.repo = cgit_get_repoinfo(url);
	if (ctx.repo) {
		ctx.qry.repo = ctx.repo->url;
		return;
	}

	cmd = strchr(url, '/');
	while (!ctx.repo && cmd) {
		cmd[0] = '\0';
		ctx.repo = cgit_get_repoinfo(url);
		if (ctx.repo == NULL) {
			cmd[0] = '/';
			cmd = strchr(cmd + 1, '/');
			continue;
		}

		ctx.qry.repo = ctx.repo->url;
		p = strchr(cmd + 1, '/');
		if (p) {
			p[0] = '\0';
			if (p[1])
				ctx.qry.path = trim_end(p + 1, '/');
		}
		if (cmd[1])
			ctx.qry.page = xstrdup(cmd + 1);
		return;
	}
}

char *substr(const char *head, const char *tail)
{
	char *buf;

	buf = xmalloc(tail - head + 1);
	strncpy(buf, head, tail - head);
	buf[tail - head] = '\0';
	return buf;
}

char *parse_user(char *t, char **name, char **email, unsigned long *date)
{
	char *p = t;
	int mode = 1;

	while (p && *p) {
		if (mode == 1 && *p == '<') {
			*name = substr(t, p - 1);
			t = p;
			mode++;
		} else if (mode == 1 && *p == '\n') {
			*name = substr(t, p);
			p++;
			break;
		} else if (mode == 2 && *p == '>') {
			*email = substr(t, p + 1);
			t = p;
			mode++;
		} else if (mode == 2 && *p == '\n') {
			*email = substr(t, p);
			p++;
			break;
		} else if (mode == 3 && isdigit(*p)) {
			*date = atol(p);
			mode++;
		} else if (*p == '\n') {
			p++;
			break;
		}
		p++;
	}
	return p;
}

#ifdef NO_ICONV
#define reencode(a, b, c)
#else
const char *reencode(char **txt, const char *src_enc, const char *dst_enc)
{
	char *tmp;

	if (!txt || !*txt || !src_enc || !dst_enc)
		return *txt;

	tmp = reencode_string(*txt, src_enc, dst_enc);
	if (tmp) {
		free(*txt);
		*txt = tmp;
	}
	return *txt;
}
#endif

struct commitinfo *cgit_parse_commit(struct commit *commit)
{
	struct commitinfo *ret;
	char *p = commit->buffer, *t = commit->buffer;

	ret = xmalloc(sizeof(*ret));
	ret->commit = commit;
	ret->author = NULL;
	ret->author_email = NULL;
	ret->committer = NULL;
	ret->committer_email = NULL;
	ret->subject = NULL;
	ret->msg = NULL;
	ret->msg_encoding = NULL;

	if (p == NULL)
		return ret;

	if (strncmp(p, "tree ", 5))
		die("Bad commit: %s", sha1_to_hex(commit->object.sha1));
	else
		p += 46; // "tree " + hex[40] + "\n"

	while (!strncmp(p, "parent ", 7))
		p += 48; // "parent " + hex[40] + "\n"

	if (p && !strncmp(p, "author ", 7)) {
		p = parse_user(p + 7, &ret->author, &ret->author_email,
			&ret->author_date);
	}

	if (p && !strncmp(p, "committer ", 9)) {
		p = parse_user(p + 9, &ret->committer, &ret->committer_email,
			&ret->committer_date);
	}

	if (p && !strncmp(p, "encoding ", 9)) {
		p += 9;
		t = strchr(p, '\n');
		if (t) {
			ret->msg_encoding = substr(p, t + 1);
			p = t + 1;
		}
	}

	// skip unknown header fields
	while (p && *p && (*p != '\n')) {
		p = strchr(p, '\n');
		if (p)
			p++;
	}

	// skip empty lines between headers and message
	while (p && *p == '\n')
		p++;

	if (!p)
		return ret;

	t = strchr(p, '\n');
	if (t) {
		ret->subject = substr(p, t);
		p = t + 1;

		while (p && *p == '\n') {
			p = strchr(p, '\n');
			if (p)
				p++;
		}
		if (p)
			ret->msg = xstrdup(p);
	} else
		ret->subject = xstrdup(p);

	if (ret->msg_encoding) {
		reencode(&ret->subject, PAGE_ENCODING, ret->msg_encoding);
		reencode(&ret->msg, PAGE_ENCODING, ret->msg_encoding);
	}

	return ret;
}


struct taginfo *cgit_parse_tag(struct tag *tag)
{
	void *data;
	enum object_type type;
	unsigned long size;
	char *p;
	struct taginfo *ret;

	data = read_sha1_file(tag->object.sha1, &type, &size);
	if (!data || type != OBJ_TAG) {
		free(data);
		return 0;
	}

	ret = xmalloc(sizeof(*ret));
	ret->tagger = NULL;
	ret->tagger_email = NULL;
	ret->tagger_date = 0;
	ret->msg = NULL;

	p = data;

	while (p && *p) {
		if (*p == '\n')
			break;

		if (!strncmp(p, "tagger ", 7)) {
			p = parse_user(p + 7, &ret->tagger, &ret->tagger_email,
				&ret->tagger_date);
		} else {
			p = strchr(p, '\n');
			if (p)
				p++;
		}
	}

	// skip empty lines between headers and message
	while (p && *p == '\n')
		p++;

	if (p && *p)
		ret->msg = xstrdup(p);
	free(data);
	return ret;
}
