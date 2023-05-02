package native

/*
#cgo CFLAGS: -DAUDIOFS_CGO=1 -Werror=unused-result -D_FORTIFY_SOURCE=2 -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector --param=ssp-buffer-size=4 -m64 -mtune=generic -fPIC
#cgo !darwin LDFLAGS: -fPIE

#cgo !darwin LDFLAGS: -static -Wl,--start-group -lswresample-audiofs -lswscale-audiofs -lavformat-audiofs -lavutil-audiofs -lavcodec-audiofs -lavfilter-audiofs -lchromaprint_audiofs -lfftw3 -ljansson -lstdc++ -lm -lz -Wl,--end-group
#cgo darwin LDFLAGS: -framework VideoToolbox -framework CoreFoundation -framework CoreMedia -framework CoreVideo -framework CoreServices -liconv -lswresample-audiofs -lswscale-audiofs -lavformat-audiofs -lavutil-audiofs -lavcodec-audiofs -lavfilter-audiofs -lchromaprint_audiofs -lfftw3 -ljansson -lstdc++ -lm -lz

#include "golang_glue.h"
*/
import "C"
import (
	"encoding/json"
	"errors"
	"fmt"
	"github.com/sirupsen/logrus"
	"gitlab.com/t4cc0re/audiofs/config"
	"gitlab.com/t4cc0re/audiofs/lib/types"
	"unsafe"
)

func init() {
	C.audiofs_libav_setup()
	data := COnwedByteSliceFromAudioFSBuffer(unsafe.Pointer(C.test_buffer))
	println(string(data))
	fmt.Printf("build mode: %s\n", mode)
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

//export get_setting_string
func get_setting_string(setting *C.char) *C.char {
	str := C.GoString(setting)
	return C.CString(config.Config.GetString(str))
}

//export get_setting_int
func get_setting_int(setting *C.char) C.int {
	str := C.GoString(setting)
	return C.int(config.Config.GetInt(str))
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

func COnwedByteSliceFromAudioFSBuffer(ptr unsafe.Pointer) []byte {

	//lock := uint(C.offset_audiofs_buffer_lock)
	//data := uint(C.offset_audiofs_buffer_data)
	//leng := uint(C.offset_audiofs_buffer_len)

	//println(lock)
	//println(data)
	//println(leng)

	fmt.Printf("%p\n", ptr)

	buffer := (*C.audiofs_buffer)(ptr)
	C.pthread_mutex_lock(&buffer.lock)
	//len2 := C.audiofs_buffer_data_len(ptr)
	//
	//println(len2)
	//data2 := unsafe.Pointer(uintptr(ptr) + uintptr(data))
	fmt.Printf("%p\n", buffer.data)
	C.pthread_mutex_lock(&buffer.used_outside_audiofs)
	C.pthread_mutex_unlock(&buffer.lock)

	//return unsafe.Slice((*byte)(data3), int(len2))
	return nil
}
