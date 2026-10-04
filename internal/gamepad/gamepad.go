package gamepad

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"syscall"
	"time"
	"unsafe"

	"golang.org/x/sys/unix"
)

// Button is a named control. The shell maps these to on-screen prompts, so the
// names are presentation-facing rather than kernel-facing.
type Button string

const (
	ButtonA      Button = "a"
	ButtonB      Button = "b"
	ButtonX      Button = "x"
	ButtonY      Button = "y"
	ButtonL1     Button = "l1"
	ButtonR1     Button = "r1"
	ButtonL2     Button = "l2"
	ButtonR2     Button = "r2"
	ButtonL3     Button = "l3"
	ButtonR3     Button = "r3"
	ButtonSelect Button = "select"
	ButtonStart  Button = "start"
	ButtonGuide  Button = "guide"
)

// Dpad directions, taken either from the hat or synthesised from the left
// stick so the shell sees both as the same control.
const (
	DirUp    Button = "up"
	DirDown  Button = "down"
	DirLeft  Button = "left"
	DirRight Button = "right"
)

// EventKind distinguishes the states the shell reacts to.
type EventKind string

const (
	KindButton       EventKind = "button"
	KindAxis         EventKind = "axis"
	KindConnected    EventKind = "connected"
	KindDisconnected EventKind = "disconnected"
)

// Event is one input state change.
type Event struct {
	Kind    EventKind `json:"kind"`
	Device  string    `json:"device"`
	Button  Button    `json:"button,omitempty"`
	Pressed bool      `json:"pressed,omitempty"`
	// Axis is "lx", "ly", "rx" or "ry".
	Axis string `json:"axis,omitempty"`
	// Value is normalised to -1..1 for sticks and 0..1 for triggers.
	Value float64 `json:"value,omitempty"`
}

// ErrNoRumble reports that a controller exposes no force feedback.
var ErrNoRumble = errors.New("gamepad: controller has no force feedback")

// Reader watches every controller under a device directory and publishes state
// changes on a channel. It is safe for concurrent use.
type Reader struct {
	events chan Event
	devDir string
	// rescan is how often a missing controller is looked for again.
	rescan time.Duration
	// stopped is closed by Close so watchers return instead of polling.
	stopped chan struct{}

	mu      sync.Mutex
	closed  bool
	devices map[string]*device
	// primary is the device path of the most recently connected pad.
	primary string
	wg      sync.WaitGroup
}

// NewReader creates a reader over the real input devices.
func NewReader() *Reader { return newReader("/dev/input", 2*time.Second) }

func newReader(devDir string, rescan time.Duration) *Reader {
	if rescan <= 0 {
		rescan = 2 * time.Second
	}
	return &Reader{
		events:  make(chan Event, 64),
		devDir:  devDir,
		rescan:  rescan,
		stopped: make(chan struct{}),
		devices: map[string]*device{},
	}
}

// Events returns the channel state changes are published on. It is closed when
// the reader is closed.
func (r *Reader) Events() <-chan Event { return r.events }

// Close stops watching and closes the event channel.
func (r *Reader) Close() error {
	r.mu.Lock()
	if r.closed {
		r.mu.Unlock()
		return nil
	}
	r.closed = true
	close(r.stopped)
	devices := make([]*device, 0, len(r.devices))
	for _, d := range r.devices {
		devices = append(devices, d)
	}
	r.devices = map[string]*device{}
	r.mu.Unlock()

	for _, d := range devices {
		d.close()
	}
	r.wg.Wait()
	close(r.events)
	return nil
}

// Watch discovers controllers and keeps discovering them, so unplugging and
// replugging works. It blocks until Close is called.
func (r *Reader) Watch() {
	r.scan()
	ticker := time.NewTicker(r.rescan)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			r.scan()
		case <-r.stopped:
			return
		}
	}
}

// scan opens any controller that is not already watched and starts its reader.
func (r *Reader) scan() {
	entries, err := os.ReadDir(r.devDir)
	if err != nil {
		return
	}
	for _, entry := range entries {
		name := entry.Name()
		if len(name) < 6 || name[:5] != "event" {
			continue
		}
		path := filepath.Join(r.devDir, name)

		r.mu.Lock()
		if r.closed {
			r.mu.Unlock()
			return
		}
		if _, watched := r.devices[path]; watched {
			r.mu.Unlock()
			continue
		}
		d, err := openDevice(path)
		if err != nil {
			r.mu.Unlock()
			continue
		}
		r.devices[path] = d
		r.primary = path
		r.mu.Unlock()

		r.publish(Event{Kind: KindConnected, Device: d.name})
		r.wg.Add(1)
		go func(d *device) {
			defer r.wg.Done()
			r.read(d)
		}(d)
	}
}

