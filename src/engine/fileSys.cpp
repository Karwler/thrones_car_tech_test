#include "world.h"

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

Settings::Settings() :
	maximized(false),
	avolume(0),
	display(0),
#ifdef __ANDROID__
	screen(Screen::desktop),
#else
	screen(Screen::window),
#endif
	vsync(VSync::synchronized),
	msamples(4),
	texScale(100),
#ifdef OPENGLES
	shadowRes(0),
#else
	shadowRes(1024),
#endif
	softShadows(true),
	gamma(1.f),
	size(1280, 720),
	mode{ SDL_PIXELFORMAT_RGB888, 1920, 1080, 60, nullptr },
	scaleTiles(true),
	scalePieces(false),
	chatLines(511),
	fontRegular(true),
	address(loopback),
	port(Com::defaultPort)
{}

// SETUP

void Setup::clear() {
	tiles.clear();
	mids.clear();
	pieces.clear();
}

// FILE SYS

string FileSys::dirData, FileSys::dirConfig;

void FileSys::init() {
#if defined(__ANDROID__)
	if (const char* path = SDL_AndroidGetExternalStoragePath())
		dirConfig = path + string("/");
#elif defined(EMSCRIPTEN)
	dirData = "/";
#else
	const char* eopt = World::args.getOpt(World::argExternal);
#ifdef EXTERNAL
	bool external = eopt ? stob(eopt) : true;
#else
	bool external = eopt ? stob(eopt) : false;
#endif
	if (char* path = SDL_GetBasePath()) {
#ifdef __APPLE__
		dirData = path;
#else
		dirData = path + string("data/");
#endif
		if (SDL_free(path); !external)
			dirConfig = dirData;
	}
#if defined(_WIN32)
	if (const char* path = SDL_getenv("AppData"); external && path) {
		dirConfig = path + string("/Thrones/");
		std::replace(dirConfig.begin(), dirConfig.end(), '\\', '/');
		createDir(dirConfig);
	}
#elif defined(__APPLE__)
	if (const char* path = SDL_getenv("HOME"); external && path) {
		dirConfig = path + string("/Library/Preferences/Thrones/");
		createDir(dirConfig);
	}
#elif !defined(EMSCRIPTEN)
	if (const char* path = SDL_getenv("HOME"); external && path) {
		dirConfig = path + string("/.config/thrones/");
		createDir(dirConfig);
	}
#endif
#endif
}

Settings* FileSys::loadSettings() {
	Settings* sets = new Settings();
#ifndef EMSCRIPTEN
	for (const string& line : readFileLines(configPath(fileSettings))) {
		pairStr il = readIniLine(line);
#ifndef __ANDROID__
		if (il.first == iniKeywordMaximized)
			sets->maximized = stob(il.second);
		else if (il.first == iniKeywordDisplay)
			sets->display = uint8(std::clamp(sstoul(il.second), 0ul, ulong(SDL_GetNumVideoDisplays())));
		else if (il.first == iniKeywordScreen)
			sets->screen = strToEnum<Settings::Screen>(Settings::screenNames, il.second);
		else if (il.first == iniKeywordSize)
			sets->size = stoiv<ivec2>(il.second.c_str(), strtoul);
		else if (il.first == iniKeywordMode)
			sets->mode = strToDisp(il.second);
		else
#endif
#ifndef OPENGLES
		if (il.first == iniKeywordMsamples)
			sets->msamples = uint8(std::clamp(sstoul(il.second), 0ul, 8ul));
		else if (il.first == iniKeywordShadowRes)
			sets->shadowRes = uint16(std::clamp(sstoul(il.second), 0ul, ulong(Settings::shadowResMax)));
		else if (il.first == iniKeywordSoftShadows)
			sets->softShadows = stob(il.second);
		else
#endif
		if (il.first == iniKeywordTexScale)
			sets->texScale = uint8(std::clamp(sstoul(il.second), 1ul, 100ul));
		else if (il.first == iniKeywordVsync)
			sets->vsync = Settings::VSync(strToEnum<int8>(Settings::vsyncNames, il.second) - 1);
		else if (il.first == iniKeywordGamma)
			sets->gamma = std::clamp(sstof(il.second), 0.f, Settings::gammaMax);
		else if (il.first == iniKeywordAVolume)
			sets->avolume = uint8(std::clamp(sstoul(il.second), 0ul, ulong(SDL_MIX_MAXVOLUME)));
		else if (il.first == iniKeywordScaleTiles)
			sets->scaleTiles = stob(il.second);
		else if (il.first == iniKeywordScalePieces)
			sets->scalePieces = stob(il.second);
		else if (il.first == iniKeywordChatLines)
			sets->chatLines = uint16(std::clamp(stoul(il.second), 0ul, ulong(Settings::chatLinesMax)));
		else if (il.first == iniKeywordFontRegular)
			sets->fontRegular = stob(il.second);
		else if (il.first == iniKeywordAddress)
			sets->address = il.second;
		else if (il.first == iniKeywordPort)
			sets->port = uint16(sstoul(il.second));
	}
#endif
	return sets;
}

