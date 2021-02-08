#include "stdafx.h"

/**
    changelog

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

DECLARE_COMPONENT_VERSION("Ncm Loader", "0.1.0", "Load and play Netease Music specific file format (ncm) directly.\n");

// This will prevent users from renaming your component around (important for proper troubleshooter behaviors) or
// loading multiple instances of it.
VALIDATE_COMPONENT_FILENAME("foo_input_ncm.dll");

DECLARE_FILE_TYPE("Netease Music specific format files", "*.ncm")