// drop forgets a controller so a later scan can pick it up again.
func (r *Reader) drop(path, name string) {
	r.mu.Lock()
	if _, watched := r.devices[path]; watched {
		delete(r.devices, path)
	}
	closed := r.closed
	r.mu.Unlock()
	if closed {
		return
	}
	r.publish(Event{Kind: KindDisconnected, Device: name})
}

// read consumes one device until it is unplugged or closed.
func (r *Reader) read(d *device) {
	held := map[Button]bool{}
	// Direction state is tracked separately from the raw buttons because a stick
	// release is a return to centre rather than an event of its own.
	dirHeld := map[Button]bool{}
	stickDir := map[string]Button{}

	releaseAll := func() {
		for b, down := range held {
			if down {
				r.publish(Event{Kind: KindButton, Device: d.name, Button: b, Pressed: false})
			}
		}
		for b, down := range dirHeld {
			if down {
				r.publish(Event{Kind: KindButton, Device: d.name, Button: b, Pressed: false})
			}
		}
	}
	defer releaseAll()

	buf := make([]byte, eventSize)
	for {
		n, err := d.fd.Read(buf)
		if err != nil {
			r.drop(d.path, d.name)
			return
		}
		if n < eventSize {
			continue
		}
		ev := decodeEvent(buf[:eventSize])

		switch ev.Typ {
		case unix.EV_KEY:
			button, isButton := d.button(ev.Code)
			if !isButton {
				continue
			}
			switch ev.Val {
			case keyRelease:
				if held[button] {
					held[button] = false
					r.publish(Event{Kind: KindButton, Device: d.name, Button: button, Pressed: false})
				}
			case keyPress:
				// keyRepeat is a held key, not a new edge, so it is ignored.
				if !held[button] {
					held[button] = true
					r.publish(Event{Kind: KindButton, Device: d.name, Button: button, Pressed: true})
				}
			}

		case unix.EV_ABS:
			name, isAxis := d.axis(ev.Code)
			if !isAxis {
				continue
			}
			value := d.normalise(ev.Code, ev.Val)

			// Only the left stick drives the dpad.
			if name == "lx" || name == "ly" {
				dir := stickDirection(name, value)
				if prev := stickDir[name]; prev != "" && prev != dir && dirHeld[prev] {
					delete(dirHeld, prev)
					r.publish(Event{Kind: KindButton, Device: d.name, Button: prev, Pressed: false})
				}
				if dir != "" && !dirHeld[dir] {
					dirHeld[dir] = true
					r.publish(Event{Kind: KindButton, Device: d.name, Button: dir, Pressed: true})
				}
				stickDir[name] = dir
			}
			if name == "lx" || name == "ly" {
				r.publish(Event{Kind: KindAxis, Device: d.name, Axis: name, Value: value})
			}
		}
	}
}

// stickDirection maps a normalised left-stick value to a dpad direction, or ""
// when the stick is inside the deadzone.
func stickDirection(axis string, value float64) Button {
	switch axis {
	case "lx":
		switch {
		case value > stickThreshold:
			return DirRight
		case value < -stickThreshold:
			return DirLeft
		}
	case "ly":
		switch {
		case value < -stickThreshold:
			return DirUp
		case value > stickThreshold:
			return DirDown
		}
	}
	return ""
}

// Rumble plays a force-feedback effect on the named controller. Controllers
// without rumble report ErrNoRumble rather than failing silently.
func (r *Reader) Rumble(device string, strong, weak uint16, durationMs uint16) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	for _, d := range r.devices {
		if d.name == device {
			return d.rumble(strong, weak, durationMs)
		}
	}
	return fmt.Errorf("gamepad: no controller named %q", device)
}

// Primary returns the name of a connected controller for callers that do not
// track devices themselves, such as rumble requests from the frontend. The most
// recently connected pad wins, because that is the one the player is holding.
func (r *Reader) Primary() string {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.primary == "" {
		return ""
	}
	if _, ok := r.devices[r.primary]; !ok {
		return ""
	}
	return r.devices[r.primary].name
}

// publish never blocks the device reader: a full channel means the shell is not
// draining, and dropping a change is better than stalling input.
func (r *Reader) publish(ev Event) {
	r.mu.Lock()
	closed := r.closed
	r.mu.Unlock()
	if closed {
		return
	}
	select {
	case r.events <- ev:
	default:
	}
}

