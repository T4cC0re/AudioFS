//go:build release
package native

/*
#cgo CFLAGS: -flto -ffat-lto-objects -O3 -DAUDIOFS_NO_TRACE=1 -I${SRCDIR}/dependencies/release/built/include
#cgo LDFLAGS: -L${SRCDIR}/dependencies/release/built/lib
*/
import "C"
var mode = "non-release"