bool FileSys::saveSettings(const Settings* sets) {
#ifdef EMSCRIPTEN
	return true;
#else
	string text;
#ifndef __ANDROID__
	text += makeIniLine(iniKeywordMaximized, btos(sets->maximized));
	text += makeIniLine(iniKeywordDisplay, toStr(sets->display));
	text += makeIniLine(iniKeywordScreen, Settings::screenNames[uint8(sets->screen)]);
	text += makeIniLine(iniKeywordSize, toStr(sets->size));
	text += makeIniLine(iniKeywordMode, dispToStr(sets->mode));
#endif
#ifndef OPENGLES
	text += makeIniLine(iniKeywordMsamples, toStr(sets->msamples));
	text += makeIniLine(iniKeywordShadowRes, toStr(sets->shadowRes));
	text += makeIniLine(iniKeywordSoftShadows, btos(sets->softShadows));
#endif
	text += makeIniLine(iniKeywordTexScale, toStr(sets->texScale));
	text += makeIniLine(iniKeywordVsync, Settings::vsyncNames[uint8(int8(sets->vsync)+1)]);
	text += makeIniLine(iniKeywordGamma, toStr(sets->gamma));
	text += makeIniLine(iniKeywordAVolume, toStr(sets->avolume));
	text += makeIniLine(iniKeywordScaleTiles, btos(sets->scaleTiles));
	text += makeIniLine(iniKeywordScalePieces, btos(sets->scalePieces));
	text += makeIniLine(iniKeywordChatLines, toStr(sets->chatLines));
	text += makeIniLine(iniKeywordFontRegular, btos(sets->fontRegular));
	text += makeIniLine(iniKeywordAddress, sets->address);
	text += makeIniLine(iniKeywordPort, toStr(sets->port));
	return writeFile(configPath(fileSettings), text);
#endif
}

umap<string, Com::Config> FileSys::loadConfigs(const char* file) {
	umap<string, Com::Config> confs;
#ifndef EMSCRIPTEN
	Com::Config* cit = nullptr;
	for (const string& line : readFileLines(configPath(file))) {
		if (string title = readIniTitle(line); !title.empty())
			cit = &confs.emplace(std::move(title), Com::Config()).first->second;
		else if (cit) {
			if (pairStr it = readIniLine(line); !strcicmp(it.first, iniKeywordBoardSize))
				cit->homeSize = stoiv<svec2>(it.second.c_str(), strtol);
			else if (!strcicmp(it.first, iniKeywordSurvivalPass))
				cit->survivalPass = uint8(sstol(it.second));
			else if (!strcicmp(it.first, iniKeywordSurvivalMode))
				cit->survivalMode = strToEnum<Com::Config::Survival>(Com::Config::survivalNames, it.second);
			else if (!strcicmp(it.first, iniKeywordFavorLimit))
				cit->favorLimit = stob(it.second);
			else if (!strcicmp(it.first, iniKeywordFavorMax))
				cit->favorMax = uint8(sstol(it.second));
			else if (!strcicmp(it.first, iniKeywordDragonDist))
				cit->dragonDist = uint8(sstol(it.second));
			else if (!strcicmp(it.first, iniKeywordDragonSingle))
				cit->dragonSingle = stob(it.second);
			else if (!strcicmp(it.first, iniKeywordDragonDiag))
				cit->dragonDiag = stob(it.second);
			else if (sizet len = strlen(iniKeywordTile); !strncicmp(it.first, iniKeywordTile, len))
				readAmount(it, len, Com::tileNames, cit->tileAmounts);
			else if (len = strlen(iniKeywordMiddle); !strncicmp(it.first, iniKeywordMiddle, len))
				readAmount(it, len, Com::tileNames, cit->middleAmounts);
			else if (len = strlen(iniKeywordPiece); !strncicmp(it.first, iniKeywordPiece, len))
				readAmount(it, len, Com::pieceNames, cit->pieceAmounts);
			else if (!strcicmp(it.first, iniKeywordWinFortress))
				cit->winFortress = uint16(sstol(it.second));
			else if (!strcicmp(it.first, iniKeywordWinThrone))
				cit->winThrone = uint16(sstol(it.second));
			else if (!strcicmp(it.first, iniKeywordCapturers))
				cit->readCapturers(it.second);
			else if (!strcicmp(it.first, iniKeywordShift))
				readShift(it.second, *cit);
		}
	}
#endif
	return !confs.empty() ? confs : umap<string, Com::Config>{ pair(Com::Config::defaultName, Com::Config()) };
}

