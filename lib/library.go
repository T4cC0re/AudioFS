package lib

import (
	"fmt"
	"gitlab.com/t4cc0re/audiofs/config"
	// _ "gitlab.com/t4cc0re/audiofs/native"
)

type Error struct {
	errorText string
	error
	errorCode int
}

func (e *Error) Error() string {
	if e.error != nil {
		return e.error.Error()
	}
	return e.errorText
}

func (e *Error) Unwrap() error {
	return e.error
}
func (e *Error) Code() int {
	return e.errorCode
}

func NewError(text string, code int) *Error {
	return &Error{
		errorText: text,
		errorCode: code,
	}
}

func WrapError(err error, code int) *Error {
	return &Error{
		error:     err,
		errorCode: code,
	}
}

var (
	ERR_NOTIMPLEMENTED = NewError("not implemented", -1)
	ERR_UNKNOWN        = NewError("unknown error", -0xff)
)

func useNativeFFmpeg() bool {
	return config.Config.GetBool("experimental.native_code.ffmpeg")
}

func AddToCatalog(path string) error {
	fmt.Println("AddToCatalog: '" + path + "'")
	return ERR_NOTIMPLEMENTED
}

func ImportFile(path string, keepOriginal bool, carefulDedupe bool) error {
	fmt.Println("ImportFiles: '" + path + "'")
	fmt.Printf("keepOriginal: %t\n", keepOriginal)
	fmt.Printf("carefulDedupe: %t\n", carefulDedupe)
	return ERR_NOTIMPLEMENTED
}

func ImportCatalog(keepOriginal bool, carefulDedupe bool) error {
	fmt.Println("ImportCatalog")
	fmt.Printf("keepOriginal: %t\n", keepOriginal)
	fmt.Printf("carefulDedupe: %t\n", carefulDedupe)
	return ERR_NOTIMPLEMENTED
}

func Exists(path string, carefulDedupe bool) (bool, error) {
	return false, ERR_NOTIMPLEMENTED
}
