//go:build release
package native

/*
#cgo CFLAGS: -flto -ffat-lto-objects -O3 -DAUDIOFS_NO_TRACE=1
#cgo amd64 CFLAGS: -I${SRCDIR}/dependencies/release_x86_64/built/include
#cgo arm64 CFLAGS: -I${SRCDIR}/dependencies/release_arm64/built/include
#cgo amd64 LDFLAGS: -L${SRCDIR}/dependencies/release_x86_64/built/lib
#cgo arm64 LDFLAGS: -L${SRCDIR}/dependencies/release_arm64/built/lib
*/
import "C"
var mode = "release"
