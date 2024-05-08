#ifndef FILE_BROWSE_HPP
#define FILE_BROWSE_HPP

#include <sys/stat.h>
#include <dirent.h>
#include <vector>

using namespace std;

struct DirEntry {
	string name;
	bool isDirectory;
	char tid[5];
};

typedef struct {
	char gameTitle[12];			//!< 12 characters for the game title.
	char gameCode[4];			//!< 4 characters for the game code.
} sNDSHeadertitlecodeonly;

void findNdsFiles(char *path, vector<DirEntry>& dirContents);

void getDirectoryContents (char *path, vector<DirEntry>& dirContents);

#endif //FILE_BROWSE_HPP
