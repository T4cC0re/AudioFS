//go:build !release
package native

/*
#cgo CFLAGS: -O0 -g -I${SRCDIR}/dependencies/non-release/built/include
#cgo LDFLAGS: -L${SRCDIR}/dependencies/non-release/built/lib
*/
import "C"

var mode = "release"
