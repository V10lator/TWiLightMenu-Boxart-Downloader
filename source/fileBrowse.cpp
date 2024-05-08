#include "common.hpp"
#include "fileBrowse.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#define SCANNING_MSG "Scanning SD card for DS roms...\n\n(Press B to cancel)\n\n\n\n\n\n\n\n\n"
#define SCANNING_MSG_SIZE (strlen(SCANNING_MSG))

using namespace std;
extern bool continueNdsScan;
static char scanningMessage[512] = SCANNING_MSG;

/**
 * Get the title ID.
 * @param ndsFile DS ROM image.
 * @param buf Output buffer for title ID. (Must be at least 4 characters.)
 * @return 0 on success; non-zero on error.
 */
static void grabTID(const char *path, char *buf) {
	FILE *f = fopen(path, "rb");
	fseek(f, offsetof(sNDSHeadertitlecodeonly, gameCode), SEEK_SET);

	if(fread(buf, 4, 1, f) != 1)
		*(uint32_t *)buf = *(uint32_t *)"####";

	fclose(f);
}

static void showDirError(const char *path)
{
	string msg = "Unable to open the directory\n";
	msg += path;
	Msg::DisplayMsg(msg);
	for(uint i = 0; i < 60 * 5; ++i)
		gspWaitForVBlank();
}

void findNdsFiles(char *path, vector<DirEntry>& dirContents) {
	DIR *pdir = opendir(path);

	if (pdir == NULL) {
		showDirError(path);
		return;
	}

	strncpy(scanningMessage + SCANNING_MSG_SIZE, path, 512 - SCANNING_MSG_SIZE - 1);
	Msg::DisplayMsg(scanningMessage);

	char *ip = path + strlen(path);
	struct dirent* pent;
	DirEntry dirEntry;
	dirEntry.tid[4] = 0;
	uint32_t i;
	bool diff;
	while (continueNdsScan)
	{
		pent = readdir(pdir);
		if (pent == NULL)
			break;

		if(pent->d_name[0] == '.')
			continue;

		strcpy(ip, pent->d_name);
		dirEntry.name = pent->d_name;
		dirEntry.isDirectory = pent->d_type == DT_DIR;;

		if (dirEntry.isDirectory)
		{
			if(dirEntry.name.compare("3ds") != 0
			&& dirEntry.name.compare("DCIM") != 0
			&& dirEntry.name.compare("gm9") != 0
			&& dirEntry.name.compare("luma") != 0
			&& dirEntry.name.compare("Nintendo 3DS") != 0
			&& dirEntry.name.compare("private") != 0
			&& dirEntry.name.compare("retroarch") != 0) {
				strcat(ip, "/");
				findNdsFiles(path, dirContents);
			}
		} else if(dirEntry.name.length() >= 3 && strcasecmp(dirEntry.name.substr(dirEntry.name.length()-3, 3).c_str(), "nds") == 0) {
			// Get game's TID
			// char game_TID[5];
			grabTID(path, dirEntry.tid);
			if(*(uint32_t *)dirEntry.tid != *(uint32_t *)"####")
			{
				diff = true;
				for(i = 0; i < dirContents.size(); ++i)
				{
					diff = *(uint32_t *)dirContents[i].tid != *(uint32_t *)dirEntry.tid;
					if(!diff)
						break;
				}

				if(diff)
					dirContents.push_back(dirEntry);
			}
		}

		*ip = '\0';
	}
	closedir(pdir);
}

void getDirectoryContents (char *path, vector<DirEntry>& dirContents) {
	dirContents.clear();
	DIR *pdir = opendir(path);

	if (pdir == NULL) {
		showDirError(path);
		return;
	}

	DirEntry dirEntry;
	struct dirent* pent;

	while(true) {
		pent = readdir(pdir);
		if(pent == NULL)
			break;

		if(pent->d_name[0] == '.')
			continue;

		dirEntry.name = pent->d_name;
		dirEntry.isDirectory = pent->d_type == DT_DIR;

		dirContents.push_back (dirEntry);
	}

	closedir(pdir);
}
