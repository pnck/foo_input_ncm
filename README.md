## Play NCM files directly with our favourite
![screenshot](/screenshot.png)


### How to setup and build project

1. Download [foobar2000 SDK](https://www.foobar2000.org/SDK) and extract into vendor/sdk
2. Download WTL from [sourceforge](https://sourceforge.net/projects/wtl/) and extract into vendor/wtl
3. Ensure the project structure matches given example in vendor/README.md
4. Open fb2k_ncm.sln, there should be a main project (`foo_input_ncm`) and dependency projects listed (`pfc`, `foobar2000_SDK`, `foobar2000_sdk_helpers`, `foobar2000_component_client`).
5. Edit properties of `foobar2000_sdk_helpers`, goto `C/C++ -> General -> Additional Include Directories`, add wtl include directory. e.g. `../../../wtl/Include`
6. (optional) Edit properties of `foo_input_ncm`, switch to `Debug` profile, then goto `Debug`, edit the commandline and working directory to run foobar2000 host executable. Highly recommemded to install a standalone copy of foobar2000 into test/foobar2000, which is defaultly specified in debug profile.
7. (optional) You can also change the post-build command line to copy the built file to wherever you want.


### Special Thanks & Reference

- [foo_input_msu](https://github.com/qwertymodo/foo_input_msu) by [qwertymodo](https://github.com/qwertymodo)
- [foo_input_nemuc](https://github.com/hozuki/foo_input_nemuc) by [hozuki](https://github.com/hozuki)
- [foo_input_spotify](https://github.com/FauxFaux/foo_input_spotify) by [FauxFaux](https://github.com/FauxFaux)
- The very detailed [develop tutorio](http://yirkha.fud.cz/tmp/496351ef.tutorial-draft.html) for starters
- Official component samples included in [foobar2000 sdk](https://www.foobar2000.org/SDK)
  
- And the [ncmdump](https://github.com/anonymous5l/ncmdump) which encourages me doing a lot of research on the file structure