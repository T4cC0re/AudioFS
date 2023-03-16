package native

/*
#cgo CFLAGS: -flto -ffat-lto-objects -fPIC -DAUDIOFS_CGO=1
#cgo pkg-config: libavformat libavutil libavcodec libchromaprint
#cgo LDFLAGS: -ljansson

#include <stdlib.h>
// region libav.c
extern void audiofs_libav_setup();
extern char *get_metadate_from_file(char *path);
extern char *chromaprint_from_file(const char *path);
// endregion libav.c

//region logbuffer.c
extern void audiofs_log_level_set(int level);
// endregion logbuffer.c

// helper functions
*/
import "C"
import (
	"encoding/json"
	"errors"
	"github.com/sirupsen/logrus"
	"gitlab.com/t4cc0re/audiofs/lib/types"
	"unsafe"
)

func init() {
	C.audiofs_libav_setup()
}

func ApplyLogrusLevel() {
	C.audiofs_log_level_set(C.int(logrus.GetLevel()))
}

// go_print prints to logrus from C using the specified level.
//
//export go_print
func go_print(level C.int, function *C.char, file *C.char, line C.int, string *C.char, len C.int) {
	cstr := C.GoStringN(string, len)
	f := C.GoString(file)      // This is compiled in on the C side, so we don't run chance of reading over a buffer boundary
	fc := C.GoString(function) // This is compiled in on the C side, so we don't run chance of reading over a buffer boundary
	l := logrus.WithField("function", fc).WithField("file", f).WithField("line", line).WithField("type", "C")
	switch int(level) {
	case int(logrus.PanicLevel), int(logrus.FatalLevel), int(logrus.ErrorLevel):
		// Don't respect panic and fatal levels.
		l.Error(cstr)
	case int(logrus.WarnLevel):
		l.Warn(cstr)
	case int(logrus.InfoLevel):
		l.Info(cstr)
	case int(logrus.DebugLevel):
		l.Debug(cstr)
	case int(logrus.TraceLevel):
		l.Trace(cstr)
	}
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

	val := types.FileMetadata{}
	err := json.Unmarshal([]byte(JSON), &val)
	if err != nil {
		return nil, errors.Join(err, errors.New("unknown"))
	}

	return &val, nil
}
