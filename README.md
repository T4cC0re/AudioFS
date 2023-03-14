# AudioFS
Store and deduplicate raw audio
---

###### Premises

- All audio which uses lossy compression will be stored verbatim
- Lossless audio based on PCM will be converted to either a: highly compressed FLAC, b: AIFF
- Audio will be deduplicated based on [chromaprint](https://github.com/acoustid/chromaprint) fingerprints. Multiple formats will be kept if they were added.
  - e.g. if an MP3 was found, it will be kept.
  - two different PCM based files (e.g. one AIFF, one FLAC - both 16-bit) will be deduplicated if their fingerprints, bit-depth, and channellayout match.
  - There will be a "careful" dedupe mode, that will additionally check (for PCM files) that their raw audio is bit-for-bit identical. Testing will determine whether this is needed. Theoretically, the previous checks should be sufficient.
- All metadata will be archived inside AudioFS, so a symantically equivalent file can be reproduced.
- There will be a mode to just 'catalog' audio. This mode will create database entries, but not import the files into AudioFS. You can use this to check, if AudioFS' deduplication would work for you, or just to organize your collection.
- There will be a mode to check if a file's audio is already existent in the database.
- There will be a mode to import files into AudioFS and delete the original file.
- on-demand file hierarchy that presents (tagged) content as .aiff files (or the compressed original where needed)

Initial versions will shell out to ffprobe, ffmpeg and fpcalc to facilitate evaluation and conversion. This might be changed later to use dynamically linked version of those libraries.

###### Storage

AudioFS is planned to support a few different storage backends for audio storage.

- Filesystem:
  - Depends on an operating system provided filesystem to host the database files and the raw audio content.
- Object Storage:
  - Use an S3 compatible storage to store the database and raw audio content.
  - It might be required to have the database stored locally during changes. Implementation details are unclear at the moment.
  - read-many write one principle. There will be a lock file in the basedir, that is a process specific ID. Will need to be overridden by force if unmounted uncleanly.

The database will be facilitated via SQLite. This might change in the future, but a migration-path will be guaranteed in that case.

###### APIs

It is planned to support access to AudioFS for 3rd-party apps via the following APIs:

- Scriptable CLI - guaranteed
- HTTP API/FCGI - guaranteed
- FUSE (Windows Support via [Dokany](https://github.com/dokan-dev/dokany) is planned) - planned, but might not materialize; possibly untagged files only - no on-demand transcode or conversion
- macOS replicated_file_provider_extension - planned

# Dev notes:

Identify unique recordings:
```
./fpcalc -length 0 -json ./Rolling\ In\ The\ Deep\ Metal.wav
```

Grab all metadata of a file:
```
ffprobe -i the.file -print_format json -show_private_data -show_entries 'stream:stream_tags:format:format_tags:program:program_tags:chapter'           
```

###### Fuse
https://github.com/durch/rust-s3

###### Fuse
https://github.com/dokan-dev/dokan-rust/pull/5
https://github.com/zargony/fuse-rs

###### macOS wrapping:
https://github.com/youngdynasty/go-swift  
Don’t use the NSFileProviderExtension class in macOS. Instead, create an NSObject subclass that adopts the NSFileProviderReplicatedExtension and NSFileProviderEnumerating protocols. For more information, see Replicated File Provider extension.  
https://developer.apple.com/documentation/fileprovider/nsfileproviderreplicatedextension  
https://developer.apple.com/documentation/fileprovider/nsfileproviderenumerating  
https://developer.apple.com/documentation/fileprovider/replicated_file_provider_extension  


###### AIFF Implementation:
https://web.archive.org/web/20221029201913/https%3A%2F%2Fwww.mmsp.ece.mcgill.ca%2FDocuments%2FAudioFormats%2FAIFF%2FAIFF.html  
https://web.archive.org/web/20221029201929/https%3A%2F%2Fwww.mmsp.ece.mcgill.ca%2FDocuments%2FAudioFormats%2FAIFF%2FDocs%2FMacOS_Sound-extract.pdf
AIFF on-the-fly implementation to be tested via HTTP stream output to ffmpeg
