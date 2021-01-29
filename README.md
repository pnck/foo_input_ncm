### How to setup and build project

1. Download [foobar2000 SDK](https://www.foobar2000.org/SDK) and extract into lib/sdk
2. Download WTL from [sourceforge](https://sourceforge.net/projects/wtl/) and extract into lib/wtl
3. Ensure the project structure matches given example in lib/README.md
4. Open fb2k_ncm.sln, there should be a main project (`foo_input_ncm`) and dependency projects listed (`pfc`, `foobar2000_SDK`, `foobar2000_sdk_helpers`, `foobar2000_component_client`).
5. Edit properties of `foobar2000_sdk_helpers`, goto `C/C++ -> General -> Additional Include Directories`, add wtl include directory. e.g. `../../../wtl/Include`
6. (optional) Edit properties of `foo_input_ncm`, switch to `Debug` profile, then goto `Debug`, edit the commandline and working directory to run foobar2000 host executable. Highly recommemded to install a standalone copy of foobar2000 into test/foobar2000, which is defaultly specified in debug profile.
7. (optional) You can also change the post-build command line to copy the built file to wherever you want.