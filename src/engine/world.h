#pragma once

#include "windowSys.h"

// class that makes accessing stuff easier
class World {
private:
	static WindowSys windowSys;		// the thing on which everything runs
	static vector<string> vals;		// TODO: check if these could be saved as char[]
	static uset<string> flags;
	static umap<string, string> opts;

public:
	static FileSys* fileSys();
	static WindowSys* winSys();
	static Scene* scene();
	static Program* program();
	static ProgState* state();
	static Game* game();
	static Settings* sets();

	static const vector<string>& getVals();
	static bool hasFlag(const string& flg);
	static const string& getOpt(const string& key);
#ifdef _WIN32
	static void setArgs(PWSTR pCmdLine);
#else
	static void setArgs(int argc, char** argv);
#endif
	template <class F, class... A> static void prun(F func, A... args);
private:
	template <class C, class F, class... A> static void run(C* obj, F func, A... args);
};

inline FileSys* World::fileSys() {
	return windowSys.getFileSys();
}

inline WindowSys* World::winSys() {
	return &windowSys;
}

inline Scene* World::scene() {
	return windowSys.getScene();
}

inline Program* World::program() {
	return windowSys.getProgram();
}

inline ProgState* World::state() {
	return windowSys.getProgram()->getState();
}

inline Game* World::game() {
	return windowSys.getProgram()->getGame();
}

inline Settings* World::sets() {
	return windowSys.getSets();
}

inline const vector<string>& World::getVals() {
	return vals;
}

inline bool World::hasFlag(const string& flg) {
	return flags.count(flg);
}

template <class F, class... A>
void World::prun(F func, A... args) {
	run(program(), func, args...);
}

template <class C, class F, class... A>
void World::run(C* obj, F func, A... args) {
	if (func)
		(obj->*func)(args...);
}
