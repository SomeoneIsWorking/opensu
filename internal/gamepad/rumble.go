package gamepad

import (
	"os"
	"unsafe"
)

// Force feedback, from linux/input.h and linux/input-event-codes.h.
//
// The kernel's struct ff_effect is a fixed header followed by a union of effect
// types. Only FF_RAMP with a one-byte envelope is needed for a constant rumble
// level, so the buffer is sized for the largest layout these kernels accept and
// every field is written at its documented offset.

const (
	ffTypeRamp = 0x50

	// ffEffectMaxSize covers struct ff_effect up to ff_periodic_effect, the
	// largest variant the union can hold. Writing more than the driver expects
	// fails, so uploadEffect probes downwards from here.
	ffEffectMaxSize = 72
)

// Offsets within struct ff_effect, from the field order in linux/input.h:
//
//	type       __u16   0
//	id         __s16   2
//	direction  __u16   4
//	trigger    (8)     6   struct ff_trigger { button, interval }
//	replay     (8)    14   struct ff_replay { length, delay }
//	u.ramp    (12)    22   struct ff_ramp_effect { start_level, end_level, envelope }
//	                 34   envelope.replay_length, envelope.replay_count
//	                 36   envelope.data[]
const (
	ffOffsetType         = 0
	ffOffsetID           = 2
	ffOffsetDirection    = 4
	ffOffsetLength       = 14 + 0 // replay.length
	ffOffsetGain         = 22 + 8 // u.gain, which precedes u.ramp in the union
	ffOffsetStart        = 22 + 0 // u.ramp.start_level
	ffOffsetEnd          = 22 + 2 // u.ramp.end_level
	ffOffsetEnvelopeLen  = 34 + 0
	ffOffsetEnvelopeRep  = 34 + 2
	ffOffsetEnvelopeData = 36
)

// ffRampSize is the smallest struct ff_effect that can carry FF_RAMP with a
// one-byte envelope: the header plus a ramp effect.
const ffRampSize = ffOffsetEnvelopeData + 1

// uploadEffect writes a force-feedback effect, trying each plausible struct size
// until one is accepted. Drivers reject a write whose length does not match
// their own struct ff_effect, and that size varies by kernel version, so the
// largest accepted request wins.
func uploadEffect(f *os.File, effect []byte) error {
	for size := len(effect); size >= ffRampSize; size -= 8 {
		if ioctlPtr(f.Fd(), eviocsff(uintptr(size)), unsafe.Pointer(&effect[0])) == nil {
			return nil
		}
	}
	return ErrNoRumble
}

// newRampEffect builds a constant-level ramp effect lasting durationMs.
func newRampEffect(id, level, durationMs uint16) []byte {
	effect := make([]byte, ffEffectMaxSize)
	putU16(effect[ffOffsetType:], ffTypeRamp)
	putU16(effect[ffOffsetID:], id)
	putU16(effect[ffOffsetDirection:], 0)
	putU16(effect[ffOffsetLength:], durationMs)
	// Full gain, no attack and no fade, so the level is reached immediately and
	// held for the whole effect.
	putU16(effect[ffOffsetGain:], 0xffff)
	putU16(effect[ffOffsetStart:], level)
	putU16(effect[ffOffsetEnd:], level)
	effect[ffOffsetEnvelopeLen] = 1
	effect[ffOffsetEnvelopeRep] = 0
	effect[ffOffsetEnvelopeData] = byte(level >> 8)
	effect[ffOffsetEnvelopeData+1] = byte(level)
	return effect
}

func putU16(buf []byte, v uint16) {
	buf[0] = byte(v)
	buf[1] = byte(v >> 8)
}
