package gitin

import (
	"encoding/json"
	"io"
	"time"

	git "github.com/jeffwelling/git2go/v37"
)

type JSONSignature struct {
	Name  string    `json:"name"`
	Email string    `json:"email"`
	Date  time.Time `json:"date"`
}

type JSONCommit struct {
	ID        string         `json:"id"`
	Parent    *string        `json:"parent"`
	Author    *JSONSignature `json:"author"`
	Committer *JSONSignature `json:"committer"`
	Summary   *string        `json:"summary"`
	Message   *string        `json:"message"`
}

type JSONRef struct {
	Name   string     `json:"name"`
	Commit JSONCommit `json:"commit"`
}

type JSONOut struct {
	Branches []JSONRef    `json:"branches,omitempty"`
	Tags     []JSONRef    `json:"tags,omitempty"`
	Commits  []JSONCommit `json:"commits,omitempty"`
}

func toJSONSignature(sig *git.Signature) *JSONSignature {
	if sig == nil {
		return nil
	}
	return &JSONSignature{
		Name:  sig.Name,
		Email: sig.Email,
		Date:  sig.When,
	}
}

func strPtrNonEmpty(s string) *string {
	if s == "" {
		return nil
	}
	return &s
}

func toJSONCommit(c *git.Commit) JSONCommit {
	id := c.Id().String()

	var parent *string
	if c.ParentCount() > 0 {
		p := c.ParentId(0).String()

		if p != "" {
			parent = &p
		}
	}

	return JSONCommit{
		ID:        id,
		Parent:    parent,
		Author:    toJSONSignature(c.Author()),
		Committer: toJSONSignature(c.Committer()),
		Summary:   strPtrNonEmpty(c.Summary()),
		Message:   strPtrNonEmpty(c.Message()),
	}
}

func writejsonrefs(w io.Writer, info *repoinfo) error {
	out := JSONOut{
		Branches: make([]JSONRef, 0, len(info.refs)),
		Tags:     make([]JSONRef, 0),
	}
	for _, r := range info.refs {
		if r == nil || r.commit == nil {
			continue
		}
		jr := JSONRef{
			Name:   r.refname,
			Commit: toJSONCommit(r.commit),
		}
		if r.istag {
			out.Tags = append(out.Tags, jr)
		} else {
			out.Branches = append(out.Branches, jr)
		}
	}

	enc := json.NewEncoder(w)
	enc.SetIndent("", "  ")
	return enc.Encode(out)
}

func writejsoncommits(w io.Writer, c []JSONCommit) error {
	out := JSONOut{
		Commits: c,
	}
	enc := json.NewEncoder(w)
	enc.SetIndent("", "  ")
	return enc.Encode(out)
}