template <sizet N, sizet S>
void FileSys::readAmount(const pairStr& it, sizet wlen, const array<string, N>& names, array<uint16, S>& amts) {
	if (uint8 id = strToEnum<uint8>(names, it.first.substr(wlen)); id < amts.size())
		amts[id] = uint16(sstol(it.second));
}

void FileSys::readShift(const string& line, Com::Config& conf) {
	for (const char* pos = line.c_str(); *pos;) {
		if (string word = readWordM(pos); !strcicmp(word, iniKeywordLeft))
			conf.shiftLeft = true;
		else if (!strcicmp(word, iniKeywordRight))
			conf.shiftLeft = false;
		else if (!strcicmp(word, iniKeywordNear))
			conf.shiftNear = true;
		else if (!strcicmp(word, iniKeywordFar))
			conf.shiftNear = false;
	}
}

bool FileSys::saveConfigs(const umap<string, Com::Config>& confs, const char* file) {
#ifdef EMSCRIPTEN
	return true;
#else
	string text;
	for (const pair<const string, Com::Config>& it : confs) {
		text += makeIniLine(it.first);
		text += makeIniLine(iniKeywordBoardSize, toStr(it.second.homeSize));
		text += makeIniLine(iniKeywordSurvivalPass, toStr(it.second.survivalPass));
		text += makeIniLine(iniKeywordSurvivalMode, Com::Config::survivalNames[uint8(it.second.survivalMode)]);
		text += makeIniLine(iniKeywordFavorLimit, btos(it.second.favorLimit));
		text += makeIniLine(iniKeywordFavorMax, toStr(it.second.favorMax));
		text += makeIniLine(iniKeywordDragonDist, toStr(it.second.dragonDist));
		text += makeIniLine(iniKeywordDragonSingle, btos(it.second.dragonSingle));
		text += makeIniLine(iniKeywordDragonDiag, btos(it.second.dragonDiag));
		writeAmounts(text, iniKeywordTile, Com::tileNames, it.second.tileAmounts);
		writeAmounts(text, iniKeywordMiddle, Com::tileNames, it.second.middleAmounts);
		writeAmounts(text, iniKeywordPiece, Com::pieceNames, it.second.pieceAmounts);
		text += makeIniLine(iniKeywordWinFortress, toStr(it.second.winFortress));
		text += makeIniLine(iniKeywordWinThrone, toStr(it.second.winThrone));
		text += makeIniLine(iniKeywordCapturers, it.second.capturersString());
		text += makeIniLine(iniKeywordShift, string(it.second.shiftLeft ? iniKeywordLeft : iniKeywordRight) + ' ' + (it.second.shiftNear ? iniKeywordNear : iniKeywordFar));
		text += linend;
	}
	return writeFile(configPath(file), text);
#endif
}

template <sizet N, sizet S>
void FileSys::writeAmounts(string& text, const string& word, const array<string, N>& names, const array<uint16, S>& amts) {
	for (sizet i = 0; i < amts.size(); i++)
		text += makeIniLine(word + names[i], toStr(amts[i]));
}

