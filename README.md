[![Build All](https://github.com/pnck/foo_input_ncm/actions/workflows/build.yml/badge.svg)](https://github.com/pnck/foo_input_ncm/actions/workflows/build.yml)
![GitHub Release](https://img.shields.io/github/v/release/pnck/foo_input_ncm)
![GitHub Release Date](https://img.shields.io/github/release-date/pnck/foo_input_ncm)

## Play NCM files directly with our favorite

- On Windows
  ![screenshot-win](/docs/screenshot-win.png)

- Or on MacOS
  ![screenshot-mac](/docs/screenshot-mac.png)


### Features
- [x] Seamlessly load and play `.ncm` format files in _foobar2000_.
- [x] Fully integrated with the powerful built-in _`metadb`_ system, enabling efficient searching and strong management capabilities.
- [x] Unwrap/Convert `.ncm` to normal `.mp3` or `.flac` format with blazing speed.
- [x] Edit `replay gain`/`metadata`/`album art` in place.
  - *Will be unrecognizable by the _NCM player_ after editing tags. Reset it by `Remove tags` utility. (Properties - Tools - Remove tags)

### Compatibility

- [x] x64 foobar2000 on Windows (version >= v2.0)
- [x] Legacy x86 foobar2000 on Windows (version < v2.0)
- [x] foobar2000 for MacOS with component available (version >= v2.0)
- [x] ARM64 (M Serials) and Intel x64 MacOS are both supported.

### Installation

1. Download the released `fb2k-component` from the [Release Page](https://github.com/pnck/foo_input_ncm/releases/latest).
2. Install the component in **Preferences / Components**.
3. `.ncm` musics would be available for playing and shown in the library.

### Special Thanks & Reference

- [foo_input_msu](https://github.com/qwertymodo/foo_input_msu) by [qwertymodo](https://github.com/qwertymodo)
- [foo_input_nemuc](https://github.com/hozuki/foo_input_nemuc) by [hozuki](https://github.com/hozuki)
- [foo_input_spotify](https://github.com/FauxFaux/foo_input_spotify) by [FauxFaux](https://github.com/FauxFaux)
- The very detailed [development tutorial](http://yirkha.fud.cz/tmp/496351ef.tutorial-draft.html) for starters
- Official component samples included in [foobar2000 sdk](https://www.foobar2000.org/SDK)
- And the [ncmdump](https://github.com/anonymous5l/ncmdump) which encourages me to do a lot of research on the file structure
