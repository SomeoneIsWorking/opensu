// Package vdf parses Valve's KeyValues text format, the shape used by Steam's
// app manifests, library folders and per-user configuration.
package vdf

import (
	"errors"
	"fmt"
	"sort"
	"strconv"
	"strings"
)

// Map is a parsed KeyValues document. A value is either a string or a nested
// Map. Steam writes keys with inconsistent casing between files, so lookups are
// case-insensitive while the original keys are preserved for iteration.
type Map map[string]any

// ErrSyntax reports a document that is not valid KeyValues text.
var ErrSyntax = errors.New("vdf: malformed input")

// Parse reads a KeyValues document.
func Parse(src string) (Map, error) {
	l := &lexer{src: []byte(src)}
	return l.parseObject(true)
}

// Keys returns the document's keys in sorted order so callers that iterate
// entries get a reproducible sequence.
func (m Map) Keys() []string {
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	return keys
}

// Walk returns the raw value at the given key path.
func (m Map) Walk(keys ...string) (any, bool) {
	var cur any = m
	for _, key := range keys {
		node, ok := cur.(Map)
		if !ok {
			return nil, false
		}
		v, ok := node.lookup(key)
		if !ok {
			return nil, false
		}
		cur = v
	}
	return cur, true
}

// Node walks the given key path and returns the nested Map found there.
func (m Map) Node(keys ...string) (Map, bool) {
	v, ok := m.Walk(keys...)
	if !ok {
		return nil, false
	}
	child, ok := v.(Map)
	return child, ok
}

// Has reports whether the given key path exists, whatever its value.
func (m Map) Has(keys ...string) bool {
	_, ok := m.Walk(keys...)
	return ok
}

// Get returns the string at the given key path.
func (m Map) Get(keys ...string) (string, bool) {
	v, ok := m.Walk(keys...)
	if !ok {
		return "", false
	}
	s, ok := v.(string)
	return s, ok
}

// GetInt returns the integer at the given key path, and whether the value was
// present and parseable.
func (m Map) GetInt(keys ...string) (int64, bool) {
	s, ok := m.Get(keys...)
	if !ok {
		return 0, false
	}
	n, err := strconv.ParseInt(strings.TrimSpace(s), 10, 64)
	if err != nil {
		return 0, false
	}
	return n, true
}

func (m Map) lookup(key string) (any, bool) {
	if v, ok := m[key]; ok {
		return v, true
	}
	for k, v := range m {
		if strings.EqualFold(k, key) {
			return v, true
		}
	}
	return nil, false
}

// lexer walks the document with an explicit offset. An offset rather than a
// pushback reader keeps token classification and token consumption separate
// without depending on read/unread discipline.
type lexer struct {
	src []byte
	pos int
}

// parseObject parses either a whole document or one nested block.
func (l *lexer) parseObject(top bool) (Map, error) {
	out := Map{}
	for {
		l.skipSpace()
		if l.pos >= len(l.src) {
			if top {
				return out, nil
			}
			return nil, fmt.Errorf("%w: unterminated block", ErrSyntax)
		}
		if l.src[l.pos] == '}' {
			if top {
				return nil, fmt.Errorf("%w: unexpected close brace", ErrSyntax)
			}
			l.pos++
			return out, nil
		}
		if l.src[l.pos] == '{' {
			return nil, fmt.Errorf("%w: unexpected open brace", ErrSyntax)
		}

		key, err := l.readToken()
		if err != nil {
			return nil, err
		}

		l.skipSpace()
		if l.pos < len(l.src) && l.src[l.pos] == '{' {
			l.pos++
			child, err := l.parseObject(false)
			if err != nil {
				return nil, err
			}
			out[key] = child
			continue
		}
		// A key with no value is legal; record it as empty so callers testing
		// for presence rather than content still find it.
		if l.pos < len(l.src) && (l.src[l.pos] != '}' && l.src[l.pos] != '{') {
			value, err := l.readToken()
			if err != nil {
				return nil, err
			}
			out[key] = value
			continue
		}
		out[key] = ""
	}
}

// skipSpace advances past whitespace and comments.
func (l *lexer) skipSpace() {
	for l.pos < len(l.src) {
		c := l.src[l.pos]
		if c == ' ' || c == '\t' || c == '\r' || c == '\n' {
			l.pos++
			continue
		}
		// "//" starts a comment that runs to end of line.
		if c == '/' && l.pos+1 < len(l.src) && l.src[l.pos+1] == '/' {
			for l.pos < len(l.src) && l.src[l.pos] != '\n' {
				l.pos++
			}
			continue
		}
		return
	}
}

// readToken reads one quoted or bare token, leaving the offset just past it.
func (l *lexer) readToken() (string, error) {
	l.skipSpace()
	if l.pos >= len(l.src) {
		return "", fmt.Errorf("%w: expected token", ErrSyntax)
	}
	if l.src[l.pos] == '"' {
		return l.readQuoted()
	}
	start := l.pos
	for l.pos < len(l.src) && !isDelim(l.src[l.pos]) {
		l.pos++
	}
	if l.pos == start {
		return "", fmt.Errorf("%w: empty token", ErrSyntax)
	}
	return string(l.src[start:l.pos]), nil
}

func (l *lexer) readQuoted() (string, error) {
	l.pos++ // opening quote
	var sb strings.Builder
	for l.pos < len(l.src) {
		c := l.src[l.pos]
		switch c {
		case '"':
			l.pos++
			return sb.String(), nil
		case '\\':
			if l.pos+1 >= len(l.src) {
				return "", fmt.Errorf("%w: unterminated escape", ErrSyntax)
			}
			esc := l.src[l.pos+1]
			l.pos += 2
			switch esc {
			case 'n':
				sb.WriteByte('\n')
			case 't':
				sb.WriteByte('\t')
			case '\\', '"':
				sb.WriteByte(esc)
			default:
				// Unknown escapes keep both characters, which is what a path
				// containing an unrecognised escape needs.
				sb.WriteByte('\\')
				sb.WriteByte(esc)
			}
		default:
			sb.WriteByte(c)
			l.pos++
		}
	}
	return "", fmt.Errorf("%w: unterminated string", ErrSyntax)
}

func isDelim(c byte) bool {
	switch c {
	case ' ', '\t', '\r', '\n', '{', '}', '"':
		return true
	}
	return false
}
