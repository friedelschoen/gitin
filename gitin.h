#pragma once

#include <git2/patch.h>
#include <git2/types.h>
#include <stdio.h>

#define MAXPINS 8
#define SUMMARY 5

#define LEN(s) (sizeof(s) / sizeof(*s))


/* helper struct for pipe(2) */
typedef struct {
	int read;
	int write;
} pipe_t;

enum {
	ConfigString,
	ConfigInteger,
	ConfigBoolean,
};

struct config {
	const char* name;
	int         type;
	void*       target;
};

struct deltainfo {
	git_patch* patch;
	size_t     addcount;
	size_t     delcount;
};

struct commitstats {
	size_t addcount;
	size_t delcount;

	struct deltainfo* deltas;
	size_t            ndeltas;
};

struct repoinfo {
	git_repository* repo;
	git_commit*     commit;

	const char* repodir;
	char        destdir[1024];
	int         relpath;

	char        name[100];
	const char* revision;
	const char* description;
	const char* cloneurl;

	const char* submodules;

	const char* pinfiles[MAXPINS];
	int         pinfileslen;

	char** headfiles;
	int    headfileslen;
	int    headfilesalloc;
};

struct callcached_param {
	const char*            command;
	FILE*                  fp;
	const struct repoinfo* info;
	const char*            filename;
	uint32_t               contenthash;
	const char*            cachename;
	const char*            content;
	ssize_t                ncontent;
	const char**           environ;
	int                    nenviron;
};


extern struct config config_keys[];

extern const char* colorscheme;
extern const char* favicon;
extern const char* favicontype;
extern const char* footertext;
extern const char* highlightcmd;
extern const char* pandoccmd;
extern const char* configtreecmd;
extern const char* logoicon;
extern const char* pinfiles[];
extern const char* extrapinfiles;
extern const char* sitedescription;
extern const char* sitename;
extern const char* stylesheet;
extern long long   maxcommits;
extern long long   maxfilesize;
extern int         splitdirectories;

extern const char* configfile;
extern const char* jsonfile;
extern const char* commitatomfile;
extern const char* tagatomfile;

extern const char* default_revision;

extern int force, verbose, columnate;

extern const char* filetypes[][3];

FILE* xfopen(const char* mode, const char* format, ...) __attribute__((format(printf, 2, 3)));
void  xmkdirf(int mode, const char* format, ...) __attribute__((format(printf, 2, 3)));
int   removedir(char* path);
void  normalize_path(char* path);
void  unhide_path(char* path);
void  printprogress(const char* what, ssize_t indx, ssize_t ncommits);
int   endswith(const char* str, const char* suffix);

int bufferwrite(const char* buffer, size_t len, const char* format, ...)
    __attribute__((format(printf, 3, 4)));
int bufferread(char* buffer, size_t len, const char* format, ...)
    __attribute__((format(printf, 3, 4)));

int  getdiff(struct commitstats* ci, const struct repoinfo* info, git_commit* commit, int docache);
void freediff(struct commitstats* ci);

void hprintf(FILE* file, const char* format, ...);
void vhprintf(FILE* file, const char* format, va_list args);

char* parseconfig(FILE* file, struct config* keys);

ssize_t callcached(struct callcached_param* params);

int  writearchive(const struct repoinfo* info, const struct git_reference* ref);
void writeatomheader(FILE* fp, const struct repoinfo* info);
void writeatomfooter(FILE* fp);
void writecommitatom(FILE* fp, git_commit* commit, const char* tag);
void writecommitfile(const struct repoinfo* info, git_commit* commit, const struct commitstats* ci,
                     int parentlink);
void writefooter(FILE* fp);
int  writefiles(struct repoinfo* info);
void writeindex(const char* destdir, char** repos, int nrepos);
int  writeindexline(FILE* fp, const struct repoinfo* info);
void writejsoncommit(FILE* fp, git_commit* commit, int first);
void writejsonref(FILE* fp, const struct repoinfo* info, git_reference* ref, git_commit* commit);
int  writelog(const struct repoinfo* info);
void writeheader(FILE* fp, const struct repoinfo* info, int relpath, const char* name,
                 const char* description, ...);
void writepreview(FILE* fp, const struct repoinfo* info, int relpath, const char* filename,
                  const char* s, ssize_t len, uint32_t contenthash);
int  writerefs(FILE* fp, FILE* json, const struct repoinfo* info);
void writerepo(FILE* index, const char* repodir, const char* destination);
void writeshortlog(FILE* fp, const struct repoinfo* info);
