## Retagging

By design, music meta information such as title/artist/album etc is embedded and re-encrypted into the `ncm` header. This decision was reached considering the following factors:

- The audio content isn't _"clean"_. Meaning if we take the embedded tag of the audio as overwrites, some music might be "polluted" by the tags at the original state.

- Audio content is not verified when played by _NCM player_. So there is no way to tell whether it's modified, nor to completely restore the file state.

- Replay gain is written to the audio content because it's recoverable and **simply tested** that the file hash will turn back after clearing the custom gain information.

- The meta information stored in the `ncm` header is essentially a standard stringified `JSON` object, allowing easy extension while preserving all original fields. Although modifying it would change the identification key, causing the _NCM player_ to refuse to playback, it's totally recoverable by deleting the overwrites.

Currently, the downloaded `.ncm` files can be recognized and played by the _NCM player_ if:

- It's put in a monitored directory.
- The album art or the replay gain is edited, while other meta information isn't.
- Other meta information is once edited, then cleared by `Remove tags` utility.

## Extractor

- The extraction menu is unavailable in old fb2k versions (v1.6), due to API incompatity.
- The extraction process is multi-threaded. Spawn order: `UI => extractor(x1) => worker(xN)`.
- The output file would be overwritten if existed, and no warning would be given in this case. See the comments at
  `ncm_file::save_raw_audio()`.
