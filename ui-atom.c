/* ui-atom.c: functions for atom feeds
 *
 * Copyright (C) 2008 Lars Hjemli
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#include "cgit.h"
#include "html.h"
#include "ui-shared.h"

void add_entry(struct commit *commit, char *host)
{
	char delim = '&';
	char *hex;
	char *mail, *t, *t2;
	struct commitinfo *info;

	info = cgit_parse_commit(commit);
	hex = sha1_to_hex(commit->object.sha1);
	html("<entry>\n");
	html("<title>");
	html_txt(info->subject);
	html("</title>\n");
	html("<updated>");
	cgit_print_date(info->author_date, FMT_ATOMDATE, ctx.cfg.local_time);
	html("</updated>\n");
	html("<author>\n");
	if (info->author) {
		html("<name>");
		html_txt(info->author);
		html("</name>\n");
	}
	if (info->author_email && !ctx.cfg.noplainemail) {
		mail = xstrdup(info->author_email);
		t = strchr(mail, '<');
		if (t)
			t++;
		else
			t = mail;
		t2 = strchr(t, '>');
		if (t2)
			*t2 = '\0';
		html("<email>");
		html_txt(t);
		html("</email>\n");
		free(mail);
	}
	html("</author>\n");
	html("<published>");
	cgit_print_date(info->author_date, FMT_ATOMDATE, ctx.cfg.local_time);
	html("</published>\n");
	if (host) {
		html("<link rel='alternate' type='text/html' href='");
		html(cgit_httpscheme());
		html_attr(host);
		html_attr(cgit_pageurl(ctx.repo->url, "commit", NULL));
		if (ctx.cfg.virtual_root)
			delim = '?';
		htmlf("%cid=%s", delim, hex);
		html("'/>\n");
	}
	htmlf("<id>%s</id>\n", hex);
	html("<content type='text'>\n");
	html_txt(info->msg);
	html("</content>\n");
	html("<content type='xhtml'>\n");
	html("<div xmlns='http://www.w3.org/1999/xhtml'>\n");
	html("<pre>\n");
	html_txt(info->msg);
	html("</pre>\n");
	html("</div>\n");
	html("</content>\n");
	html("</entry>\n");
	cgit_free_commitinfo(info);
}


void cgit_print_atom(char *tip, char *path, int max_count)
{
	char *host;
	const char *argv[] = {NULL, tip, NULL, NULL, NULL};
	struct commit *commit;
	struct rev_info rev;
	int argc = 2;

	if (!tip)
		argv[1] = ctx.qry.head;

	if (path) {
		argv[argc++] = "--";
		argv[argc++] = path;
	}

	init_revisions(&rev, NULL);
	rev.abbrev = DEFAULT_ABBREV;
	rev.commit_format = CMIT_FMT_DEFAULT;
	rev.verbose_header = 1;
	rev.show_root_diff = 0;
	rev.max_count = max_count;
	setup_revisions(argc, argv, &rev, NULL);
	prepare_revision_walk(&rev);

	host = cgit_hosturl();
	ctx.page.mimetype = "text/xml";
	ctx.page.charset = "utf-8";
	cgit_print_http_headers(&ctx);
	html("<feed xmlns='http://www.w3.org/2005/Atom'>\n");
	html("<title>");
	html_txt(ctx.repo->name);
	html("</title>\n");
	html("<subtitle>");
	html_txt(ctx.repo->desc);
	html("</subtitle>\n");
	if (host) {
		html("<link rel='alternate' type='text/html' href='");
		html(cgit_httpscheme());
		html_attr(host);
		html_attr(cgit_repourl(ctx.repo->url));
		html("'/>\n");
	}
	while ((commit = get_revision(&rev)) != NULL) {
		add_entry(commit, host);
		free(commit->buffer);
		commit->buffer = NULL;
		free_commit_list(commit->parents);
		commit->parents = NULL;
	}
	html("</feed>\n");
}
