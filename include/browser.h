#pragma once
#include "ui.h"

// Minimal HTTP(S) text browser: fetches a page, strips it to readable text
// plus a numbered link list, paginates it on screen, and caches pages
// (to SD if present, else internal flash) so a revisit is instant.
// Blocks until the user taps the header back chip with no history left.
void browserRun();