// The Linux input event ABI, from linux/input-event-codes.h.
const (
	eventSize = int(unsafe.Sizeof(inputEvent{}))

	keyPress   = 1
	keyRelease = 0

	// stickThreshold is the fraction of full deflection that counts as a
	// direction, chosen above typical stick drift.
	stickThreshold = 0.5
)

// inputEvent mirrors struct input_event on 64-bit Linux.
type inputEvent struct {
	Sec  int64
	Usec int64
	Typ  uint16
	Code uint16
	Val  int32
}

// decodeEvent reads an input_event out of a raw read buffer.
func decodeEvent(buf []byte) inputEvent {
	return inputEvent{
		Sec:  int64(*(*int64)(unsafe.Pointer(&buf[0]))),
		Usec: int64(*(*int64)(unsafe.Pointer(&buf[8]))),
		Typ:  *(*uint16)(unsafe.Pointer(&buf[16])),
		Code: *(*uint16)(unsafe.Pointer(&buf[18])),
		Val:  *(*int32)(unsafe.Pointer(&buf[20])),
	}
}

// absInfo mirrors struct input_absinfo.
type absInfo struct {
	Value  int32
	Min    int32
	Max    int32
	Fuzz   int32
	Flat   int32
	Thresh int32
}

// device is one open controller with its decoded capability maps.
type device struct {
	path string
	name string
	fd   *os.File

	buttons map[uint16]Button
	axes    map[uint16]string
	ranges  map[uint16]absInfo
}

func openDevice(path string) (*device, error) {
	fd, err := os.OpenFile(path, os.O_RDONLY|syscall.O_NONBLOCK, 0)
	if err != nil {
		return nil, err
	}
	d := &device{
		path:    path,
		fd:      fd,
		buttons: map[uint16]Button{},
		axes:    map[uint16]string{},
		ranges:  map[uint16]absInfo{},
	}
	if err := d.describe(); err != nil {
		fd.Close()
		return nil, err
	}
	d.name = d.deviceName()
	return d, nil
}

// describe reads the capability bits and keeps only devices that look like a
// controller: absolute axes plus the four face buttons. That excludes mice,
// keyboards and the power button, which share /dev/input.
func (d *device) describe() error {
	evBits, err := ioctlBits(d.fd, eviocgbit(nrGetEvBits, 0, 0, 0), 32)
	if err != nil {
		return err
	}
	if !bitInRange(evBits, unix.EV_KEY, 0) || !bitInRange(evBits, unix.EV_ABS, 0) {
		return fmt.Errorf("gamepad: %s is not a controller", d.path)
	}

	keyBits, err := ioctlBits(d.fd, eviocgkeybit(0, 96), 96)
	if err != nil {
		return err
	}
	absBits, err := ioctlBits(d.fd, eviocgabsbit(0, 64), 64)
	if err != nil {
		return err
	}

	faces := 0
	for code, button := range codeButtons {
		if !bitInRange(keyBits, code, 0) {
			continue
		}
		d.buttons[code] = button
		if code == codeButtonSouth || code == codeButtonEast ||
			code == codeButtonNorth || code == codeButtonWest {
			faces++
		}
	}
	if faces == 0 {
		return fmt.Errorf("gamepad: %s has no face buttons", d.path)
	}

	for code, name := range codeAxes {
		if !bitInRange(absBits, code, 0) {
			continue
		}
		d.axes[code] = name
		if info, err := ioctlAbsInfo(d.fd, code); err == nil {
			d.ranges[code] = info
		}
	}
	return nil
}

func (d *device) deviceName() string {
	if name, ok := ioctlString(d.fd, eviocgname(128), 128); ok && name != "" {
		return name
	}
	return filepath.Base(d.path)
}

func (d *device) button(code uint16) (Button, bool) {
	button, ok := d.buttons[code]
	return button, ok
}

func (d *device) axis(code uint16) (string, bool) {
	name, ok := d.axes[code]
	return name, ok
}

// normalise maps a raw axis value to -1..1, or 0..1 for triggers, using the
// range the device reports rather than an assumed one.
func (d *device) normalise(code uint16, value int32) float64 {
	info, ok := d.ranges[code]
	if !ok {
		return 0
	}
	span := float64(info.Max) - float64(info.Min)
	if span <= 0 {
		return 0
	}
	norm := (float64(value) - float64(info.Min)) / span

	name, isAxis := codeAxes[code]
	if isAxis && isTriggerAxis(name) {
		// Triggers rest at their minimum, so 0 at rest and 1 fully pressed.
		return clamp(norm, 0, 1)
	}
	norm = norm*2 - 1
	// The kernel reports Y with its minimum at the top, so up is negative.
	if name == "ly" || name == "ry" {
		norm = -norm
	}
	return clamp(norm, -1, 1)
}

