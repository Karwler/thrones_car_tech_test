#pragma once

#include "utils/utils.h"
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#endif

class Settings {
public:
	enum class VSync : uint8 {
		immediate,
		synchronized,
		adaptive
	};
	static const array<string, sizet(VSync::adaptive)+1> vsyncNames;

	bool maximized, fullscreen;
	VSync vsync;
	vec2i resolution;
	string address;
	uint16 port;

public:
	Settings();

	int vsyncToInterval() const;
};

inline int Settings::vsyncToInterval() const {
	return vsync <= VSync::synchronized ? int(vsync) : -1;
}

enum FileType : uint8 {
	FTYPE_REG = 0x1,
	FTYPE_DIR = 0x2,
	FTYPE_OTH = 0x4,

	FTYPE_NON = 0x0,
	FTYPE_STD = 0x3
};

inline FileType operator~(FileType a) {
	return FileType(~uint8(a));
}

inline FileType operator&(FileType a, FileType b) {
	return FileType(uint8(a) & uint8(b));
}

inline FileType operator&=(FileType& a, FileType b) {
	return a = FileType(uint8(a) & uint8(b));
}

inline FileType operator^(FileType a, FileType b) {
	return FileType(uint8(a) ^ uint8(b));
}

inline FileType operator^=(FileType& a, FileType b) {
	return a = FileType(uint8(a) ^ uint8(b));
}

inline FileType operator|(FileType a, FileType b) {
	return FileType(uint8(a) | uint8(b));
}

inline FileType operator|=(FileType& a, FileType b) {
	return a = FileType(uint8(a) | uint8(b));
}

// handles all filesystem interactions
class FileSys {
public:
#ifdef _WIN32
	static constexpr char dirSavs[] = "saves\\";
	static constexpr char dirTexs[] = "textures\\";
#else
	static constexpr char dirSavs[] = "saves/";
	static constexpr char dirTexs[] = "textures/";
#endif
	static constexpr char extIni[] = ".ini";
private:
#ifdef _WIN32
	static constexpr sizet pathMax = 32767;
#else
	static constexpr char linkExe[] = "/proc/self/exe";
#endif
	static constexpr char fileSettings[] = "settings.ini";

	static constexpr char iniKeywordMaximized[] = "maximized";
	static constexpr char iniKeywordFullscreen[] = "fullscreen";
	static constexpr char iniKeywordResolution[] = "resolution";
	static constexpr char iniKeywordVsync[] = "vsync";
	static constexpr char iniKeywordAddress[] = "address";
	static constexpr char iniKeywordPort[] = "port";

#ifdef _WIN32		// os's font directories
	array<string, 2> dirFonts;
#else
	array<string, 3> dirFonts;
#endif
public:
	FileSys();

	Settings* loadSettings();
	bool saveSettings(const Settings* sets);

	static vector<string> listDir(const string& drc, FileType filter = FTYPE_STD);
	static bool createDir(const string& path);
	static FileType fileType(const string& file, bool readLink = true);

private:
	static vector<string> readFileLines(const string& file, bool printMessage = true);
	static string readTextFile(const string& file, bool printMessage = true);
	static bool writeTextFile(const string& file, const string& lines);
	static pairStr splitIniLine(const string& line);
	static string makeIniLine(const string& key, const string& val);

	static void setWorkingDir();
#ifdef _WIN32
	static string wgetenv(const string& name);
	static bool atrcmp(DWORD attrs, FileType filter);
#else
	static FileType stmtoft(const string& file, int (*statfunc)(const char*, struct stat*));
	static bool dtycmp(const string& drc, const dirent* entry, FileType filter, bool readLink);
#endif
};

inline bool FileSys::createDir(const string& path) {
#ifdef _WIN32
	return CreateDirectoryW(stow(path).c_str(), 0);
#else
	return !mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
}
#ifdef _WIN32
inline bool FileSys::atrcmp(DWORD attrs, FileType filter) {
	return attrs & FILE_ATTRIBUTE_DIRECTORY ? filter & FTYPE_DIR : filter & FTYPE_REG;
}
#else
inline FileType FileSys::fileType(const string& file, bool readLink) {
	return stmtoft(file, readLink ? stat : lstat);
}
#endif
inline string FileSys::makeIniLine(const string& key, const string& val) {
	return key + '=' + val + '\n';
}