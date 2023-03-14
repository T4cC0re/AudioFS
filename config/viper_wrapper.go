package config

import (
	"fmt"
	"github.com/spf13/viper"
	"sync"
)

type Viper struct {
	*viper.Viper
	*sync.RWMutex
}

var Config *Viper

func init() {
	Config = &Viper{Viper: viper.GetViper(), RWMutex: &sync.RWMutex{}}
	Config.SetConfigType("yaml")
	Config.SetConfigName("audiofs")
	Config.AddConfigPath(".")
	Config.SetEnvPrefix("audiofs")
	Config.AutomaticEnv()
	err := Config.ReadInConfig()
	if err != nil { // Handle errors reading the config file
		if _, ok := err.(viper.ConfigFileNotFoundError); ok {
			// Config file not found; ignore error if desired
		} else {
			// Config file was found but another error was produced
			panic(fmt.Errorf("fatal error config file: %w", err))
		}
	}
}

func (v *Viper) Store() error {
	return Config.WriteConfigAs("./audiofs.yaml")
}

func (v *Viper) Sub(key string) *Viper {
	return &Viper{Viper: v.Viper.Sub(key), RWMutex: v.RWMutex}
}
