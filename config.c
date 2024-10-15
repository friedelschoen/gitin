#include "gitin.h"

/* Configuration keys for the configuration parser. */
struct config config_keys[] = {
	/* Site information settings */
	{ "name", ConfigString, &sitename },
	{ "description", ConfigString, &sitedescription },
	{ "footer", ConfigString, &footertext },

	/* File and path settings */
	{ "favicon", ConfigString, &favicon },
	{ "favicontype", ConfigString, &favicontype },
	{ "logoicon", ConfigString, &logoicon },
	{ "stylesheet", ConfigString, &stylesheet },
	{ "colorscheme", ConfigString, &colorscheme },
	{ "pinfiles", ConfigString, &extrapinfiles },
	{ "splitdirectories", ConfigBoolean, &splitdirectories },

	{ "cmd/highlight", ConfigString, &highlightcmd },
	{ "cmd/pandoc", ConfigString, &pandoccmd },
	{ "cmd/configtree", ConfigString, &configtreecmd },

	/* Limits for commits and file sizes */
	{ "limit/commits", ConfigInteger, &maxcommits },
	{ "limit/filesize", ConfigInteger, &maxfilesize },

	/* Files and output configurations */
	{ "files/json", ConfigString, &jsonfile },
	{ "files/commit-atom", ConfigString, &commitatomfile },
	{ "files/tag-atom", ConfigString, &tagatomfile },

	{ 0 },
};

/* --- Site information settings --- */

/* The title of the site, which will be displayed in the HTML header or as the page title. */
const char* sitename = "My Repositories";

/* A description of the site, typically shown in metadata or as a subheading. */
const char* sitedescription = "";

/* Footer text that will appear at the bottom of every generated HTML page. */
const char* footertext = "Generated by <i><code>gitin</code></i>!";

/* --- File and path settings --- */

/* Path to the config files */
const char* configfile = "gitin.conf";

/* Path to the favicon file, which will be used in the HTML output. */
const char* favicon = "favicon.svg";

/* The MIME type of the favicon file, indicating the type of image (SVG in this case). */
const char* favicontype = "image/svg+xml";

/* Path to the logo icon file, which will be displayed in the header of the HTML page. */
const char* logoicon = "logo.svg";

/* Path to the stylesheet (CSS file) used for styling the generated HTML. */
const char* stylesheet = "style.css";

/* Command to execute for highlighting syntax in files within the HTML output. The colorscheme will
 * be substituted into this command. */
const char* highlightcmd =
    "chroma --html --html-only --html-lines --html-inline-styles --style=$scheme --lexer=$type";

const char* pandoccmd = "pandoc --from=$type --to=html";

const char* configtreecmd = "gitin-configtree $type";

/* Color scheme to use for syntax highlighting in the HTML output. */
const char* colorscheme = "pastie";

/* Controls whether files are separated and written into individual directories or with each
 * directory having its own HTML index. */
int splitdirectories = 0;

const char* pinfiles[] = {
	"README",          "README.md",       "CONTRIBUTING",
	"CONTRIBUTING.md", "CHANGELOG",       "CHANGELOG.md",
	"LICENSE",         "LICENSE.md",      "COPYING",
	"COPYING.md",      "CODE_OF_CONDUCT", "CODE_OF_CONDUCT.md",
	"SECURITY",        "SECURITY.md",     NULL,
};

const char* extrapinfiles = NULL;

/* --- Commit and log settings --- */

/* Maximum number of commits to display in the log view. A value of -1 means no limit. */
long long maxcommits = -1;

/* --- File size settings --- */

/* Maximum file size (in bytes) that will be processed for syntax highlighting. Files larger than
 * this size (1MB) will be skipped. */
long long maxfilesize = 1e+5;    // 1MB

/* --- Output file names --- */

/* Name of the JSON file containing commit data. */
const char* jsonfile = "commits.json";

/* Name of the file used for Atom feeds of commits. */
const char* commitatomfile = "atom.xml";

/* Name of the file used for Atom feeds of tags or branches. */
const char* tagatomfile = "tags.xml";

const char* default_revision = "HEAD";
