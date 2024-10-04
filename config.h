#pragma once

#include "parseconfig.h"

extern struct config config_keys[];

extern const char* colorscheme;
extern const char* favicon;
extern const char* favicontype;
extern const char* footertext;
extern const char* highlightcmd;
extern const char* logoicon;
extern const char* pinfiles[];
extern const char* extrapinfiles;
extern const char* sitedescription;
extern const char* sitename;
extern const char* stylesheet;
extern long long   maxcommits;
extern long long   maxfilesize;
extern int         filesperdirectory;

extern const char* configfile;
extern const char* jsonfile;
extern const char* commitatomfile;
extern const char* tagatomfile;

extern const char* copystylesheet;
extern const char* copyfavicon;
extern const char* copylogoicon;

extern int force, verbose, columnate;
