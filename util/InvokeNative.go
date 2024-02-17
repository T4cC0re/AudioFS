package util

import (
    "encoding/json"
	"errors"
	"github.com/sirupsen/logrus"
//	"gitlab.com/t4cc0re/audiofs/config"
	"gitlab.com/t4cc0re/audiofs/lib/types"
	"os/exec"

)


func GetMetadataFromFile(path string) (*types.FileMetadata, error) {

	JSON, err := exec.Command("./bin/native", path).Output()
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


