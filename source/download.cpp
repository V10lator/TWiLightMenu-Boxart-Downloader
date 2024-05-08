#include "animation.hpp"
#include "cia.hpp"
#include "download.hpp"
#include "extract.hpp"
#include "fileBrowse.hpp"
#include "formatting.hpp"
#include "gui.hpp"
#include "inifile.h"
#include "keyboard.h"
#include "thread.h"

#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <unistd.h>

#define  USER_AGENT   APP_TITLE "-" VERSION_STRING

bool continueNdsScan = true;

static char downloadMessage[] = "Downloading\nsdmc:/_nds/TWiLightMenu/boxart/####.png";
static char *boxartPath = downloadMessage + 12;
static char boxartUrl[128] = "https://art.gametdb.com/ds/coverS/";
static CURL *hnd;

curl_off_t downloadTotal;
curl_off_t downloadNow;

static volatile FILE *downfile = NULL;
static volatile size_t file_buffer_pos;
static char* g_buffers[2];
static volatile uint g_index;
static Thread fsCommitThread;
static LightEvent readyToCommit;
static LightEvent waitCommit;
static volatile bool fileThreadRunning;
static volatile bool writeError;
#define FILE_ALLOC_SIZE 0x60000

u32 *socubuf;

static int curlProgress(CURL *hnd,
					curl_off_t dltotal, curl_off_t dlnow,
					curl_off_t ultotal, curl_off_t ulnow)
{
	downloadTotal = dltotal;
	downloadNow = dlnow;

	return 0;
}

static bool filecommit() {
	if(file_buffer_pos == 0)
		return true;

	if(downfile == NULL || fwrite(g_buffers[!g_index], file_buffer_pos, 1, (FILE *)downfile) != 1)
		return false;

	file_buffer_pos = 0;
	return true;
}

static void commitToFileThreadFunc(void* args)
{
	do
	{
		LightEvent_Signal(&waitCommit);

		LightEvent_Wait(&readyToCommit);
		LightEvent_Clear(&readyToCommit);

		writeError = !filecommit();
	}
	while(fileThreadRunning);

	threadExit(0);
}

static size_t file_handle_data(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	(void)userdata;
	if (writeError || !size || !nmemb)
		return 0;

	size *= nmemb;
	size_t tofill;

	if(file_buffer_pos + size < FILE_ALLOC_SIZE)
		tofill = 0;
	else
	{
		tofill = FILE_ALLOC_SIZE - file_buffer_pos;
		memcpy(g_buffers[g_index] + file_buffer_pos, ptr, tofill);

		LightEvent_Wait(&waitCommit);
		LightEvent_Clear(&waitCommit);
		g_index = !g_index;
		LightEvent_Signal(&readyToCommit);
	}

	memcpy(g_buffers[g_index] + file_buffer_pos, ptr + tofill, size - tofill);
	file_buffer_pos += size - tofill;
	return size;
}

static bool initDownloader()
{
	if(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK)
	{
		hnd = curl_easy_init();
		if(hnd != NULL)
		{
			if(curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, FILE_ALLOC_SIZE) == CURLE_OK &&
				curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 0L) == CURLE_OK &&
				curl_easy_setopt(hnd, CURLOPT_USERAGENT, USER_AGENT) == CURLE_OK &&
				curl_easy_setopt(hnd, CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK &&
				curl_easy_setopt(hnd, CURLOPT_FAILONERROR, 1L) == CURLE_OK &&
				curl_easy_setopt(hnd, CURLOPT_ACCEPT_ENCODING, "") == CURLE_OK &&
				curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L) == CURLE_OK &&
				curl_easy_setopt(hnd, CURLOPT_XFERINFOFUNCTION, curlProgress) == CURLE_OK &&
				curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, file_handle_data) == CURLE_OK &&
				curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L) == CURLE_OK //&&
