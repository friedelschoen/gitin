package writer

import (
	"encoding/xml"
	"fmt"
	"io"
	"time"

	"github.com/friedelschoen/gitin-go/internal/wrapper"
	git "github.com/jeffwelling/git2go/v37"
)

const atomNS = "http://www.w3.org/2005/Atom"

type atomFeed struct {
	XMLName  xml.Name    `xml:"feed"`
	XMLNS    string      `xml:"xmlns,attr"`
	Title    string      `xml:"title"`
	Subtitle string      `xml:"subtitle,omitempty"`
	Entries  []atomEntry `xml:"entry"`
}

type atomEntry struct {
	ID        string      `xml:"id"`
	Published string      `xml:"published,omitempty"`
	Title     string      `xml:"title,omitempty"`
	Link      atomLink    `xml:"link"`
	Author    *atomPerson `xml:"author,omitempty"`
	Content   atomContent `xml:"content"`
}

type atomLink struct {
	Rel  string `xml:"rel,attr,omitempty"`
	Type string `xml:"type,attr,omitempty"`
	Href string `xml:"href,attr"`
}

type atomPerson struct {
	Name  string `xml:"name,omitempty"`
	Email string `xml:"email,omitempty"`
}

type atomContent struct {
	Type string `xml:"type,attr,omitempty"`
	Body string `xml:",chardata"`
}

func rfc3339(t time.Time) string {
	if t.IsZero() {
		return ""
	}

	return t.UTC().Format(time.RFC3339)
}

func makeEntry(commit *git.Commit, refname string) atomEntry {
	author := commit.Author()
	committer := commit.Committer()
	summary := commit.Summary()
	message := commit.Message()
	oid := commit.Id().String()

	var parent string
	if parentcommit := commit.ParentId(0); parentcommit != nil {
		parent = parentcommit.String()
	}

	var published string
	if author != nil {
		published = rfc3339(author.When)
	} else if committer != nil {
		published = rfc3339(committer.When)
	}

	title := summary
	if refname != "" && summary != "" {
		title = fmt.Sprintf("[%s] %s", refname, summary)
	}

	var body string
	body += fmt.Sprintf("commit %s\n", oid)
	if parent != "" {
		body += fmt.Sprintf("parent %s\n", parent)
	}
	if author != nil {
		body += fmt.Sprintf("Author: %s <%s>\n", author.Name, author.Email)
		body += fmt.Sprintf("Date:   %s\n", rfc3339(author.When))
	}
	if message != "" {
		body += "\n" + message
	}

	var person *atomPerson
	if author != nil {
		person = &atomPerson{Name: author.Name, Email: author.Email}
	}

	return atomEntry{
		ID:        oid,
		Published: published,
		Title:     title,
		Link: atomLink{
			Rel:  "alternate",
			Type: "text/html",
			Href: fmt.Sprintf("commit/%s.html", oid),
		},
		Author:  person,
		Content: atomContent{Type: "text", Body: body},
	}
}

func writeatomrefs(w io.Writer, info *wrapper.RepoInfo) error {
	var entries []atomEntry
	for _, r := range info.Refs {
		entries = append(entries, makeEntry(r.Commit, r.Refname))
	}
	return writeatomfeed(w, info, entries)
}

func writeatomfeed(w io.Writer, info *wrapper.RepoInfo, entries []atomEntry) error {
	feed := atomFeed{
		XMLNS:    atomNS,
		Title:    fmt.Sprintf("%s, branch HEAD", info.Name),
		Subtitle: info.Description,
		Entries:  entries,
	}

	enc := xml.NewEncoder(w)
	enc.Indent("", "  ")

	if _, err := io.WriteString(w, xml.Header); err != nil {
		return err
	}

	if err := enc.Encode(feed); err != nil {
		return err
	}
	return enc.Flush()
}
