#include "stdafx.h"

DECLARE_COMPONENT_VERSION(
"Ncm Loader",
"0.0.1",
"Load Netease Music specific file format (ncm) directly.\n"
);

// This will prevent users from renaming your component around (important for proper troubleshooter behaviors) or loading multiple instances of it.
VALIDATE_COMPONENT_FILENAME("foo_input_ncm.dll");

DECLARE_FILE_TYPE("Netease Music specific format files","*.ncm")