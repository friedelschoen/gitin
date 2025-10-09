package gitin

// #include "path.h"

// #include <string.h>

func pathunhide(path string) string { // void pathunhide(char* path) {
	runes := []rune(path)
	for i, chr := range runes { // for (char* chr = path; *chr; chr++) {
		if chr == '.' && (i == 0 || runes[i-1] == '/' && runes[i+1] != '/') { // if (*chr == '.' && (chr == path || chr[-1] == '/') && chr[1] != '/')
			runes[i] = '-' // *chr = '-';
		}
	}
	return string(runes)
}
