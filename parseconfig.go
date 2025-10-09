package gitin

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"iter"
	"maps"
	"os"
	"reflect"
	"strconv"
	"strings"
	"time"
)

func UnmarshalConf(data map[string]string, prefix string, out any) error {
	v := reflect.ValueOf(out)
	if v.Kind() != reflect.Ptr || v.IsNil() {
		return errors.New("output must be a non-nil pointer to a struct")
	}

	v = v.Elem()
	if v.Kind() != reflect.Struct {
		return errors.New("output must point to a struct")
	}

	t := v.Type()
	for i := range t.NumField() {
		field := t.Field(i)
		tag := field.Tag.Get("conf")
		var valStr string
		for field := range strings.FieldsSeq(tag) {
			var ok bool
			field, ok = strings.CutPrefix(field, prefix+".")
			if prefix != "" {
				if !ok {
					continue
				}
			}

			valStr, ok = data[tag]
			if !ok {
				continue // Key not found, skip
			}
		}
		if valStr == "" {
			continue
		}

		fv := v.Field(i)
		if !fv.CanSet() {
			continue
		}

		// Handle time.Duration explicitly
		if field.Type == reflect.TypeOf(time.Duration(0)) {
			dur, err := time.ParseDuration(valStr)
			if err != nil {
				return fmt.Errorf("invalid duration for %q: %w", tag, err)
			}
			fv.Set(reflect.ValueOf(dur))
			continue
		}

		// Parse built-in kinds
		switch fv.Kind() {
		case reflect.String:
			fv.SetString(valStr)
		case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
			n, err := strconv.ParseInt(valStr, 10, 64)
			if err != nil {
				return fmt.Errorf("invalid int for %q: %w", tag, err)
			}
			fv.SetInt(n)
		case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
			n, err := strconv.ParseUint(valStr, 10, 64)
			if err != nil {
				return fmt.Errorf("invalid uint for %q: %w", tag, err)
			}
			fv.SetUint(n)
		case reflect.Float32, reflect.Float64:
			f, err := strconv.ParseFloat(valStr, 64)
			if err != nil {
				return fmt.Errorf("invalid float for %q: %w", tag, err)
			}
			fv.SetFloat(f)
		case reflect.Bool:
			switch strings.ToLower(valStr) {
			case "yes", "y", "true":
				fv.SetBool(true)
			case "no", "n", "false":
				fv.SetBool(false)
			default:
				return fmt.Errorf("invalid bool for %q: %s", tag, valStr)
			}
		default:
			return fmt.Errorf("unsupported field type %s for key %q", fv.Type(), tag)
		}
	}

	return nil
}

func trimComment(line string) string {
	inQuote := false
	for i, chr := range line {
		if chr == '"' {
			inQuote = !inQuote
		}
		if !inQuote && strings.ContainsRune(";#", chr) {
			return line[:i]
		}
	}
	return line
}

func ParseConfig(file io.Reader, filename string) iter.Seq2[string, map[string]string] {
	return func(yield func(string, map[string]string) bool) {
		scan := bufio.NewScanner(file)
		current := make(map[string]string)
		var base map[string]string
		var section string
		var linenr int
		for scan.Scan() {
			line := scan.Text()
			linenr++

			line = strings.TrimSpace(trimComment(line))
			if len(line) == 0 {
				continue
			}

			if line[0] == '[' {
				end := strings.IndexByte(line, ']')
				if end != len(line)-1 {
					fmt.Fprintf(os.Stderr, "%s:%d: garbage found after `]`: %s\n", filename, linenr, line[end:])
				}
				newsection := strings.TrimSpace(line[1:end])
				if len(newsection) == 0 {
					fmt.Fprintf(os.Stderr, "%s:%d: section is empty\n", filename, linenr)
					continue
				}
				if section == "" {
					base = current
				} else if !yield(section, current) {
					return
				}

				section = newsection
				current = maps.Clone(base)
				continue
			}

			key, value, ok := strings.Cut(line, "=")
			if !ok {
				fmt.Fprintf(os.Stderr, "%s:%d: not a key-value pair: %s\n", filename, linenr, line)
				continue
			}
			key = strings.TrimSpace(key)
			if len(key) == 0 {
				fmt.Fprintf(os.Stderr, "%s:%d: key is empty\n", filename, linenr)
				continue
			}
			value = strings.TrimSpace(value)
			if len(value) >= 2 && value[0] == '"' && value[len(value)-1] == '"' {
				value = value[1 : len(value)-1]
			}
			current[key] = value
		}
		if len(section) > 0 {
			yield(section, current)
		}
	}
}
