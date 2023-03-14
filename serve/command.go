package serve

import (
	"errors"
	"github.com/spf13/cobra"
)

var cmdServe = &cobra.Command{
	Use:   "serve",
	Short: "serves AudioFS via platform dependant methods",
	Long:  `serves AudioFS via platform dependant methods`,
	Args:  cobra.MaximumNArgs(0),
	RunE: func(cmd *cobra.Command, args []string) error {
		return errors.New("please select a subcommand")
	},
}

func Inject(rootCommand *cobra.Command) {
	rootCommand.AddCommand(cmdServe)
}
