#include "fileSys.h"
#include <map>
#ifndef _WIN32
#include <unistd.h>
#endif

// SETTINGS

const array<string, Settings::screenNames.size()> Settings::screenNames = {
	"window",
	"borderless",
	"fullscreen",
	"desktop"
};

const array<string, Settings::vsyncNames.size()> Settings::vsyncNames = {
	"adaptive",
	"immediate",
	"synchronized"
};

const array<string, Settings::smoothNames.size()> Settings::smoothNames = {
	"off",
	"fast",
	"nice"
};

Settings::Settings() :
	maximized(false),
	screen(Screen::window),
	vsync(VSync::synchronized),
	smooth(Smooth::nice),
	size(800, 600),
	mode({ SDL_PIXELFORMAT_RGB888, 1920, 1080, 60, nullptr }),
	address(loopback),
	port(Com::defaultPort)
{}

// FILE SYS

FileSys::FileSys() {
	// check if all (more or less) necessary files and directories exist
	if (setWorkingDir())
		std::cerr << "failed to set working directory" << std::endl;
	if (fileType(dirTexs) != FTYPE_DIR)
		std::cerr << "failed to find texture directory" << std::endl;
}

Settings* FileSys::loadSettings() {
	Settings* sets = new Settings();
	for (const string& line : readTextFile(fileSettings)) {
		if (pairStr il = readIniLine(line); il.first == iniKeywordMaximized)
			sets->maximized = stob(il.second);
		else if (il.first == iniKeywordScreen)
			sets->screen = valToEnum<Settings::Screen>(Settings::screenNames, il.second);
		else if (il.first == iniKeywordSize)
			sets->size = vec2i::get(il.second, strtoul, 0);
		else if (il.first == iniKeywordMode)
			sets->mode = strToDisp(il.second);
		else if (il.first == iniKeywordVsync)
			sets->vsync = Settings::VSync(valToEnum<int8>(Settings::vsyncNames, il.second) - 1);
		else if (il.first == iniKeywordSmooth)
			sets->smooth = valToEnum<Settings::Smooth>(Settings::smoothNames, il.second);
		else if (il.first == iniKeywordAddress)
			sets->address = il.second;
		else if (il.first == iniKeywordPort)
			sets->port = uint16(stoul(il.second));
	}
	return sets;
}

bool FileSys::saveSettings(const Settings* sets) {
	string text;
	text += makeIniLine(iniKeywordMaximized, btos(sets->maximized));
	text += makeIniLine(iniKeywordScreen, Settings::screenNames[uint8(sets->screen)]);
	text += makeIniLine(iniKeywordSize, sets->size.toString());
	text += makeIniLine(iniKeywordMode, dispToStr(sets->mode));
	text += makeIniLine(iniKeywordVsync, Settings::vsyncNames[uint8(sets->vsync)+1]);
	text += makeIniLine(iniKeywordSmooth, Settings::smoothNames[uint8(sets->smooth)]);
	text += makeIniLine(iniKeywordAddress, sets->address);
	text += makeIniLine(iniKeywordPort, to_string(sets->port));
	return writeTextFile(fileSettings, text);
}

