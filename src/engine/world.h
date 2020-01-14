#pragma once

#include "windowSys.h"

// class that makes accessing stuff easier
class World {
public:
	static constexpr char argExternal = 'e';
	static constexpr char argSetup = 'd';
#ifdef EMSCRIPTEN
	static constexpr char envLocale[] = "C";
#else
	static constexpr char envLocale[] = "";
#endif

	static Arguments args;
private:
	static WindowSys windowSys;		// the thing ontop of which everything runs

public:
	static WindowSys* window();
	static AudioSys* audio();
	static FontSet* fonts();
	static Game* game();
	static Netcp* netcp();
	static Program* program();
	static ProgState* state();
	template <class T> static T* state();
	static Settings* sets();
	static Scene* scene();
	static const ShaderGeometry* geom();
	static const ShaderDepth* depth();
	static const ShaderGui* gui();

#ifdef _WIN32
	static void setArgs(PWSTR pCmdLine);
#endif
	static void setArgs(int argc, char** argv);

	template <class F, class... A> static void prun(F func, A... args);
	static void play(const string& name);
};

inline WindowSys* World::window() {
	return &windowSys;
}

inline AudioSys* World::audio() {
	return windowSys.getAudio();
}

inline FontSet* World::fonts() {
	return windowSys.getFonts();
}

inline Game* World::game() {
	return windowSys.getProgram()->getGame();
}

inline Netcp* World::netcp() {
	return windowSys.getProgram()->getNetcp();
}

inline Program* World::program() {
	return windowSys.getProgram();
}

inline ProgState* World::state() {
	return windowSys.getProgram()->getState();
}

template <class T>
T* World::state() {
	return static_cast<T*>(windowSys.getProgram()->getState());
}

inline Scene* World::scene() {
	return windowSys.getScene();
}

inline Settings* World::sets() {
	return windowSys.getSets();
}

inline const ShaderGeometry* World::geom() {
	return windowSys.getGeom();
}

inline const ShaderDepth* World::depth() {
	return windowSys.getDepth();
}

inline const ShaderGui* World::gui() {
	return windowSys.getGUI();
}
#ifdef _WIN32
inline void World::setArgs(PWSTR pCmdLine) {
	args.setArgs(pCmdLine, { argSetup }, {});
}
#endif
inline void World::setArgs(int argc, char** argv) {
	args.setArgs(argc, argv, stos, { argSetup }, {});
}

template <class F, class... A>
void World::prun(F func, A... args) {
	if (func)
		(program()->*func)(args...);
}

inline void World::play(const string& name) {
	if (windowSys.getAudio())
		windowSys.getAudio()->play(name);
}