//				curl_easy_setopt(hnd, CURLOPT_VERBOSE, 1L) == CURLE_OK &&
//				curl_easy_setopt(hnd, CURLOPT_STDERR, stdout) == CURLE_OK
			)
			{
				g_buffers[0] = (char*)memalign(0x1000, FILE_ALLOC_SIZE);
				if(g_buffers[0] != NULL)
				{
					g_buffers[1] = (char*)memalign(0x1000, FILE_ALLOC_SIZE);
					if(g_buffers[1] != NULL)
					{
						socubuf = memalign(0x1000, 0x100000);
						if(socubuf != NULL)
						{
							if(!R_FAILED(socInit(socubuf, 0x100000)))
							{
								LightEvent_Init(&waitCommit, RESET_STICKY);
								LightEvent_Init(&readyToCommit, RESET_STICKY);

								s32 prio = 0;
								svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
								fileThreadRunning = true;
								fsCommitThread = threadCreate(commitToFileThreadFunc, NULL, 0x1000, prio - 1, -2, true);

								if(fsCommitThread != NULL)
									return true;

								socExit();
							}

							free(socubuf);
						}

						free(g_buffers[1]);
					}

					free(g_buffers[0]);
				}
			}

			curl_easy_cleanup(hnd);
		}

		curl_global_cleanup();
	}

	return false;
}

static void deinitDownloader()
{
	curl_easy_cleanup(hnd);
	curl_global_cleanup();

	fileThreadRunning = false;
	LightEvent_Signal(&readyToCommit);
	threadJoin(fsCommitThread, U64_MAX);

	free(g_buffers[0]);
	free(g_buffers[1]);

	socExit();
	free(socubuf);
}

static Result downloadToFile() {

	Result retcode = 0;
	CURLcode cres;

	downloadTotal = 1;
	downloadNow = 0;
	g_index = 0;
	file_buffer_pos = 0;
	writeError = false;

//	printf("Downloading from:\n%s\nto:\n%s\n", boxartUrl, boxartPath);

	downfile = fopen(boxartPath, "wb");
	if (!downfile) {
		retcode = -2;
		goto exit;
	}

	curl_easy_setopt(hnd, CURLOPT_URL, boxartUrl);
	cres = curl_easy_perform(hnd);
	if (cres != CURLE_OK) {
		retcode = -cres;
		goto exit;
	}

	LightEvent_Wait(&waitCommit);
	LightEvent_Clear(&waitCommit);
	g_index = !g_index;
	if (!filecommit())
		retcode = -3;
	
exit:
	if(downfile != NULL)
	{
		fclose((FILE *)downfile);
		downfile = NULL;

		if(retcode != 0)
			deleteFile(boxartPath);
	}
	
	LightEvent_Signal(&readyToCommit);
	return retcode;
}

/**
 * Check Wi-Fi status.
 * @return True if Wi-Fi is connected; false if not.
 */
static bool checkWifiStatus(void)
{
	u32 wifiStatus;
	return R_SUCCEEDED(ACU_GetWifiStatus(&wifiStatus)) && wifiStatus;
}

static const char* getBoxartRegion(char tid_region) {
	// European boxart languages.
	static const char *const ba_langs_eur[] = {
		"EN",	// Japanese (English used in place)
		"EN",	// English
		"FR",	// French
		"DE",	// German
		"IT",	// Italian
		"ES",	// Spanish
		"ZHCN",	// Simplified Chinese
		"KO",	// Korean
	};
	CIniFile ini("sdmc:/_nds/TWiLightMenu/settings.ini");
	int language = ini.GetInt("SRLOADER", "LANGUAGE", -1);
	const char *ba_region;

	switch (tid_region) {
		case 'E':
		case 'T':
			ba_region = "US";	// USA
			break;
		case 'J':
			ba_region = "JA";	// Japanese
			break;
		case 'K':
			ba_region = "KO";	// Korean
			break;

		case 'O':			// USA/Europe
			// if(region == CFG_REGION_USA) {
				// System is USA region.
				// Get the USA boxart if it's available.
				ba_region = "EN";
				break;
			// }
			// fall-through
		case 'P':			// Europe
		default:
			// System is not USA region.
			// Get the European boxart that matches the system's language.
			// TODO: Check country code for Australia.
			// This requires parsing the Config savegame. (GetConfigInfoBlk2())
			// Reference: https://3dbrew.org/wiki/Config_Savegame
			if(language == -1)
				ba_region = "EN";	// TODO: make this actually set to the console's language
			else
				ba_region = ba_langs_eur[language];
			break;

		case 'U':
			// Alternate country code for Australia.
			ba_region = "AU";
			break;

		// European country-specific localizations.
		case 'D':
			ba_region = "DE";	// German
			break;
		case 'F':
			ba_region = "FR";	// French
			break;
		case 'H':
			ba_region = "NL";	// Dutch
			break;
		case 'I':
			ba_region = "IT";	// Italian
			break;
		case 'R':
			ba_region = "RU";	// Russian
			break;
		case 'S':
			ba_region = "ES";	// Spanish
			break;
		case '#':
			ba_region = "HB"; // Homebrew
			break;
	}
	return ba_region;
}

