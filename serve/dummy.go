package serve

import (
	"fmt"
	"github.com/spf13/cobra"
)

var cmdDummy = &cobra.Command{
	Use:   "dummy",
	Short: "dummy server",
	Long:  `dummy server`,
	Args:  cobra.MinimumNArgs(0),
	Run: func(cmd *cobra.Command, args []string) {
		fmt.Println("ASDFASDFASDF")
	},
	Hidden: false,
}

func init() {
	cmdServe.AddCommand(cmdDummy)
}
