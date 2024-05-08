#pragma once

#include "common.hpp"

#define APP_TITLE "TWLMenu Updater"

enum DownloadError {
	DL_ERROR_NONE = 0,
	DL_ERROR_WRITEFILE,
	DL_ERROR_ALLOC,
	DL_ERROR_STATUSCODE,
	DL_ERROR_GIT,
};

struct ThemeEntry {
	std::string downloadUrl;
	std::string name;
	std::string path;
	std::string sdPath;
};

/**
 * Download boxart from gametdb for all roms found on SD.
 */
void downloadBoxart(void);
