#include "hprintf.h"
#include "writer.h"

#include <git2/commit.h>

void writecommitatom(FILE* fp, git_commit* commit, git_reference* tag) {
	char                 oid[GIT_OID_SHA1_HEXSIZE + 1], parentoid[GIT_OID_SHA1_HEXSIZE + 1];
	const git_signature* author    = git_commit_author(commit);
	const git_signature* committer = git_commit_committer(commit);
	const char*          summary   = git_commit_summary(commit);
	const char*          message   = git_commit_message(commit);

	git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));
	git_oid_tostr(parentoid, sizeof(parentoid), git_commit_parent_id(commit, 0));

	fputs("<entry>\n", fp);

	fprintf(fp, "<id>%s</id>\n", oid);
	if (author) {
		hprintf(fp, "<published>%t</published>\n", &author->when);
	}
	if (committer) {
		hprintf(fp, "<published>%t</published>\n", &committer->when);
	}
	if (summary) {
		fputs("<title>", fp);
		if (tag) {
			hprintf(fp, "[%y] ", git_reference_shorthand(tag));
		}
		hprintf(fp, "%y</title>", summary);
	}
	fprintf(fp, "<link rel=\"alternate\" type=\"text/html\" href=\"commit/%s.html\" />\n", oid);

	if (author) {
		hprintf(fp, "<author>\n<name>%y</name>\n<email>%y</email>\n</author>\n", author->name,
		        author->email);
	}

	fputs("<content>", fp);
	fprintf(fp, "commit %s\n", oid);
	if (*parentoid)
		fprintf(fp, "parent %s\n", parentoid);
	if (author) {
		hprintf(fp, "Author: %y &lt;%y&gt;\n", author->name, author->email);
		hprintf(fp, "Date:   %T\n", &author->when);
	}
	if (message) {
		hprintf(fp, "\n%y", message);
	}
	fputs("\n</content>\n</entry>\n", fp);
}

void writeatomheader(FILE* fp, const struct repoinfo* info) {
	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	      "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n",
	      fp);
	hprintf(fp, "<title>%y, branch HEAD</title>\n<subtitle>%y</subtitle>\n", info->name,
	        info->description);
}

void writeatomfooter(FILE* fp) {
	fputs("</feed>\n", fp);
}

void writeatomrefs(FILE* atom, const struct repoinfo* info) {
	writeatomheader(atom, info);

	for (int i = 0; i < info->nbranches; i++) {
		writecommitatom(atom, info->branches[i].commit, info->branches[i].ref);
	}

	for (int i = 0; i < info->ntags; i++) {
		writecommitatom(atom, info->tags[i].commit, info->tags[i].ref);
	}

	writeatomfooter(atom);
}