static void scanToCancelBoxArt(void) {
	while(continueNdsScan) {
		hidScanInput();
		if(hidKeysDown() & KEY_B) {
			continueNdsScan = false;
		}
		gspWaitForVBlank();
	}
}

void downloadBoxart(void)
{
	static char path[4096]; // TODO
	vector<DirEntry> dirContents;
	u32 keys;

	Msg::DisplayMsg("Would you like to choose a directory, or scan\nthe full card?\n\n\n\n\n\n\n\n\n\n Cancel    Choose Directory    Full SD");

	while(true)
	{
		gspWaitForVBlank();
		hidScanInput();
		keys = hidKeysDown();

		if(keys & KEY_A) {
			strcpy(path, "sdmc:/");
			bool dirChosen = false;
			uint selectedDir = 0;
			uint ds;
			while(!dirChosen) {
				getDirectoryContents(path, dirContents);
//				removeFiles(dirContents);
				ds = dirContents.size();

				while(true)
				{
					//gspWaitForVBlank();
					hidScanInput();
					keys = hidKeysDown();
					if(keys & KEY_A) {
						if(dirContents[selectedDir].isDirectory)
						{
							strcat(path, (dirContents[selectedDir].name + "/").c_str());
							selectedDir = 0;
						}

						break;
					}
					if(keys & KEY_B) {
						for(char *c = path + strlen(path) - 2; c != path; --c)
						{
							if(*c == '/')
							{
								*++c = '\0';
								dirChosen = true;
								break;
							}
						}

						if(dirChosen)
						{
							selectedDir = 0;
							dirChosen = false;
							break;
						}

						return;
					}
					if(keys & KEY_X) {
						dirChosen = true;
						break;
					}

					keys = hidKeysDownRepeat();
					if(keys & KEY_UP) {
						if(selectedDir == 0)
							selectedDir = ds;

						if(selectedDir != 0)
							--selectedDir;
					} else if(keys & KEY_DOWN) {
						if(ds > 1)
						{
							if(++selectedDir == ds)
								selectedDir = 0;
						}
					} else if(keys & KEY_LEFT) {
						selectedDir -= 10;
						if(selectedDir < 0) {
							selectedDir = 0;
						}
					} else if(keys & KEY_RIGHT) {
						selectedDir += 10;
						if(selectedDir > ds) {
							selectedDir = ds-1;
						}
					}

					std::string dirs;
					for(uint i=(selectedDir<10) ? 0 : selectedDir-10;i<ds&&i<((selectedDir<10) ? 11 : selectedDir+1);i++) {
						dirs += i == selectedDir ? "> " : "  ";
						dirs += dirContents[i].name;
						dirs += dirContents[i].isDirectory ? "/\n" :  "\n";
					}
					for(uint i=0;i<((ds<10) ? 11-ds : 0);i++) {
						dirs += "\n";
					}
					dirs += " Back    Open    Choose current";
					Msg::DisplayMsg(dirs.c_str());
				}
			}
			break;
		}
		if(keys & KEY_X) {
			strcpy(path, "sdmc:/");
			break;
		}
		if(keys & KEY_B)
			return;
	}

	Msg::DisplayMsg("Scanning SD card for DS roms...\n\n(Press  to cancel)");

	continueNdsScan = true;
	createThread((ThreadFunc)scanToCancelBoxArt);
	dirContents.clear();
	findNdsFiles(path, dirContents);
	continueNdsScan = false;

	if(!checkWifiStatus())
	{
		Msg::DisplayMsg("No WiFi!\n\n(Press  to exit)");
		do
		{
			hidScanInput();
			if(hidKeysDown() & KEY_B)
				return;

			gspWaitForVBlank();
		}
		while(!checkWifiStatus());
	}

	if(initDownloader())
	{
		for(uint i = 0; i < dirContents.size(); ++i)
		{
			*(uint32_t *)(boxartPath + 31) = *(uint32_t *)dirContents[i].tid;
			if(access(boxartPath, F_OK) != 0)
			{
				Msg::DisplayMsg(downloadMessage);
				sprintf(boxartUrl + 34, "%s/%s.png", getBoxartRegion(dirContents[i].tid[3]), dirContents[i].tid);
				downloadToFile();
			}
		}

		deinitDownloader();
	}

	Msg::DisplayMsg("Done!");
	do
	{
		gspWaitForVBlank();
		hidScanInput();
	}
	while(!hidKeysDown());
}
