package util

import (
	"encoding/json"
	"errors"
	"fmt"
	"github.com/klauspost/compress/zstd"
	"gitlab.com/t4cc0re/audiofs/config"
)

func MarshallCompressed(v any) ([]byte, error) {
	return MarshallCompressedWithAlgo(
		config.Config.GetString("json_export.compression.algo"),
		config.Config.GetInt("json_export.compression.level"),
		v,
	)
}

func UnmarshallCompressed(data []byte, v any) error {
	return UnmarshallCompressedWithAlgo(
		config.Config.GetString("json_export.compression.algo"),
		data,
		v,
	)
}

func UnmarshallCompressedWithAlgo(algo string, input []byte, v any) error {
	var uncompressed []byte
	var err error
	switch algo {
	case "zstd":
		var decoder, _ = zstd.NewReader(nil)
		defer decoder.Close()
		uncompressed, err = decoder.DecodeAll(input, make([]byte, 0, len(input)))
		fmt.Printf("compressed: %d, JSON: %d\n", len(input), len(uncompressed))
	case "none", "":
		// No compression? Nothing to do :D
		uncompressed = input
	default:
		err = errors.New("unsupported algorithm")
	}
	if err != nil {
		return err //TODO: wrap
	}

	return json.Unmarshal(uncompressed, v)
}

func MarshallCompressedWithAlgo(algo string, level int, v any) ([]byte, error) {
	buf, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}

	switch algo {
	case "zstd":
		var encoder, _ = zstd.NewWriter(nil, zstd.WithEncoderLevel(zstd.EncoderLevelFromZstd(level)))
		defer encoder.Close()
		buf2 := encoder.EncodeAll(buf, make([]byte, 0, len(buf)))
		fmt.Printf("JSON: %d, compressed: %d\n", len(buf), len(buf2))
		return buf2, nil
	case "none", "":
		return buf, nil
	default:
		return nil, errors.New("unsupported algorithm")
	}
}
