#include "stdafx.h"

/**
    changelog

Version 0.3.0
- port to support multiple platform / architecture (x64/x86 Windows, Intel/ARM MacOS)

Version 0.2.0
- decode and play any ncm file
- correctly recognize meta info and album artworks

Version 0.1.0
- decode and play any ncm file
- meta info correctly recognized (without album artwork)

Version 0.0.2
- able to play specific content (flac/mp3) ncm file
- get basic file stats

Version 0.0.1
- file decryption ciphers
- loadable plugin stub

**/

#ifdef CURRENT_VERSION
#define VER_STR(X) #X
#define EXTRACT(X) VER_STR(X)
#define NCM_LOADER_VERSION EXTRACT(CURRENT_VERSION)
#else
#define NCM_LOADER_VERSION "0.3.1"
#endif

DECLARE_COMPONENT_VERSION("Ncm Loader", NCM_LOADER_VERSION, "Load and play Netease Music specific file format (ncm) directly.\n");

// This will prevent users from renaming your component around (important for proper troubleshooter behaviors) or
// loading multiple instances of it.
VALIDATE_COMPONENT_FILENAME("foo_input_ncm.dll");

DECLARE_FILE_TYPE("Netease Music specific format files", "*.ncm")

FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;