// isTriggerAxis reports whether an axis is a pressure trigger. On most pads
// these are ABS_Z and ABS_RZ; some report triggers on ABS_RX/ABS_RY when the
// sticks are absent.
func isTriggerAxis(name string) bool { return name == "lz" || name == "rz" }

func clamp(v, lo, hi float64) float64 {
	if v < lo {
		return lo
	}
	if v > hi {
		return hi
	}
	return v
}

// rumble plays a constant-strength effect on the two slots the kernel reserves
// for simple rumble.
func (d *device) rumble(strong, weak uint16, durationMs uint16) error {
	ffBits, err := ioctlBits(d.fd, eviocgbit(0x15, 0, 0, 0), 8)
	if err != nil {
		return ErrNoRumble
	}
	if !bitInRange(ffBits, unix.EV_FF, 0) {
		return ErrNoRumble
	}

	const (
		strongSlot = 0x50
		weakSlot   = 0x51
	)
	for _, slot := range []struct {
		id    uint16
		level uint16
	}{{strongSlot, strong}, {weakSlot, weak}} {
		if slot.level == 0 {
			continue
		}
		if err := d.play(slot.id, slot.level, durationMs); err != nil {
			return err
		}
	}
	return nil
}

// play uploads a ramp effect for one slot and starts it.
func (d *device) play(id, level, durationMs uint16) error {
	effect := newRampEffect(id, level, durationMs)
	if err := uploadEffect(d.fd, effect); err != nil {
		return err
	}
	idValue := int32(id)
	if err := ioctlPtr(d.fd.Fd(), eviocrmff(), unsafe.Pointer(&idValue)); err != nil {
		return err
	}
	if err := ioctlPtr(d.fd.Fd(), eviocsff(uintptr(ffRampSize)), unsafe.Pointer(&idValue)); err != nil {
		return err
	}
	return ioctlPtr(d.fd.Fd(), eviocplay(), unsafe.Pointer(&idValue))
}

func (d *device) close() { d.fd.Close() }

// Kernel button and axis codes, from linux/input-event-codes.h.
const (
	codeButtonSouth  = 0x130 // BTN_SOUTH, BTN_A
	codeButtonEast   = 0x131 // BTN_EAST,  BTN_B
	codeButtonNorth  = 0x133 // BTN_NORTH, BTN_X
	codeButtonWest   = 0x134 // BTN_WEST,  BTN_Y
	codeButtonTL     = 0x136
	codeButtonTR     = 0x137
	codeButtonTL2    = 0x138
	codeButtonTR2    = 0x139
	codeButtonSelect = 0x13a
	codeButtonStart  = 0x13b
	codeButtonMode   = 0x13c
	codeButtonThumbL = 0x13d
	codeButtonThumbR = 0x13e
	codeDPadUp       = 0x220
	codeDPadDown     = 0x221
	codeDPadLeft     = 0x222
	codeDPadRight    = 0x223
)

// codeButtons maps BTN_* codes to the names the shell uses. The codes follow
// the common gamepad convention, which is what the overwhelming majority of
// USB and Bluetooth pads report; a pad that deviates is a device-specific case
// handled by its own driver.
var codeButtons = map[uint16]Button{
	codeButtonSouth:  ButtonA,
	codeButtonEast:   ButtonB,
	codeButtonNorth:  ButtonY,
	codeButtonWest:   ButtonX,
	codeButtonTL:     ButtonL1,
	codeButtonTR:     ButtonR1,
	codeButtonTL2:    ButtonL2,
	codeButtonTR2:    ButtonR2,
	codeButtonSelect: ButtonSelect,
	codeButtonStart:  ButtonStart,
	codeButtonMode:   ButtonGuide,
	codeButtonThumbL: ButtonL3,
	codeButtonThumbR: ButtonR3,
	codeDPadUp:       DirUp,
	codeDPadDown:     DirDown,
	codeDPadLeft:     DirLeft,
	codeDPadRight:    DirRight,
}

// codeAxes maps ABS_* codes to stick and trigger names.
var codeAxes = map[uint16]string{
	0x00: "lx", // ABS_X
	0x01: "ly", // ABS_Y
	0x02: "lz", // ABS_Z
	0x03: "rx", // ABS_RX
	0x04: "ry", // ABS_RY
	0x05: "rz", // ABS_RZ
}
