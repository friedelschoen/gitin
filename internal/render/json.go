package render

import (
	"encoding/json"
	"io"

	"github.com/friedelschoen/gitin-go/internal/wrapper"
)

type refJSON struct {
	Name   string `json:"name"`
	Commit string `json:"commit"`
}

type outJSON struct {
	Branches []refJSON             `json:"branches,omitempty"`
	Tags     []refJSON             `json:"tags,omitempty"`
	Commits  []*wrapper.CommitInfo `json:"commits,omitempty"`
}

func WriteJSONRefs(w io.Writer, refs []*wrapper.ReferenceInfo) error {
	out := outJSON{
		Branches: make([]refJSON, 0, len(refs)),
		Tags:     make([]refJSON, 0),
	}
	for _, r := range refs {
		if r == nil || r.Commit == nil {
			continue
		}
		jr := refJSON{
			Name:   r.Refname,
			Commit: r.Commit.Id().String(),
		}
		if r.IsTag {
			out.Tags = append(out.Tags, jr)
		} else {
			out.Branches = append(out.Branches, jr)
		}
	}

	enc := json.NewEncoder(w)
	enc.SetIndent("", "  ")
	return enc.Encode(out)
}
