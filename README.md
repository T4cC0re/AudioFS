# AudioFS
Store and deduplicate raw audio
---

###### Premises

- All audio which uses lossy compression will be stored verbatim
- Lossless audio based on PCM will be converted to highly compressed FLAC.
- Audio will be deduplicated based on [chromaprint](https://github.com/acoustid/chromaprint) fingerprints. Multiple formats will be kept if they were added.
  - e.g. if an MP3 was found, it will be kept.
  - two different PCM based files (e.g. one AIFF, one FLAC - both 16-bit) will be deduplicated if their fingerprints, bit-depth, and channellayout match.
  - There will be a "careful" dedupe mode, that will additionally check (for PCM files) that their raw audio is bit-for-bit identical. Testing will determine whether this is needed. Theoretically, the previous checks should be sufficient.
- All metadata will be archived inside AudioFS, so a symantically equivalent file can be reproduced.
- There will be a mode to just 'catalog' audio. This mode will create database entries, but not import the files into AudioFS. You can use this to check, if AudioFS' deduplication would work for you, or just to organize your collection.
- There will be a mode to check if a file's audio is already existent in the database.

Initial versions will shell out to ffprobe, ffmpeg and fpcalc to facilitate evaluation and conversion. This might be changed later to use dynamically linked version of those libraries.

###### Storage

AudioFS is planned to support a few different storage backends for audio storage.

- Filesystem:
  - Depends on an operating system provided filesystem to host the database files and the raw audio content.
- Object Storage:
  - Use an S3 compatible storage to store the database and raw audio content.
  - It might be required to have the database stored locally during changes. Implementation details are unclear at the moment.

The database will be facilitated via SQLite. This might change in the future, but a migration-path will be guaranteed in that case.

###### APIs

It is planned to support access to AudioFS for 3rd-party apps via the following APIs:

- Scriptable CLI - guaranteed
- HTTP API - guaranteed
- FUSE (Windows Support via [Dokany](https://github.com/dokan-dev/dokany) is planned) - planned, but might not materialize

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