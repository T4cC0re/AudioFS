package native

/*
#cgo CFLAGS: -flto -ffat-lto-objects -fPIC
#cgo pkg-config: libavformat libavutil libavcodec
#cgo LDFLAGS: -ljansson

#include <stdlib.h>
// region libav.c
extern void audiofs_libav_setup();
extern char * get_metadate_from_file(char *path);
// endregion libav.c
*/
import "C"
import (
	"encoding/json"
	"errors"
	"gitlab.com/t4cc0re/audiofs/lib/types"
	"unsafe"
)

func init() {
	C.audiofs_libav_setup()
}

func GetMetadataFromFile(path string) (*types.FileMetadata, error) {
	cstr := C.CString(path)
	defer C.free(unsafe.Pointer(cstr))
	metadata := C.get_metadate_from_file(cstr)
	if metadata == nil {
		return nil, errors.New("asdf")
	}
	JSON := C.GoString(metadata)
	C.free(unsafe.Pointer(metadata))

	println(JSON)

	val := types.FileMetadata{}
	err := json.Unmarshal([]byte(JSON), &val)
	if err != nil {
		return nil, errors.Join(err, errors.New("unknown"))
	}

	return &val, nil
}
