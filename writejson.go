package gitin

import (
	"encoding/json"
	"io"
	"time"

	git "github.com/jeffwelling/git2go/v37"
)

// JSON-vormen (bewust met pointers zodat ontbrekende waarden als null verschijnen)
type JSONSignature struct {
	Name  string    `json:"name"`
	Email string    `json:"email"`
	Date  time.Time `json:"date"`
}

type JSONCommit struct {
	ID        string         `json:"id"`
	Parent    *string        `json:"parent"`    // null als geen parent
	Author    *JSONSignature `json:"author"`    // null als onbekend
	Committer *JSONSignature `json:"committer"` // null als onbekend
	Summary   *string        `json:"summary"`   // null als leeg
	Message   *string        `json:"message"`   // null als leeg
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
		Date:  sig.When, // encoding/json -> RFC3339
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
		// sommige bindings geven "" i.p.v. nil; normaliseer:
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

// Vervanging voor writejsonrefs: schrijft netjes geformatteerde JSON.
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

// Vervanging voor writejsonrefs: schrijft netjes geformatteerde JSON.
func writejsoncommits(w io.Writer, c []JSONCommit) error {
	out := JSONOut{
		Commits: c,
	}
	enc := json.NewEncoder(w)
	enc.SetIndent("", "  ")
	return enc.Encode(out)
}
