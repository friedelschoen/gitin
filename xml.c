#include "xml.h"

/* Percent-encode, see RFC3986 section 2.1. */
void
percentencode(FILE *fp, const char *s, size_t len)
{
	static char tab[] = "0123456789ABCDEF";
	unsigned char uc;
	size_t i;

	for (i = 0; *s && i < len; s++, i++) {
		uc = *s;
		/* NOTE: do not encode '/' for paths or ",-." */
		if (uc < ',' || uc >= 127 || (uc >= ':' && uc <= '@') ||
		    uc == '[' || uc == ']') {
			putc('%', fp);
			putc(tab[(uc >> 4) & 0x0f], fp);
			putc(tab[uc & 0x0f], fp);
		} else {
			putc(uc, fp);
		}
	}
}

/* Escape characters below as HTML 2.0 / XML 1.0. */
void
xmlencode(FILE *fp, const char *s, size_t len)
{
	size_t i;

	for (i = 0; *s && i < len; s++, i++) {
		switch(*s) {
		case '<':  fputs("&lt;",   fp); break;
		case '>':  fputs("&gt;",   fp); break;
		case '\'': fputs("&#39;",  fp); break;
		case '&':  fputs("&amp;",  fp); break;
		case '"':  fputs("&quot;", fp); break;
		default:   putc(*s, fp);
		}
	}
}

/* Escape characters below as HTML 2.0 / XML 1.0, ignore printing '\r', '\n' */
void
xmlencodeline(FILE *fp, const char *s, size_t len)
{
	size_t i;

	for (i = 0; *s && i < len; s++, i++) {
		switch(*s) {
		case '<':  fputs("&lt;",   fp); break;
		case '>':  fputs("&gt;",   fp); break;
		case '\'': fputs("&#39;",  fp); break;
		case '&':  fputs("&amp;",  fp); break;
		case '"':  fputs("&quot;", fp); break;
		case '\r': break; /* ignore CR */
		case '\n': break; /* ignore LF */
		default:   putc(*s, fp);
		}
	}
}
