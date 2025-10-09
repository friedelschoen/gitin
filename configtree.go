package gitin

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"html"
	"io"
	"sort"
	"strings"

	"github.com/pelletier/go-toml/v2"
	"gopkg.in/yaml.v3"
	"howett.net/plist"
)

var ErrFileType = errors.New("unsupported filetype")

func WriteConfigTree(w io.Writer, r io.Reader, fileType string) error {
	fileType = strings.ToLower(fileType)

	var data any
	var err error
	switch fileType {

	case "json":
		dec := json.NewDecoder(r)
		if err = dec.Decode(&data); err != nil {
			fail(w, err)
			return err
		}
	case "yaml", "yml":
		dec := yaml.NewDecoder(r)
		if err = dec.Decode(&data); err != nil {
			fail(w, err)
			return err
		}
	case "ini":
		data, err = parseINI(r)
		if err != nil {
			fail(w, err)
			return err
		}
	case "toml":
		dec := toml.NewDecoder(r)
		if err = dec.Decode(&data); err != nil {
			fail(w, err)
			return err
		}
	case "plist":
		data, err := io.ReadAll(r)
		if err != nil {
			fail(w, err)
			return err
		}
		if _, err = plist.Unmarshal(data, &data); err != nil {
			fail(w, err)
			return err
		}
	default:
		fail(w, err)
		return ErrFileType
	}

	writeHTML(w, data, 0)
	return nil
}

func fail(w io.Writer, err error) {
	fmt.Fprintf(w, "<p><i>Unable to parse config</i>: <code>%s</code></p>\n", html.EscapeString(err.Error()))
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

	case []any:
		fmt.Fprintf(w, "%s<ol>\n", ind)
		for _, it := range x {
			fmt.Fprintf(w, "%s    <li>\n", ind)
			writeHTML(w, it, indent+2)
			fmt.Fprintf(w, "%s    </li>\n", ind)
		}
		fmt.Fprintf(w, "%s</ol>\n", ind)

	default:
		fmt.Fprintf(w, "%s%s\n", ind, html.EscapeString(toString(x)))
	}
}

func toString(v any) string {
	switch t := v.(type) {
	case nil:
		return "<i>nil</i>"
	case string:
		return t
	case fmt.Stringer:
		return t.String()
	default:
		return fmt.Sprintf("%v", v)
	}
}
