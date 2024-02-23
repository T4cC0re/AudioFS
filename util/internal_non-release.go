//go:build !release

package util

import (
	"os"
	"path"
	"path/filepath"
	"strings"

	"github.com/sirupsen/logrus"
)

const straceOptions = "" //"-C"
const nativeBlob = "native"

var nativeBlobExec = []string{}

func init() {
	if strings.HasPrefix(nativeBlob, "/") {
		nativeBlobExec = []string{nativeBlob}
	} else {
		currentExecutable, err := os.Executable()
		if err != nil {
			logrus.Errorf("Incorrectable error: %+v", err)
			panic(err)
		}
		nativeBlobExec = []string{path.Join(filepath.Dir(currentExecutable), nativeBlob)}
	}
	if len(straceOptions) != 0 {
		strace := strings.Split(straceOptions, " ")
		strace = append([]string{"strace"}, strace...)
		nativeBlobExec = append(strace, nativeBlobExec...)
	}
}
