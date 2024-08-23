/* favicon to use */
static const char *faviconicon = "favicon.svg";

/* logo to use in header*/
static const char *logoicon = "logo.svg";

/* stylesheet to use */
static const char *stylesheet = "style.css";

/* files to use as lisense or readme, these will be tagged in the header */
static const char *licensefiles[] = { "HEAD:LICENSE", "HEAD:LICENSE.md", "HEAD:COPYING" };
static const char *readmefiles[] = { "HEAD:README", "HEAD:README.md" };

/* where to put the html files relative to repodir */
static const char* destination = ".";

/* how to index file is called */
static const char *indexfile = "index.html";

/* command to execute to highlight a file, use `cat` to disable */
static const char *highlightcmd = "chroma --html --html-only --html-inline-styles --style=xcode --lexer=$type";

/* name of cache directory for highlighting */
static const char *highlightcache = ".stagit/files";

/* name of cache directory for commit */
static const char *commitcache = ".stagitcommits";

/* maximum of commits to log, -1 indicates not used */
static long long nlogcommits = -1;