umap<string, Setup> FileSys::loadSetups() {
	umap<string, Setup> sets;
#ifndef EMSCRIPTEN
	umap<string, Setup>::iterator sit;
	for (const string& line : readFileLines(configPath(fileSetups))) {
		if (string title = readIniTitle(line); !title.empty())
			sit = sets.emplace(std::move(title), Setup()).first;
		else if (pairStr it = readIniLine(line); !sets.empty()) {
			if (sizet len = strlen(iniKeywordTile); !strncicmp(it.first, iniKeywordTile, len))
				sit->second.tiles.emplace(stoiv<svec2>(it.first.c_str() + len, strtol), strToEnum<Com::Tile>(Com::tileNames, it.second));
			else if (len = strlen(iniKeywordMiddle); !strncicmp(it.first, iniKeywordMiddle, len))
				sit->second.mids.emplace(uint16(sstoul(it.first.substr(len))), strToEnum<Com::Tile>(Com::tileNames, it.second));
			else if (len = strlen(iniKeywordPiece); !strncicmp(it.first, iniKeywordPiece, len))
				sit->second.pieces.emplace(stoiv<svec2>(it.first.c_str() + len, strtol), strToEnum<Com::Piece>(Com::pieceNames, it.second));
		}
	}
#endif
	return sets;
}

bool FileSys::saveSetups(const umap<string, Setup>& sets) {
#ifdef EMSCRIPTEN
	return true;
#else
	string text;
	for (const pair<const string, Setup>& it : sets) {
		text += makeIniLine(it.first);
		for (const pair<svec2, Com::Tile>& ti : it.second.tiles)
			text += makeIniLine(iniKeywordTile + toStr(ti.first, "_"), Com::tileNames[uint8(ti.second)]);
		for (const pair<uint16, Com::Tile>& mi : it.second.mids)
			text += makeIniLine(iniKeywordMiddle + toStr(mi.first), Com::tileNames[uint8(mi.second)]);
		for (const pair<svec2, Com::Piece>& pi : it.second.pieces)
			text += makeIniLine(iniKeywordPiece + toStr(pi.first, "_"), Com::pieceNames[uint8(pi.second)]);
		text += linend;
	}
	return writeFile(configPath(fileSetups), text);
#endif
}

umap<string, Sound> FileSys::loadAudios(const SDL_AudioSpec& spec) {
	World::window()->writeLog("loading audio");
	SDL_RWops* ifh = SDL_RWFromFile(dataPath(fileAudios).c_str(), defaultReadMode);
	if (!ifh) {
		constexpr char errorAudios[] = "failed to load sounds";
		std::cerr << errorAudios << std::endl;
		World::window()->writeLog(errorAudios);
		return {};
	}
	uint16 size;
	SDL_RWread(ifh, &size, sizeof(size), 1);
	umap<string, Sound> auds(size);

	uint8 ibuf[audioHeaderSize];
	uint32* up = reinterpret_cast<uint32*>(ibuf + 2);
	uint16* sp = reinterpret_cast<uint16*>(ibuf + 6);
	string name;
	Sound sound;
	for (uint16 i = 0; i < size; i++) {
		SDL_RWread(ifh, ibuf, sizeof(*ibuf), audioHeaderSize);
		name.resize(ibuf[0]);
		sound.channels = ibuf[1];
		sound.length = up[0];
		sound.frequency = sp[0];
		sound.format = sp[1];
		sound.samples = sp[2];

		sound.data = static_cast<uint8*>(SDL_malloc(sound.length));
		SDL_RWread(ifh, name.data(), sizeof(*name.data()), name.length());
		SDL_RWread(ifh, sound.data, sizeof(*sound.data), sound.length);
		if (sound.convert(spec))
			auds.emplace(std::move(name), sound);
		else {
			sound.free();
			constexpr char errorFile[] = "failed to load ";
			std::cerr << errorFile << name << std::endl;
			World::window()->writeLog(errorFile + name);
		}
	}
	SDL_RWclose(ifh);
	return auds;
}

umap<string, Material> FileSys::loadMaterials() {
	World::window()->writeLog("loading materials");
	SDL_RWops* ifh = SDL_RWFromFile(dataPath(fileMaterials).c_str(), defaultReadMode);
	if (!ifh)
		throw std::runtime_error("failed to load materials");

	uint16 size;
	SDL_RWread(ifh, &size, sizeof(size), 1);
	umap<string, Material> mtls(size + 1);
	mtls.emplace();

	uint8 len;
	string name;
	Material matl;
	for (uint16 i = 0; i < size; i++) {
		SDL_RWread(ifh, &len, sizeof(len), 1);
		name.resize(len);
		SDL_RWread(ifh, name.data(), sizeof(*name.data()), name.length());
		SDL_RWread(ifh, &matl, sizeof(matl), 1);
		mtls.emplace(std::move(name), matl);
	}
	SDL_RWclose(ifh);
	return mtls;
}

