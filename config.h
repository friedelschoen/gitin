/* favicon to use */
const char *faviconicon = "favicon.svg";

/* logo to use in header*/
const char *logoicon = "logo.svg";

/* stylesheet to use */
const char *stylesheet = "style.css";

/* files to use as lisense or readme, these will be tagged in the header */
const char *licensefiles[] = { "HEAD:LICENSE", "HEAD:LICENSE.md", "HEAD:COPYING" };
int licensefileslen = 3;

const char *readmefiles[] = { "HEAD:README", "HEAD:README.md" };
int readmefileslen = 2;

/* where to put the html files relative to repodir */
const char* destination = ".";

/* how to index file is called */
const char *indexfile = "index.html";

/* command to execute to highlight a file, use `cat` to disable */
const char *highlightcmd = "chroma --html --html-only --html-inline-styles --style=xcode --lexer=$type";

/* name of cache directory for highlighting */
const char *highlightcache = ".stagit/files";

/* name of cache directory for commit */
const char *commitcache = ".stagitcommits";

/* maximum of commits to log, -1 indicates not used */
long long nlogcommits = -1;
