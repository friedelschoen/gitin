package writer

import (
	"encoding/json"
	"io"
	"time"

	"github.com/friedelschoen/gitin-go/internal/wrapper"
	git "github.com/jeffwelling/git2go/v37"
)

type signatureJSON struct {
	Name  string    `json:"name"`
	Email string    `json:"email"`
	Date  time.Time `json:"date"`
}

type commitJSON struct {
	ID        string         `json:"id"`
	Parent    *string        `json:"parent"`
	Author    *signatureJSON `json:"author"`
	Committer *signatureJSON `json:"committer"`
	Summary   *string        `json:"summary"`
	Message   *string        `json:"message"`
}

type refJSON struct {
	Name   string     `json:"name"`
	Commit commitJSON `json:"commit"`
}

type outJSON struct {
	Branches []refJSON    `json:"branches,omitempty"`
	Tags     []refJSON    `json:"tags,omitempty"`
	Commits  []commitJSON `json:"commits,omitempty"`
}

func toJSONSignature(sig *git.Signature) *signatureJSON {
	if sig == nil {
		return nil
	}
	return &signatureJSON{
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

func toJSONCommit(c *git.Commit) commitJSON {
	id := c.Id().String()

	var parent *string
	if c.ParentCount() > 0 {
		p := c.ParentId(0).String()

		if p != "" {
			parent = &p
		}
	}

	return commitJSON{
		ID:        id,
		Parent:    parent,
		Author:    toJSONSignature(c.Author()),
		Committer: toJSONSignature(c.Committer()),
		Summary:   strPtrNonEmpty(c.Summary()),
		Message:   strPtrNonEmpty(c.Message()),
	}
}

func writejsonrefs(w io.Writer, info *wrapper.RepoInfo) error {
	out := outJSON{
		Branches: make([]refJSON, 0, len(info.Refs)),
		Tags:     make([]refJSON, 0),
	}
	for _, r := range info.Refs {
		if r == nil || r.Commit == nil {
			continue
		}
		jr := refJSON{
			Name:   r.Refname,
			Commit: toJSONCommit(r.Commit),
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

func writejsoncommits(w io.Writer, c []commitJSON) error {
	out := outJSON{
		Commits: c,
	}
	enc := json.NewEncoder(w)
	enc.SetIndent("", "  ")
	return enc.Encode(out)
}
