package gitin

func pathunhide(path string) string {
	runes := []rune(path)
	for i, chr := range runes {
		if chr == '.' && (i == 0 || runes[i-1] == '/' && runes[i+1] != '/') {
			runes[i] = '-'
		}
	}
	return string(runes)
}
