		"<html dir=\"ltr\" lang=\"en\">\n<head>\n"
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
		"<meta http-equiv=\"Content-Language\" content=\"en\" />\n", fp);
	fprintf(fp, "<title>%s%s%s</title>\n", name, description[0] ? " - " : "", description);
	fprintf(fp, "<link rel=\"icon\" type=\"image/png\" href=\"%sfavicon.png\" />\n", relpath);
	fprintf(fp, "<link rel=\"alternate\" type=\"application/atom+xml\" title=\"%s Atom Feed\" href=\"%satom.xml\" />\n",
	fprintf(fp, "<link rel=\"stylesheet\" type=\"text/css\" href=\"%sstyle.css\" />\n", relpath);
	fputs("</head>\n<body>\n", fp);
	fprintf(fp, "<h1><img src=\"%slogo.png\" alt=\"\" /> %s <span class=\"desc\">%s</span></h1>\n",
		relpath, name, description);
	fprintf(fp, "<a href=\"%slog.html\">Log</a> | ", relpath);
	fprintf(fp, "<a href=\"%sfiles.html\">Files</a>", relpath);
	/*fprintf(fp, "| <a href=\"%sstats.html\">Stats</a>", relpath);*/
	fputs("\n<hr/>\n<pre>", fp);
	return !fputs("</pre>\n</body>\n</html>", fp);
	fprintf(fp, "%s %c%02d%02d", out, sign, hours, minutes);
	const char *msg;
	fprintf(fp, "<b>commit</b> <a href=\"%scommit/%s.html\">%s</a>\n",
	if (git_oid_tostr(buf, sizeof(buf), git_commit_parent_id(commit, 0)) && buf[0])
		fprintf(fp, "<b>parent</b> <a href=\"%scommit/%s.html\">%s</a>\n",
		fprintf(fp, "<b>Merge:</b>");
		for (i = 0; i < count; i++) {
		fprintf(fp, "<b>Author:</b> ");
		fprintf(fp, " &lt;<a href=\"mailto:");
		fputs("\">", fp);
		xmlencode(fp, sig->email, strlen(sig->email));
		fputs("</a>&gt;\n<b>Date:</b>   ", fp);
		fputc('\n', fp);
	if ((msg = git_commit_message(commit)))
		xmlencode(fp, msg, strlen(msg));
	if (!(error = git_commit_parent(&parent, commit, 0))) {
		if ((error = git_commit_tree(&parent_tree, parent)))
			goto err; /* TODO: handle error */
	} else {
		parent = NULL;
		parent_tree = NULL;
	}
	if ((error = git_diff_tree_to_tree(&diff, repo, parent_tree, commit_tree, NULL)))
		    GIT_DIFF_STATS_FULL | GIT_DIFF_STATS_SHORT, 80)) {
			fprintf(fp, "<b>Diffstat:</b>\n");
		fprintf(fp, "<b>diff --git a/<a href=\"%sfile/%s\">%s</a> b/<a href=\"%sfile/%s\">%s</a></b>\n",
		/* TODO: "new file mode <mode>". */
		/* TODO: add indexfrom...indexto + flags */

		fputs("<b>--- ", fp);
		if (delta->status & GIT_DELTA_ADDED)
			fputs("/dev/null", fp);
		else
			fprintf(fp, "a/<a href=\"%sfile/%s\">%s</a>",
				relpath, delta->old_file.path, delta->old_file.path);

		fputs("\n+++ ", fp);
		if (delta->status & GIT_DELTA_DELETED)
			fputs("/dev/null", fp);
		else
			fprintf(fp, "b/<a href=\"%sfile/%s\">%s</a>",
				relpath, delta->new_file.path, delta->new_file.path);
		fputs("</b>\n", fp);
		/* check binary data */
		if (delta->flags & GIT_DIFF_FLAG_BINARY) {
			fputs("Binary files differ\n", fp);
			git_patch_free(patch);
			continue;
		}

			fprintf(fp, "<span class=\"h\">%s</span>\n", hunk->header);
					fprintf(fp, "<span class=\"i\"><a href=\"#h%zu-%zu\" id=\"h%zu-%zu\">+",
						j, k, j, k);
					fprintf(fp, "<span class=\"d\"><a href=\"#h%zu-%zu\" id=\"h%zu-%zu\">-",
						j, k, j, k);
				if (line->old_lineno == -1 || line->new_lineno == -1)
					fputs("</a></span>", fp);
	int error, ret = 0;
	i = 0; /* DEBUG: to limit commits */
	fputs("<table><thead>\n<tr><td>Commit message</td><td>Author</td><td align=\"right\">Age</td>"
	      "<td align=\"right\">Files</td><td align=\"right\">+</td><td align=\"right\">-</td></tr>\n</thead><tbody>\n", fp);
		/* DEBUG */
/*		if (i++ > 100)
			break;*/
		relpath = "";

		if (git_commit_lookup(&commit, repo, &id)) {
			ret = 1;
			goto err;
		}
			goto errdiff; /* TODO: handle error */
		if (!(error = git_commit_parent(&parent, commit, 0))) {
			if ((error = git_commit_tree(&parent_tree, parent)))
				goto errdiff; /* TODO: handle error */
		} else {
			parent = NULL;
			parent_tree = NULL;
		}

		if ((error = git_diff_tree_to_tree(&diff, repo, parent_tree, commit_tree, NULL)))
		fputs("</td></tr>\n", fp);
		relpath = "../";
errdiff:
err:
	relpath = "";
	return ret;
	const char *msg, *summary;
	fputs("<entry>\n", fp);
	fprintf(fp, "<id>%s</id>\n", buf);
		fputs("</updated>\n", fp);
		fputs("<title type=\"text\">", fp);
		fputs("</title>\n", fp);
	if (git_oid_tostr(buf, sizeof(buf), git_commit_parent_id(commit, 0)) && buf[0])
		for (i = 0; i < count; i++) {
	if ((msg = git_commit_message(commit)))
		xmlencode(fp, msg, strlen(msg));
	fputs("\n</content>\n", fp);
		fputs("</name>\n<email>", fp);
		fputs("</email>\n</author>\n", fp);
	fputs("</entry>\n", fp);
	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", fp);
	fputs("<feed xmlns=\"http://www.w3.org/2005/Atom\">\n<title>", fp);
	fputs(", branch master</title>\n<subtitle>", fp);
	fputs("</subtitle>\n", fp);
	fputs("<table><thead>\n<tr><td>Mode</td><td>Name</td><td align=\"right\">Size</td></tr>\n</thead><tbody>\n", fp);
		fputs("</td></tr>\n", fp);
	/* check LICENSE */
	haslicense = !git_revparse_single(&obj, repo, "HEAD:LICENSE");
	/* check README */
	hasreadme = !git_revparse_single(&obj, repo, "HEAD:README");

	/* Atom feed */
	fp = efopen("atom.xml", "w+b");
	writeatom(fp);
	fclose(fp);
