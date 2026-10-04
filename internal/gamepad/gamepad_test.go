package gamepad

import "testing"

func TestDecodeEventLayout(t *testing.T) {
	// A raw 24-byte input_event as the kernel writes it on 64-bit Linux.
	raw := []byte{
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, // sec
		0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, // usec
		0x01, 0x00, // EV_KEY
		0x30, 0x01, // code 0x130 = BTN_SOUTH
		0x01, 0x00, 0x00, 0x00, // value 1 = press
	}
	ev := decodeEvent(raw)
	if ev.Typ != 1 {
		t.Errorf("type = %d, want 1 (EV_KEY)", ev.Typ)
	}
	if ev.Code != codeButtonSouth {
		t.Errorf("code = %#x, want %#x", ev.Code, codeButtonSouth)
	}
	if ev.Val != keyPress {
		t.Errorf("value = %d, want %d", ev.Val, keyPress)
	}
	// The first eight bytes are the seconds field, read little-endian.
	var wantSec int64
	for i := 7; i >= 0; i-- {
		wantSec = wantSec<<8 | int64(raw[i])
	}
	if ev.Sec != wantSec {
		t.Errorf("sec = %#x, want %#x", ev.Sec, wantSec)
	}
}

func TestEventSizeMatchesKernelABI(t *testing.T) {
	// struct input_event is timeval(16) + u16 + u16 + s32 = 24 bytes.
	if eventSize != 24 {
		t.Errorf("eventSize = %d, want 24", eventSize)
	}
}

func TestBitInRange(t *testing.T) {
	// BTN_SOUTH is code 0x130 = 304, so bit 304 of a bitmap starting at 0,
	// which lands in byte 38.
	bitmap := make([]byte, 96)
	bitmap[304/8] |= 1 << (304 % 8)
	if !bitInRange(bitmap, codeButtonSouth, 0) {
		t.Error("BTN_SOUTH should be reported set")
	}
	if bitInRange(bitmap, codeButtonEast, 0) {
		t.Error("BTN_EAST should be reported unset")
	}
	// A code before the bitmap's start is not a bit in it.
	if bitInRange(bitmap, 2, 3) {
		t.Error("a code before the first should not be reported set")
	}
	// A code beyond the buffer is treated as unset rather than panicking.
	if bitInRange(bitmap, 4096, 0) {
		t.Error("a code beyond the buffer should be reported unset")
	}
}

func TestStickDirectionDeadzone(t *testing.T) {
	cases := []struct {
		axis  string
		value float64
		want  Button
	}{
		{"ly", -0.9, DirUp},
		{"ly", 0.9, DirDown},
		{"ly", 0.2, ""},
		{"lx", 0.9, DirRight},
		{"lx", -0.9, DirLeft},
		{"lx", 0.2, ""},
		{"rx", 0.9, ""},
		{"ry", -0.9, ""},
	}
	for _, c := range cases {
		if got := stickDirection(c.axis, c.value); got != c.want {
			t.Errorf("stickDirection(%q, %v) = %q, want %q", c.axis, c.value, got, c.want)
		}
	}
}

func TestCodeButtonMappingMatchesFaceButtons(t *testing.T) {
	// The four face buttons must map to the names the shell shows as prompts.
	// A, B, X and Y follow the common pad convention where BTN_SOUTH is A.
	want := map[uint16]Button{
		codeButtonSouth: ButtonA,
		codeButtonEast:  ButtonB,
		codeButtonWest:  ButtonX,
		codeButtonNorth: ButtonY,
	}
	for code, button := range want {
		if codeButtons[code] != button {
			t.Errorf("code %#x maps to %q, want %q", code, codeButtons[code], button)
		}
	}
}

func TestTriggerAxesAreRecognised(t *testing.T) {
	if !isTriggerAxis("lz") || !isTriggerAxis("rz") {
		t.Error("ABS_Z and ABS_RZ must be treated as triggers")
	}
	if isTriggerAxis("lx") || isTriggerAxis("ly") {
		t.Error("stick axes must not be treated as triggers")
	}
}

