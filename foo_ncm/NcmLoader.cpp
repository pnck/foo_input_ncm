#include "stdafx.h"
#include "NcmLoader.h"


void NcmLoader::on_init()
{
    FB2K_console_formatter() << "what?";
}

static initquit_factory_t<NcmLoader> g_what;