//go:build darwin
package native

/*
#cgo amd64 CFLAGS: -I/usr/local/include
#cgo arm64 CFLAGS: -I/opt/homebrew/include
#cgo amd64 LDFLAGS: -L/usr/local/lib
#cgo arm64 LDFLAGS: -L/opt/homebrew/lib
*/
import "C"