Object FileSys::loadObj(const string& file) {
	vector<string> lines = readTextFile(file);
	if (lines.empty())
		return Object();

	// collect vertex positions, uv data, normals and face v/u/n-indices
	vector<array<int, 9>> faces;
	vector<vec3> poss;
	vector<vec2> uvs;
	vector<vec3> norms;
	for (const string& line : lines) {
		if (!line.compare(0, 1, "v"))
			poss.emplace_back(vtog<vec3>(stov(&line[1], vec3::length(), strtof, 0.f)));
		else if (!line.compare(0, 2, "vt"))
			uvs.emplace_back(vtog<vec2>(stov(&line[2], vec2::length(), strtof, 0.f)));
		else if (!line.compare(0, 2, "vn"))
			norms.emplace_back(vtog<vec3>(stov(&line[1], vec3::length(), strtof, 0.f)));
		else if (!line.compare(0, 1, "f"))
			faces.emplace_back(readFace(&line[1]));
	}

	// convert collected data to interleaved vertex information format
	vector<Vertex> verts;
	vector<ushort> elems;
	std::map<array<int, 3>, ushort> vmap;
	for (array<int, 9>& it : faces)
		for (sizet i = 0; i < 3; i++) {
			if (array<int, 3>& vp = reinterpret_cast<array<int, 3>*>(it.data())[i]; !vmap.count(vp)) {
				vmap.emplace(vp, ushort(verts.size()));
				elems.push_back(ushort(verts.size()));
				verts.emplace_back(resolveObjId(vp[0], poss), glm::normalize(resolveObjId(vp[2], norms)), resolveObjId(vp[1], uvs));
			} else
				elems.push_back(vmap[vp]);
		}
	return Object(vec3(0.f), vec3(0.f), vec3(1.f), verts, elems);
}

array<int, 9> FileSys::readFace(const char* str) {
	array<int, 9> face;
	for (uint i = 0; i < 3; i++)
		for (uint j = 0; j < 3; j++) {
			char* end;
			face[i * 3 + j] = int(strtol(str, &end, 0));
			str = str != end ? end : str + 1;

			if (*str == '/')
				str++;
			if (isSpace(*str) && j < 2) {
				std::fill_n(face.data() + i * 3 + j, 3 - j, 0);
				break;
			}
		}
	return face;
}

vector<string> FileSys::listDir(const string& drc, FileType filter) {
	vector<string> entries;
#ifdef _WIN32
	WIN32_FIND_DATAW data;
	if (HANDLE hFind = FindFirstFileW(stow(appDsep(drc) + "*").c_str(), &data); hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!isDotName(data.cFileName) && atrcmp(data.dwFileAttributes, filter))
				entries.emplace_back(wtos(data.cFileName));
		} while (FindNextFileW(hFind, &data));
		FindClose(hFind);
	}
#else
	if (DIR* directory = opendir(drc.c_str())) {
		while (dirent* entry = readdir(directory))
			if (!isDotName(entry->d_name) && dtycmp(drc, entry, filter, true))
				entries.emplace_back(entry->d_name);
		closedir(directory);
	}
#endif
	std::sort(entries.begin(), entries.end());
	return entries;
}

int FileSys::setWorkingDir() {
	char* path = SDL_GetBasePath();
	if (!path)
		return 1;
#ifdef _WIN32
	int err = _wchdir(stow(path).c_str());
#else
	int err = chdir(path);
#endif
	SDL_free(path);
	return err;
}
#ifdef _WIN32
FileType FileSys::fileType(const string& file, bool readLink) {
	DWORD attrib = GetFileAttributesW(stow(file).c_str());
	if (attrib == INVALID_FILE_ATTRIBUTES)
		return FTYPE_NON;
	if (attrib & FILE_ATTRIBUTE_DIRECTORY)
		return FTYPE_DIR;
	return FTYPE_REG;
}
#else
FileType FileSys::stmtoft(const string& file, int (*statfunc)(const char*, struct stat*)) {
	struct stat ps;
	if (statfunc(file.c_str(), &ps))
		return FTYPE_NON;

	switch (ps.st_mode & S_IFMT) {
	case S_IFDIR:
		return FTYPE_DIR;
	case S_IFREG:
		return FTYPE_REG;
	}
	return FTYPE_OTH;
}

bool FileSys::dtycmp(const string& drc, const dirent* entry, FileType filter, bool readLink) {
	switch (entry->d_type) {
	case DT_DIR:
		return filter & FTYPE_DIR;
	case DT_REG:
		return filter & FTYPE_REG;
	case DT_LNK:
		return filter & (readLink ? stmtoft(drc + entry->d_name, stat) : FTYPE_OTH);
	case DT_UNKNOWN:
		return filter & stmtoft(drc + entry->d_name, readLink ? stat : lstat);
	}
	return filter & FTYPE_OTH;
}
#endif
