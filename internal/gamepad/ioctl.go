package gamepad

import (
	"os"
	"syscall"
	"unsafe"
)

// Linux packs an ioctl request into one unsigned long: direction in the top two
// bits, then size, type and request number. These shifts and widths come from
// asm-generic/ioctl.h.
const (
	iocNRShift   = 0
	iocTypeShift = iocNRShift + 8   // 8
	iocSzShift   = iocTypeShift + 8 // 16
	iocDirShift  = iocSzShift + 14  // 30

	iocNone  = 0
	iocRead  = 2
	iocWrite = 1

	// iocRW is _IOC_READ | _IOC_WRITE, encoded as 3 in the direction field.
	iocRW = 3

	evIOCType = 'E'
)

// Request numbers from linux/input.h. EV_* codes above 0x20 are folded into the
// request number, because that is what the kernel's ioctl dispatcher expects.
const (
	nrGetEvBits  = 0x20
	nrGetKeyBits = 0x21
	nrGetAbsBits = 0x23
	nrGetName    = 0x06
	nrGetAbs     = 0x40
	nrSetFF      = 0x80
	nrRemoveFF   = 0x81
	nrGetEffects = 0x84
	nrPlay       = 0x84
)

// kernelIntSize is sizeof(int) as the kernel sees it. These ioctls are declared
// upstream with a plain int, which is 4 bytes on every architecture; using Go's
// int size would encode a request the kernel rejects.
const kernelIntSize = 4

// absInfoSize is sizeof(struct input_absinfo): six __s32 fields.
const absInfoSize = 24

// ioc assembles an ioctl request.
func ioc(dir, typ, nr, size uintptr) uintptr {
	return dir<<iocDirShift |
		size<<iocSzShift |
		typ<<iocTypeShift |
		nr<<iocNRShift
}

// eviocgbit reads a capability bitmap for the event type ev, covering len bytes
// starting at code. Asking for the event-type bitmap itself passes len 0.
func eviocgbit(nr, ev, code, length uintptr) uintptr {
	if ev == code && length == 0 {
		length = 8 // EV_* has fewer than 64 types today
	}
	return ioc(iocRead, evIOCType, nr+ev, length)
}

// eviocgkeybit reads the key/button bitmap of len bytes starting at code.
func eviocgkeybit(code, length uintptr) uintptr {
	return ioc(iocRead, evIOCType, nrGetKeyBits+code, length)
}

// eviocgabsbit reads the absolute-axis bitmap of len bytes starting at code.
func eviocgabsbit(code, length uintptr) uintptr {
	return ioc(iocRead, evIOCType, nrGetAbsBits+code, length)
}

// eviocgname reads a device name of len bytes.
func eviocgname(length uintptr) uintptr {
	return ioc(iocRead, evIOCType, nrGetName, length)
}

// eviocgabs reads an axis range. The axis code goes in the request number, as
// _IOR('E', 0x40 + (abs), struct input_absinfo) does upstream.
func eviocgabs(code uint16) uintptr {
	return ioc(iocRead, evIOCType, nrGetAbs+uintptr(code), absInfoSize)
}

// eviocsff writes a force-feedback effect whose struct is size bytes. The
// kernel's struct ff_effect is at least this large, and drivers report the size
// they accept through EVIOCGEFFECTS' sibling, EVIOCSFF's return convention, so
// callers probe.
func eviocsff(size uintptr) uintptr {
	return ioc(iocWrite, evIOCType, nrSetFF, size)
}

// eviocrmff removes a force-feedback effect by id.
func eviocrmff() uintptr {
	return ioc(iocWrite, evIOCType, nrRemoveFF, kernelIntSize)
}

// eviocplay starts a force-feedback effect by id.
func eviocplay() uintptr {
	return ioc(iocRW, evIOCType, nrPlay, kernelIntSize)
}

// ioctlBits reads a capability bitmap of length bytes.
func ioctlBits(f *os.File, request uintptr, length int) ([]byte, error) {
	buf := make([]byte, length)
	if err := ioctlPtr(f.Fd(), request, unsafe.Pointer(&buf[0])); err != nil {
		return nil, err
	}
	return buf, nil
}

// ioctlAbsInfo reads an axis' reported range.
func ioctlAbsInfo(f *os.File, code uint16) (absInfo, error) {
	var info absInfo
	err := ioctlPtr(f.Fd(), eviocgabs(code), unsafe.Pointer(&info))
	return info, err
}

// ioctlString reads a NUL-terminated name of at most max bytes.
func ioctlString(f *os.File, request uintptr, max int) (string, bool) {
	buf := make([]byte, max)
	if err := ioctlPtr(f.Fd(), request, unsafe.Pointer(&buf[0])); err != nil {
		return "", false
	}
	for i, b := range buf {
		if b == 0 {
			return string(buf[:i]), true
		}
	}
	return string(buf), true
}

// ioctlPtr performs an ioctl with a buffer pointer.
func ioctlPtr(fd, request uintptr, arg unsafe.Pointer) error {
	_, _, errno := syscall.Syscall(syscall.SYS_IOCTL, fd, request, uintptr(arg))
	if errno != 0 {
		return errno
	}
	return nil
}

// bitInRange reports whether bit code is set, where the bitmap's first code is
// first. Out-of-range bits are treated as unset.
func bitInRange(bits []byte, code uint16, first int) bool {
	index := int(code) - first
	if index < 0 {
		return false
	}
	byteIndex := index / 8
	if byteIndex >= len(bits) {
		return false
	}
	return bits[byteIndex]&(1<<(index%8)) != 0
}
