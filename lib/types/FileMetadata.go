package types

import (
	"encoding/json"
	"github.com/sirupsen/logrus"
	"math/big"
)

type FileMetadata struct {
	File struct {
		Metadata map[string]string `json:"metadata"`
		Format   struct {
			Name     string `json:"name"`
			LongName string `json:"long_name"`
			MimeType string `json:"mime_type,omitempty"`
			Flags    int    `json:"flags,omitempty"`
		} `json:"format"`
		StartTime int `json:"start_time,omitempty"`
		Duration  int `json:"duration"`
		BitRate   int `json:"bit_rate,omitempty"`
	} `json:"file"`
	Streams []struct {
		Metadata map[string]string `json:"metadata"`
		Codec    struct {
			ChLayout           string `json:"ch_layout"`
			Type               string `json:"type"`
			Name               string `json:"name"`
			CodecTag           int    `json:"codec_tag,omitempty"`
			BitRate            int    `json:"bit_rate,omitempty"`
			BitsPerCodedSample int    `json:"bits_per_coded_sample,omitempty"`
			BitsPerRawSample   int    `json:"bits_per_raw_sample,omitempty"`
			Profile            int    `json:"profile,omitempty"`
			Level              int    `json:"level,omitempty"`
			Width              int    `json:"width,omitempty"`
			Height             int    `json:"height,omitempty"`
			BitsPerSample      int    `json:"bits_per_sample,omitempty"`
			SampleRate         int    `json:"sample_rate,omitempty"`
			FrameSize          int    `json:"frame_size,omitempty"`
			BlockAlign         int    `json:"block_align,omitempty"`
			NbChannels         int    `json:"nb_channels"`
			ProfileName        string `json:"profile_name,omitempty"`
		} `json:"codec"`
		Index       int      `json:"index"`
		NbFrames    int      `json:"nb_frames"`
		Duration    int      `json:"duration"`
		TimeBaseNum int64    `json:"time_base_num"`
		TimeBaseDen int64    `json:"time_base_den"`
		TimeBase    *big.Rat `json:"time_base"`
		Chromaprint string   `json:"chromaprint,omitempty"`
	} `json:"streams"`
}

func (f *FileMetadata) filltimeBase() {
	for k := range f.Streams {
		if f.Streams[k].TimeBase == nil {
			if f.Streams[k].TimeBaseDen == 0 {
				// This would crash, so we just ignore these.
				logrus.Warn("parsed timebase denominator is 0.")
				continue
			}
			f.Streams[k].TimeBase = big.NewRat(f.Streams[k].TimeBaseNum, f.Streams[k].TimeBaseDen)
		}
		if f.Streams[k].TimeBaseNum == 0 {
			f.Streams[k].TimeBaseNum = f.Streams[k].TimeBase.Num().Int64()
		}
		if f.Streams[k].TimeBaseDen == 0 {
			f.Streams[k].TimeBaseDen = f.Streams[k].TimeBase.Denom().Int64()
		}
	}
}

func (f *FileMetadata) UnmarshalJSON(data []byte) error {
	type tmp FileMetadata // need to use a different type to prevent a stack overflow.
	var it tmp
	if err := json.Unmarshal(data, &it); err != nil {
		return err
	}
	*f = FileMetadata(it)
	f.filltimeBase()
	return nil
}
