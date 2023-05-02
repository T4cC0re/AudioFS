//go:build !release
package native

/*
#cgo CFLAGS: -O0 -g
#cgo amd64 CFLAGS: -I${SRCDIR}/dependencies/non-release_x86_64/built/include
#cgo arm64 CFLAGS: -I${SRCDIR}/dependencies/non-release_arm64/built/include
#cgo amd64 LDFLAGS: -L${SRCDIR}/dependencies/non-release_x86_64/built/lib
#cgo arm64 LDFLAGS: -L${SRCDIR}/dependencies/non-release_arm64/built/lib
*/
import "C"

var mode = "non-release"
