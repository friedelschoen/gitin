#pragma once

#include "parseconfig.h"

/* Enums */
enum {
	ArchiveTarGz  = 1 << 0,
	ArchiveTarXz  = 1 << 1,
	ArchiveTarBz2 = 1 << 2,
	ArchiveZip    = 1 << 3,
};

extern const char*       archiveexts[];
extern int               archivetypes;
extern int               archivezip;
extern int               archivetarbz2;
extern int               archivetargz;
extern int               archivetarxz;
extern size_t            autofilelimit;
extern struct configitem configkeys[];
extern const char*       colorscheme;
extern const char*       configfile;
extern const char*       configtreecmd;
extern const char*       extrapinfiles;
extern const char*       favicon;
extern const char*       favicontype;
extern const char*       filetypes[][3];
extern int               force, verbose, columnate;
extern const char*       footertext;
extern const char*       highlightcmd;
extern const char*       logoicon;
extern long long         maxcommits;
extern long long         maxfilesize;
extern const char*       pandoccmd;
extern const char*       pinfiles[];
extern const char*       sitedescription;
extern const char*       sitename;
extern int               splitdirectories;
extern const char*       stylesheet;
extern const char*       tagatomfile;
