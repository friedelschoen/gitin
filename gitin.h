#pragma once

#include <git2/patch.h>
#include <git2/types.h>
#include <stdio.h>

#define MAXPINS 8
#define SUMMARY 5

#define FORMAT(a, b) __attribute__((format(printf, a, b)))
#define LEN(s)       (sizeof(s) / sizeof(*s))
#define FORMASK(var, in)                      \
	for (int var = 1; var <= (in); var <<= 1) \
		if (var & (in))

/* Enums */
enum {
	ArchiveTarGz  = 1 << 0,
	ArchiveTarXz  = 1 << 1,
	ArchiveTarBz2 = 1 << 2,
	ArchiveZip    = 1 << 3,
};

enum {
	ConfigString,
	ConfigInteger,
	ConfigBoolean,
	ConfigBooleanAuto,
};

/* Structs */
struct blob {
	const char* name;
	const char* path;

	const char* content;
	ssize_t     length;
	uint32_t    hash;
	int         is_binary;
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

struct commitstats {
	size_t addcount;
	size_t delcount;

	struct deltainfo* deltas;
	size_t            ndeltas;
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

typedef struct {
	int read;
	int write;
} pipe_t;

struct indexinfo {
	const char* repodir;
	char*       name;
	char*       description;
};

struct repoinfo {
	git_repository* repo;
	git_reference*  head;

	const char* repodir;
	char        destdir[1024];
	int         relpath;

	char        name[100];
	const char* description;
	const char* cloneurl;

	const char* submodules;

	const char* pinfiles[MAXPINS];
	int         pinfileslen;

	char** headfiles;
	int    headfileslen;
	int    headfilesalloc;
};

/* Variables */
extern const char*   archiveexts[];
extern int           archivetypes;
extern int           archivezip;
extern int           archivetarbz2;
extern int           archivetargz;
extern int           archivetarxz;
extern size_t        autofilelimit;
extern struct config config_keys[];
extern const char*   colorscheme;
extern const char*   configfile;
extern const char*   configtreecmd;
extern const char*   extrapinfiles;
extern const char*   favicon;
extern const char*   favicontype;
extern const char*   filetypes[][3];
extern int           force, verbose, columnate;
extern const char*   footertext;
extern const char*   highlightcmd;
extern const char*   logoicon;
extern long long     maxcommits;
extern long long     maxfilesize;
extern const char*   pandoccmd;
extern const char*   pinfiles[];
extern const char*   sitedescription;
extern const char*   sitename;
extern int           splitdirectories;
extern const char*   stylesheet;
extern const char*   tagatomfile;

/* Methods */
int     bufferread(char* buffer, size_t len, const char* format, ...) FORMAT(3, 4);
char*   bufferreadmalloc(FILE* fp, size_t* pbuflen);
int     bufferwrite(const char* buffer, size_t len, const char* format, ...) FORMAT(3, 4);
ssize_t callcached(struct callcached_param* params);
int     endswith(const char* str, const char* suffix);
void    freediff(struct commitstats* ci);
int   getdiff(struct commitstats* ci, const struct repoinfo* info, git_commit* commit, int docache);
void  hprintf(FILE* file, const char* format, ...);
void  normalize_path(char* path);
char* parseconfig(FILE* file, struct config* keys);
void  printprogress(ssize_t indx, ssize_t ncommits, const char* what, ...) FORMAT(3, 4);
int   removedir(char* path);
const char* splitunit(ssize_t* size);
void        unhide_path(char* path);
void        vhprintf(FILE* file, const char* format, va_list args);
int   writearchive(const struct repoinfo* info, int type, const char* refname, git_commit* commit);
void  writeatomfooter(FILE* fp);
void  writeatomheader(FILE* fp, const struct repoinfo* info);
void  writecommitatom(FILE* fp, git_commit* commit, const char* tag);
void  writecommitfile(const struct repoinfo* info, git_commit* commit, const struct commitstats* ci,
                      int parentlink, const char* refname);
void  writefooter(FILE* fp);
int   writefiles(const struct repoinfo* info, const char* refname, git_commit* commit);
void  writeheader(FILE* fp, const struct repoinfo* info, int relpath, const char* name,
                  const char* description, ...);
void  writeindex(const char* destdir, char** repos, int nrepos);
void  writejsoncommit(FILE* fp, git_commit* commit, int first);
void  writejsonref(FILE* fp, const struct repoinfo* info, git_reference* ref, git_commit* commit);
int   writelog(const struct repoinfo* info, const char* refname, git_commit* commit);
void  writepreview(FILE* fp, const struct repoinfo* info, int relpath, struct blob* blob);
void  writerepo(struct indexinfo* indexinfo, const char* destination);
int   writerefs(FILE* fp, FILE* json, const struct repoinfo* info);
void  writeshortlog(FILE* fp, const struct repoinfo* info, git_commit* head);
FILE* xfopen(const char* mode, const char* format, ...) FORMAT(2, 3);
void  xmkdirf(int mode, const char* format, ...) FORMAT(2, 3);