func TestNormaliseUsesReportedRange(t *testing.T) {
	d := &device{
		axes: map[uint16]string{0x00: "lx", 0x01: "ly", 0x02: "lz"},
		ranges: map[uint16]absInfo{
			0x00: {Min: -32768, Max: 32767},
			0x01: {Min: -32768, Max: 32767},
			0x02: {Min: 0, Max: 255},
		},
	}
	if got := d.normalise(0x00, 0); got < -0.01 || got > 0.01 {
		t.Errorf("stick centre = %v, want ~0", got)
	}
	if got := d.normalise(0x00, 32767); got < 0.99 {
		t.Errorf("stick right = %v, want ~1", got)
	}
	// The kernel reports Y with its minimum at the top, so up must be negative.
	if got := d.normalise(0x01, -32768); got < 0.99 {
		t.Errorf("stick up = %v, want ~1 (Y is inverted)", got)
	}
	// Triggers rest at their minimum and read 0 at rest, 1 fully pressed.
	if got := d.normalise(0x02, 0); got != 0 {
		t.Errorf("trigger rest = %v, want 0", got)
	}
	if got := d.normalise(0x02, 255); got < 0.99 {
		t.Errorf("trigger pressed = %v, want ~1", got)
	}
}

func TestNormaliseClampsOutOfRange(t *testing.T) {
	d := &device{
		axes:   map[uint16]string{0x00: "lx"},
		ranges: map[uint16]absInfo{0x00: {Min: -100, Max: 100}},
	}
	if got := d.normalise(0x00, 500); got != 1 {
		t.Errorf("over-range value = %v, want 1", got)
	}
	if got := d.normalise(0x00, -500); got != -1 {
		t.Errorf("under-range value = %v, want -1", got)
	}
}

func TestNormaliseWithoutReportedRange(t *testing.T) {
	d := &device{axes: map[uint16]string{0x00: "lx"}, ranges: map[uint16]absInfo{}}
	if got := d.normalise(0x00, 12345); got != 0 {
		t.Errorf("value with no reported range = %v, want 0", got)
	}
}

func TestReaderIgnoresNonControllerFiles(t *testing.T) {
	// A directory with no event nodes must not produce readers or events.
	r := newReader(t.TempDir(), 0)
	r.scan()
	select {
	case ev := <-r.Events():
		t.Fatalf("unexpected event %+v from an empty device directory", ev)
	default:
	}
	if err := r.Close(); err != nil {
		t.Errorf("Close: %v", err)
	}
}

func TestCloseIsIdempotent(t *testing.T) {
	r := newReader(t.TempDir(), 0)
	if err := r.Close(); err != nil {
		t.Fatalf("first Close: %v", err)
	}
	if err := r.Close(); err != nil {
		t.Errorf("second Close: %v", err)
	}
}

func TestEventsChannelClosesOnClose(t *testing.T) {
	r := newReader(t.TempDir(), 0)
	if err := r.Close(); err != nil {
		t.Fatalf("Close: %v", err)
	}
	if _, open := <-r.Events(); open {
		t.Error("event channel should be closed after Close")
	}
}

func TestIOCEncoding(t *testing.T) {
	// EVIOCGNAME(128) must encode direction read, type 'E', nr 0x06, size 128,
	// per _IOC(dir, type, nr, size) = dir<<30 | size<<16 | type<<8 | nr.
	want := uintptr(2)<<30 | uintptr(128)<<16 | uintptr('E')<<8 | 0x06
	if got := eviocgname(128); got != want {
		t.Errorf("eviocgname(128) = %#x, want %#x", got, want)
	}
	// EVIOCPLAY is _IOWR('E', 0x84, int) upstream, so the direction is
	// read|write (3) and the size is the kernel's int, which is 4 bytes even on
	// a 64-bit host where Go's int is 8.
	rwWant := uintptr(3)<<30 | uintptr(4)<<16 | uintptr('E')<<8 | 0x84
	if got := eviocplay(); got != rwWant {
		t.Errorf("eviocplay() = %#x, want %#x", got, rwWant)
	}
	// EVIOCGABS folds the axis code into the request number, not the size.
	absWant := uintptr(2)<<30 | uintptr(24)<<16 | uintptr('E')<<8 | (0x40 + 0x01)
	if got := eviocgabs(0x01); got != absWant {
		t.Errorf("eviocgabs(1) = %#x, want %#x", got, absWant)
	}
}
