#pragma once

// The following headers are dependents of Nvim. They are included
// here so their contents get declared inside the global namespace
// and not the nvim namespace.
#include <cstdio>
#include <locale.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>
#include <Winsock2.h>

namespace nvim
{
extern "C" {
// This is is normally defined with a compiler argument when Nvim is built.
#define INCLUDE_GENERATED_DECLARATIONS
// Avoid compiler errors caused by names that are C++ keywords.
#define _Bool bool
#define this this_

#include <nvim/api/ui.h>
#include <nvim/api/vim.h>
#include <nvim/buffer_defs.h>
#include <nvim/event/defs.h>
#include <nvim/globals.h>
#include <nvim/main.h>
#include <nvim/move.h>
#include <nvim/pos.h>
#include <nvim/screen.h>
#include <nvim/types.h>
#include <nvim/ui.h>
#include <nvim/window.h>

// Undefine macros with names of keywords so they may be used again.
#undef _Bool
#undef this
// Undefine the "inline" macro that was defined in os/win_defs.h.
#undef inline

// nvim/main.h does not have a definition for nvim_main.
int nvim_main(int argc, char** argv);
}
}
