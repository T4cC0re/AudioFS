package main

// Package needs to be overridden to main for this to work.

/*
#cgo CFLAGS: -flto -ffat-lto-objects -fPIC

// C API definitions
#define AUDIOFS_ERR_NOTIMPLEMENTED -1;
#define AUDIOFS_ERR_UNKNOWN -0xff;
*/
import "C" // Separate import to render comment into generated header.
import (
	"fmt"
	"gitlab.com/t4cc0re/audiofs/lib"
	"os"
)

// main: This is a dummy entrypoint that we need for compilation.
func main() {}

// region "glue"
func errorToErrorCode(err error) C.int {
	if err != nil {
		var ok bool
		if err, ok = err.(*lib.Error); ok {
			return C.int(err.(*lib.Error).Code())
		}
		return C.int(lib.ERR_UNKNOWN.Code())
	}
	return C.int(0)
}

// endregion "glue"

// region "audiofs/lib exports"
//
//export audiofs_library_addToCatalog
func audiofs_library_addToCatalog(path *C.char) C.int {
	err := lib.AddToCatalog(C.GoString(path))
	return errorToErrorCode(err)
}

//export audiofs_library_import_file
func audiofs_library_import_file(path *C.char, b_keepOriginal bool, b_carefulDedupe bool) C.int {
	err := lib.ImportFile(C.GoString(path), b_keepOriginal, b_carefulDedupe)
	return errorToErrorCode(err)
}

//export audiofs_library_import_catalog
func audiofs_library_import_catalog(b_keepOriginal bool, b_carefulDedupe bool) C.int {
	err := lib.ImportCatalog(b_keepOriginal, b_carefulDedupe)
	return errorToErrorCode(err)
}

//export audiofs_library_exists
func audiofs_library_exists(path *C.char, b_carefulDedupe bool) C.int {
	res, err := lib.Exists(C.GoString(path), b_carefulDedupe)
	if err != nil {
		_, _ = fmt.Fprintf(os.Stderr, "WARN: C API returned error: %v\n", err)
		return C.int(0)
	}
	if res == true {
		return C.int(1)
	} else {
		return C.int(0)
	}
}

// endregion "audiofs/lib exports"