umap<string, Mesh> FileSys::loadObjects() {
	World::window()->writeLog("loading objects");
	SDL_RWops* ifh = SDL_RWFromFile(dataPath(fileObjects).c_str(), defaultReadMode);
	if (!ifh)
		throw std::runtime_error("failed to load objects");

	glUseProgram(*World::geom());
	uint16 size;
	SDL_RWread(ifh, &size, sizeof(size), 1);
	umap<string, Mesh> mshs(size + 1);
	mshs.emplace();

	uint8 ibuf[objectHeaderSize];
	uint16* sp = reinterpret_cast<uint16*>(ibuf + 1);
	string name;
	vector<Vertex> verts;
	vector<uint16> elems;
	for (uint16 i = 0; i < size; i++) {
		SDL_RWread(ifh, ibuf, sizeof(*ibuf), objectHeaderSize);
		name.resize(ibuf[0]);
		elems.resize(sp[0]);
		verts.resize(sp[1]);

		SDL_RWread(ifh, name.data(), sizeof(*name.data()), name.length());
		SDL_RWread(ifh, elems.data(), sizeof(*elems.data()), elems.size());
		SDL_RWread(ifh, verts.data(), sizeof(*verts.data()), verts.size());
		mshs.emplace(std::move(name), Mesh(verts, elems));
	}
	SDL_RWclose(ifh);
	return mshs;
}

umap<string, string> FileSys::loadShaders() {
	SDL_RWops* ifh = SDL_RWFromFile(dataPath(fileShaders).c_str(), defaultReadMode);
	if (!ifh)
		throw std::runtime_error("failed to load shaders");

	uint8 size;
	SDL_RWread(ifh, &size, sizeof(size), 1);
	umap<string, string> shds(size);

	uint8 ibuf[shaderHeaderSize];
	uint16* sp = reinterpret_cast<uint16*>(ibuf + 1);
	string name, text;
	for (uint8 i = 0; i < size; i++) {
		SDL_RWread(ifh, ibuf, sizeof(*ibuf), shaderHeaderSize);
		name.resize(*ibuf);
		text.resize(*sp);

		SDL_RWread(ifh, name.data(), sizeof(*name.data()), name.length());
		SDL_RWread(ifh, text.data(), sizeof(*text.data()), text.length());
		shds.emplace(std::move(name), std::move(text));
	}
	SDL_RWclose(ifh);
	return shds;
}

umap<string, Texture> FileSys::loadTextures() {
	World::window()->writeLog("loading textures");
	umap<string, Texture> texs = { pair(string(), Texture({ 255, 255, 255 })) };
	loadTextures(texs, [](umap<string, Texture>& texs, string&& name, SDL_Surface* img, GLint iform, GLenum pform) { texs.emplace(std::move(name), Texture(img, iform, pform)); });
	return texs;
}

void FileSys::loadTextures(umap<string, Texture>& texs, void (*inset)(umap<string, Texture>&, string&&, SDL_Surface*, GLint, GLenum)) {
	SDL_RWops* ifh = SDL_RWFromFile(dataPath(fileTextures).c_str(), defaultReadMode);
	if (!ifh)
		throw std::runtime_error("failed to load textures");

	int div = 100 / int(World::sets()->texScale);
	uint16 size;
	SDL_RWread(ifh, &size, sizeof(size), 1);

	uint8 ibuf[textureHeaderSize];
	uint32* wp = reinterpret_cast<uint32*>(ibuf + 1);
	uint16* sp = reinterpret_cast<uint16*>(ibuf + 5);
	vector<uint8> imgd;
	string name;
	for (uint16 i = 0; i < size; i++) {
		SDL_RWread(ifh, ibuf, sizeof(*ibuf), textureHeaderSize);
		name.resize(ibuf[0]);
		imgd.resize(wp[0]);

		SDL_RWread(ifh, name.data(), sizeof(*name.data()), name.length());
		SDL_RWread(ifh, imgd.data(), sizeof(*imgd.data()), imgd.size());
		if (SDL_Surface* img = scaleSurface(IMG_Load_RW(SDL_RWFromMem(imgd.data(), int(imgd.size())), SDL_TRUE), div))
			inset(texs, std::move(name), img, sp[0], sp[1]);
		else
			std::cerr << "failed to load " << name << ": " << SDL_GetError() << std::endl;
	}
	SDL_RWclose(ifh);
}
