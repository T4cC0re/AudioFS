package main

import (
	"fmt"
	"github.com/sirupsen/logrus"
	"gitlab.com/t4cc0re/audiofs/config"
	_ "gitlab.com/t4cc0re/audiofs/config"
	"gitlab.com/t4cc0re/audiofs/lib"
	"gitlab.com/t4cc0re/audiofs/lib/types"
	"gitlab.com/t4cc0re/audiofs/native"
	"gitlab.com/t4cc0re/audiofs/serve"
	"gitlab.com/t4cc0re/audiofs/util"
	"strings"

	"github.com/spf13/cobra"
)

func main() {
	var import_KeepOriginal = true
	var importExists_CarefulDedupe = true

	var cmdImport = &cobra.Command{
		Use:   "import [file to import]",
		Short: "import a file into AudioFS",
		Long:  `import will import a file into AudioFS and deduplicate its contents where appropriate.`,
		Args:  cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {
			lib.ImportFile(args[0], import_KeepOriginal, importExists_CarefulDedupe)
		},
	}

	var cmdImportCatalog = &cobra.Command{
		Use:   "import-catalog",
		Short: "import all cataloged references",
		Long:  `import-catalog will import all references previously stored via 'catalog', as if they were passed to 'import'. This can be used for a multi-stage deduplication.`,
		Args:  cobra.MinimumNArgs(0),
		Run: func(cmd *cobra.Command, args []string) {
			lib.ImportCatalog(import_KeepOriginal, importExists_CarefulDedupe)
		},
	}

	var cmdCatalog = &cobra.Command{
		Use:   "catalog [file to catalog]",
		Short: "add a file to the AudioFS catalog",
		Long:  `catalog will import all metadata into AudioFS, but does not import the actual audio stream. It does only keep a reference to the file provided.`,
		Args:  cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {
			lib.AddToCatalog(args[0])
		},
	}

	var cmdExists = &cobra.Command{
		Use:   "exists [file to check]",
		Short: "checks existence in the AudioFS catalog",
		Long:  `checks whether the precise file, or an equivalent audio stream is already in the AudioFS catalog`,
		Args:  cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {
			lib.Exists(args[0], importExists_CarefulDedupe)
		},
	}

	var cmdExport = &cobra.Command{
		Use:   "export [track_id] [destination file]",
		Short: "exports a track as AIFF",
		Long:  `exports a track as AIFF (even if the source was lossy)`,
		Args:  cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {
			fmt.Println("exists: " + strings.Join(args, " "))
		},
	}

	var cmdAnalyze = &cobra.Command{
		Use:   "analyze [file]",
		Short: "analyze a song",
		Long:  `analyzes a song and prints metadata about the file`,
		Args:  cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {
			val, err := native.GetMetadataFromFile(args[0])
			fmt.Printf("%+v\n", val)
			fmt.Printf("%+v\n", err)
			d, err := util.MarshallCompressed(val)
			fmt.Printf("%d\n", len(d))
			var a types.FileMetadata
			fmt.Printf("%+v\n", util.UnmarshallCompressed(d, &a))
		},
	}

	var cmdEcho = &cobra.Command{
		Use:   "echo [string to echo]",
		Short: "Echo anything to the screen",
		Long: `echo is for echoing anything back.
Echo works a lot like print, except it has a child command.`,
		Args: cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {
			fmt.Println("Print: " + strings.Join(args, " "))
		},
	}

	var cmdTimes = &cobra.Command{
		Use:   "times [# times] [string to echo]",
		Short: "Echo anything to the screen more times",
		Long: `echo things multiple times back to the user by providing
a count and a string.`,
		Args: cobra.MinimumNArgs(1),
		Run: func(cmd *cobra.Command, args []string) {

		},
	}

	config.Config.SetDefault("json_export.compression.algo", "zstd")
	config.Config.SetDefault("json_export.compression.level", 10)
	config.Config.SetDefault("import.file.keep_original", true)
	config.Config.SetDefault("import.catalog.keep_original", true)
	config.Config.SetDefault("import.file.careful_dedupe", true)
	config.Config.SetDefault("import.catalog.careful_dedupe", true)
	config.Config.SetDefault("check.careful_dedupe", true)
	config.Config.SetDefault("loglevel", "info")
	levelStr := config.Config.GetString("loglevel")
	level, err := logrus.ParseLevel(levelStr)
	if err != nil {
		logrus.Warnf("Configured `loglevel` '%s' is invalid. Using 'warning'", levelStr)
		level = logrus.WarnLevel
	}
	logrus.SetLevel(level)
	native.ApplyLogrusLevel()

	cmdImport.Flags().BoolVarP(&import_KeepOriginal, "keep", "k", true, "keep the original file")
	cmdImport.Flags().BoolVarP(&importExists_CarefulDedupe, "careful", "c", true, "only dedupe a file if PCM audio is bit-for-bit identical")
	cmdImportCatalog.Flags().BoolVarP(&import_KeepOriginal, "keep", "k", true, "keep the original file")
	cmdImportCatalog.Flags().BoolVarP(&importExists_CarefulDedupe, "careful", "c", true, "only dedupe a file if PCM audio is bit-for-bit identical")
	cmdExists.Flags().BoolVarP(&importExists_CarefulDedupe, "careful", "c", true, "also check if PCM audio is bit-for-bit identical")

	config.Config.Store()

	var rootCmd = &cobra.Command{Use: "audiofs-cli"}
	rootCmd.AddCommand(cmdAnalyze, cmdImport, cmdImportCatalog, cmdCatalog, cmdExists, cmdEcho, cmdExport)
	cmdEcho.AddCommand(cmdTimes)
	serve.Inject(rootCmd)
	rootCmd.Execute()
}
