// gitin-configtree.go
//
// Usage: gitin-configtree <yaml|ini|toml>
// Reads config from stdin and writes an HTML tree to stdout.
//
// Deps:
//
//	go get gopkg.in/yaml.v3
//	go get github.com/pelletier/go-toml/v2
package main

import (
	"bufio"
	"errors"
	"fmt"
	"html"
	"io"
	"os"
	"sort"
	"strings"

	"github.com/pelletier/go-toml/v2"
	"gopkg.in/yaml.v3"
)

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "Usage: gitin-configtree <yaml|ini|toml>")
		os.Exit(1)
	}

	fileType := strings.ToLower(os.Args[1])

	var data any
	var err error

	switch fileType {
	case "yaml":
		dec := yaml.NewDecoder(os.Stdin)
		if err = dec.Decode(&data); err != nil {
			fail(err)
			return
		}
	case "ini":
		data, err = parseINI(os.Stdin)
		if err != nil {
			fail(err)
			return
		}
	case "toml":
		// Read all from stdin (toml v2 needs a []byte or string)
		b, rerr := io.ReadAll(os.Stdin)
		if rerr != nil {
			fail(rerr)
			return
		}
		if err = toml.Unmarshal(b, &data); err != nil {
			fail(err)
			return
		}
	default:
		fmt.Println("<p>Unsupported file format</p>")
		return
	}

	writeHTML(os.Stdout, data, 0)
}

func fail(err error) {
	fmt.Fprintln(os.Stderr, err)
	fmt.Println("<p><i>Unable to parse config</i></p>")
}

// parseINI matches the Python version’s behavior:
// - ignores empty lines
// - ignores whole-line comments starting with ';' or '#'
// - requires a [section] before key=value pairs
// - no inline comments, no continued lines
func parseINI(r io.Reader) (map[string]map[string]string, error) {
	cfg := make(map[string]map[string]string)
	sc := bufio.NewScanner(r)

	var section string
	lineNo := 0

	for sc.Scan() {
		lineNo++
		line := sc.Text()

		// Handle whole-line comments first (like the Python startswith check)
		if strings.HasPrefix(line, ";") || strings.HasPrefix(line, "#") {
			line = ""
		}

		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// [section]
		if strings.HasPrefix(line, "[") && strings.HasSuffix(line, "]") {
			section = strings.TrimSpace(line[1 : len(line)-1])
			if section == "" {
				return nil, fmt.Errorf("line %d: empty section name", lineNo)
			}
			if _, ok := cfg[section]; !ok {
				cfg[section] = make(map[string]string)
			}
			continue
		}

		// key=value (only if we’re inside a section)
		if eq := strings.IndexByte(line, '='); eq >= 0 && section != "" {
			key := strings.TrimSpace(line[:eq])
			val := strings.TrimSpace(line[eq+1:])
			if key == "" {
				return nil, fmt.Errorf("line %d: empty key", lineNo)
			}
			cfg[section][key] = val
			continue
		}

		return nil, fmt.Errorf("line %d: %q is not valid INI format", lineNo, line)
	}

	if err := sc.Err(); err != nil {
		return nil, err
	}
	if len(cfg) == 0 {
		return nil, errors.New("no sections found")
	}
	return cfg, nil
}

func writeHTML(w io.Writer, v any, indent int) {
	ind := strings.Repeat("    ", indent)

	switch x := v.(type) {

	// YAML decodes into map[string]any; TOML v2 into map[string]any as well.
	case map[string]any:
		fmt.Fprintf(w, "%s<ul>\n", ind)
		// Stable order
		keys := make([]string, 0, len(x))
		for k := range x {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		for _, k := range keys {
			fmt.Fprintf(w, "%s    <li><b>%s:</b>\n", ind, html.EscapeString(k))
			writeHTML(w, x[k], indent+2)
			fmt.Fprintf(w, "%s    </li>\n", ind)
		}
		fmt.Fprintf(w, "%s</ul>\n", ind)

	// INI parser returns map[string]map[string]string
	case map[string]map[string]string:
		fmt.Fprintf(w, "%s<ul>\n", ind)
		sections := make([]string, 0, len(x))
		for s := range x {
			sections = append(sections, s)
		}
		sort.Strings(sections)
		for _, s := range sections {
			fmt.Fprintf(w, "%s    <li><b>%s:</b>\n", ind, html.EscapeString(s))
			// Convert inner map to map[string]any so we can reuse traversal
			inner := make(map[string]any, len(x[s]))
			for k, v := range x[s] {
				inner[k] = v
			}
			writeHTML(w, inner, indent+2)
			fmt.Fprintf(w, "%s    </li>\n", ind)
		}
		fmt.Fprintf(w, "%s</ul>\n", ind)

	// Slices can come as []any (TOML/YAML) or []interface{} (older libs)
	case []any:
		fmt.Fprintf(w, "%s<ol>\n", ind)
		for _, it := range x {
			fmt.Fprintf(w, "%s    <li>\n", ind)
			writeHTML(w, it, indent+2)
			fmt.Fprintf(w, "%s    </li>\n", ind)
		}
		fmt.Fprintf(w, "%s</ol>\n", ind)

	// Leaf: print scalar as text, escaped
	default:
		fmt.Fprintf(w, "%s%s\n", ind, html.EscapeString(toString(x)))
	}
}

func toString(v any) string {
	switch t := v.(type) {
	case nil:
		return "null"
	case string:
		return t
	case fmt.Stringer:
		return t.String()
	default:
		return fmt.Sprintf("%v", v)
	}
}
