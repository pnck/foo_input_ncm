## Playback and Meta Information Retrieval

- By currently observed, a `.ncm` file may contain duplicated meta tags, coming from both _the NCM file heder_ and the wrapped _audio content_. This component would take all of them and merge together to show distinct values.

- Other meta informations such as `codec`, `duration`, `bitrate`... are retrieved by the underlying decoder, typically `FLAC` or `MPEG` decoder. **And the decoder is also reading meta tags of the audio at the same time**. Thats the reason why both source of meta have to be dealed with.

- The audio content is **NOT fully decrypted in the memory at once**, but decrypt while reading on demand. This significantly reduces the memory usage, but slightly affects the speed of processing. It may be hard for compilers to optimize the `transform_view`, used in the implementation.

## Retagging

By design, the music meta information such as title/artist/album etc is embedded and re-encrypted into the `ncm` header. This decision was reached considering the following factors:

- The audio content isn't _"clean"_. Meaning if we take the embedded tag of the audio as overwrites, some music might be "polluted" by the tags at the original state.

- Audio content is not verified when played by _NCM player_. So there is no way to tell whether it's modified, nor to completely restore the file state.

- Replay gain is written to the audio content because it's recoverable and **simply tested** that the file hash will turn back after clearing the custom gain information.

- The meta information stored in the `ncm` header is essentially a standard stringified `JSON` object, allowing easy extension while preserving all original fields. Although modifying it would change its identification, causing the _NCM player_ to refuse to playback, it's totally recoverable by deleting the overwrites.
  - An `overwrite` key is **appended** to the `JSON` if user customized the meta tags. All changes would happen in the `overwrite` object.
  - **Clear all tags result in removing the `overwrite` key, which is actually equivalent to restore the file idntification.**

Currently, the downloaded `.ncm` files can be recognized and played by the _NCM player_ **IF**:

- It's put in a monitored directory.
- The album art or the replay gain is edited, while other meta information isn't.
- Meta tags are once edited, then cleared by `Remove tags` utility.

## Extractor

- The extraction menu is unavailable in old fb2k versions (v1.6), due to **API incompatity**.

- The extraction process is multi-threaded. Spawn order: `UI => extractor(x1) => worker(xN)`.

- The output file would be overwritten if existed, and no warning would be given in this case. See the comments at
  `ncm_file::save_raw_audio()`.

## The Behaviors of the _NCM Player_

- The meta info JSON is built and writen **AFTER** completing the download of the music file.

- The cover image is always retrived from the internet. The player will just ignore the embedded image.

- There may be some extra verifications to the identification （`163 key(Don't modify):xxxx`）. The player refuses to play even when off-line.

- The player actually recognize `.ncm` files by the identification, not by the decrypted meta info fields.
