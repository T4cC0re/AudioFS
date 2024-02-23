package util

import (
	"encoding/json"
	"errors"
	"os"
	"os/exec"

	"github.com/sirupsen/logrus"
	"gitlab.com/t4cc0re/audiofs/lib/types"
)

func GetMetadataFromFile(file string) (*types.FileMetadata, error) {
	commandString := append(nativeBlobExec, file)
	cmd := exec.Command(commandString[0], commandString[1:]...)
	cmd.Stderr = os.Stderr
	JSON, err := cmd.Output()
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
