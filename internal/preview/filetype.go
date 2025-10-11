package preview

import "strings"

type FileType struct {
	Pattern string
	Image   string
	Preview Previewer
	Param   string
}

func (ft FileType) Match(s string) bool {
	pre, suf, ok := strings.Cut(ft.Pattern, "*")
	if !ok {
		return s == ft.Pattern
	}
	return strings.HasPrefix(s, pre) && strings.HasSuffix(s, suf)
}
