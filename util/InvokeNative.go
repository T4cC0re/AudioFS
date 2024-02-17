package util

import (
	"encoding/json"
	"errors"
	"os"
	"os/exec"
	"path"
	"path/filepath"

	"github.com/sirupsen/logrus"
	"gitlab.com/t4cc0re/audiofs/lib/types"
)

func GetMetadataFromFile(file string) (*types.FileMetadata, error) {

	ex, err := os.Executable()
	if err != nil {
		panic(err)
	}
	exPath := filepath.Dir(ex)
	JSON, err := exec.Command(path.Join(exPath, "native"), file).Output()
	if err != nil {
		logrus.Errorf("%+v", err)
		return nil, err
	}

	val := types.FileMetadata{}
	err = json.Unmarshal([]byte(JSON), &val)
	if err != nil {
		return nil, errors.Join(err, errors.New("unknown"))
	}

	return &val, nil
}
