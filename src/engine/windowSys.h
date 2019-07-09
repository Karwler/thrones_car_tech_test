#pragma once

#include "audioSys.h"
#include "fileSys.h"
#include "scene.h"
#include "prog/program.h"
#include "utils/layouts.h"
#ifdef __APPLE__
#include <SDL2_ttf/SDL_ttf.h>
#else
#include <SDL2/SDL_ttf.h>
#endif

// loads different font sizes from one font and handles basic log display
class FontSet {
private:
	static constexpr char fileFont[] = "data/romanesque.ttf";
	static constexpr char fontTestString[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ`~!@#$%^&*()_+-=[]{}'\\\"|;:,.<>/?";
	static constexpr int fontTestHeight = 100;
	static constexpr int logSize = 18;
	static constexpr float fallbackScale = 0.9f;
	static constexpr SDL_Color textColor = { 255, 193, 37, 255 };
	static constexpr SDL_Color logColor = { 230, 220, 220, 220 };

	float heightScale;	// for scaling down font size to fit requested height
	umap<int, TTF_Font*> fonts;
	vector<uint8> fontData;

	TTF_Font* logFont;
	Texture logTex;
	vector<string> logLines;

public:
	FontSet();
	~FontSet();

	void clear();
	int length(const string& text, int height);
	Texture render(const string& text, int height);
	Texture render(const string& text, int height, uint length);

	void writeLog(string&& text, vec2i res);
	void closeLog();	// doesn't get called in destructor

private:
	TTF_Font* getFont(int height);
};

inline FontSet::~FontSet() {
	clear();
}

inline Texture FontSet::render(const string& text, int height) {
	return TTF_RenderUTF8_Blended(getFont(height), text.c_str(), textColor);
}

inline Texture FontSet::render(const string& text, int height, uint length) {
	return TTF_RenderUTF8_Blended_Wrapped(getFont(height), text.c_str(), textColor, length);
}

// for drawing
class Shader {
public:
	GLuint program;

public:
	Shader(const string& srcVert, const string& srcFrag);
	~Shader();

private:
	static GLuint loadShader(const string& source, GLenum type);
};

inline Shader::~Shader() {
	glDeleteProgram(program);
}

class ShaderScene : public Shader {
public:
	GLint pview, trans, rotscl, vertex, uvloc, normal, texsamp, viewPos;
	GLint materialDiffuse, materialEmission, materialSpecular, materialShininess, materialAlpha;
	GLint lightPos, lightAmbient, lightDiffuse, lightSpecular, lightLinear, lightQuadratic;

public:
	ShaderScene(const string& srcVert, const string& srcFrag);
};

class ShaderGUI : public Shader {
public:
	GLint pview, rect, uvrc, zloc, vertex, uvloc;
	GLint color, texsamp;
private:
	Rectangle wrect;

public:
	ShaderGUI(const string& srcVert, const string& srcFrag);
	~ShaderGUI();

	void bindRect() const;
};

inline ShaderGUI::~ShaderGUI() {
	wrect.free(this);
}

inline void ShaderGUI::bindRect() const {
	glBindVertexArray(wrect.vao);
}

// handles window events and contains video settings
class WindowSys {
public:
	static constexpr char title[] = "Thrones";
private:
	static constexpr char fileIcon[] = "data/thrones.bmp";
	static constexpr char fileCursor[] = "data/cursor.bmp";
	static constexpr char fileSceneVert[] = "scene.vert";
	static constexpr char fileSceneFrag[] = "scene.frag";
	static constexpr char fileGuiVert[] = "gui.vert";
	static constexpr char fileGuiFrag[] = "gui.frag";

	static constexpr vec2i defaultWindowPos = { SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED };
	static constexpr uint32 eventCheckTimeout = 50;
	static constexpr float ticksPerSec = 1000.f;
	static constexpr GLclampf colorClear[4] = { 0.f, 0.f, 0.f, 1.f };
	static constexpr GLbitfield clearSet = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
	static constexpr uint8 fallbackCursorSize = 18;
	
	uptr<AudioSys> audio;
	uptr<Program> program;
	uptr<Scene> scene;
	uptr<Settings> sets;
	SDL_Window* window;
	SDL_GLContext context;
	uptr<ShaderScene> space;
	uptr<ShaderGUI> gui;
	uptr<FontSet> fonts;
	vec2i curView;
	float dSec;			// delta seconds, aka the time between each iteration of the above mentioned loop
	bool run;			// whether the loop in which the program runs should continue
	SDL_Cursor* cursor;
	uint8 cursorHeight;

public:
	WindowSys();

	int start();
	void close();

	float getDSec() const;
	vec2i getView() const;
	uint8 getCursorHeight() const;
	vector<vec2i> displaySizes() const;
	vector<SDL_DisplayMode> displayModes() const;
	int displayID() const;
	void writeLog(string&& text);
	void setScreen(uint8 display, Settings::Screen screen, vec2i size, const SDL_DisplayMode& mode);
	void setVsync(Settings::VSync vsync);
	void setGamma(float gamma);
	void resetSettings();

	AudioSys* getAudio();
	FontSet* getFonts();
	Program* getProgram();
	Scene* getScene();
	Settings* getSets();
	ShaderScene* getSpace();
	ShaderGUI* getGUI();

private:
	void init();
	void exec();
	void cleanup();

	void createWindow();
	void destroyWindow();
	void handleEvent(const SDL_Event& event);	// pass events to their specific handlers
	void eventWindow(const SDL_WindowEvent& winEvent);
	void setSwapInterval();
	bool trySetSwapInterval();
	void setWindowMode();

	void updateViewport();
	bool checkCurDisplay();
	template <class T> static bool checkResolution(T& val, const vector<T>& modes);
};

inline void WindowSys::close() {
	run = false;
}

inline float WindowSys::getDSec() const {
	return dSec;
}

inline vec2i WindowSys::getView() const {
	return curView;
}

inline uint8 WindowSys::getCursorHeight() const {
	return cursorHeight;
}

inline AudioSys* WindowSys::getAudio() {
	return audio.get();
}

inline FontSet* WindowSys::getFonts() {
	return fonts.get();
}

inline Program* WindowSys::getProgram() {
	return program.get();
}

inline Scene* WindowSys::getScene() {
	return scene.get();
}

inline Settings* WindowSys::getSets() {
	return sets.get();
}

inline ShaderScene* WindowSys::getSpace() {
	return space.get();
}

inline ShaderGUI* WindowSys::getGUI() {
	return gui.get();
}

inline void WindowSys::updateViewport() {
	SDL_GL_GetDrawableSize(window, &curView.x, &curView.y);
	glViewport(0, 0, curView.x, curView.y);
}

inline int WindowSys::displayID() const {
	return SDL_GetWindowDisplayIndex(window);
}

template <class T>
bool WindowSys::checkResolution(T& val, const vector<T>& modes) {
	typename vector<T>::const_iterator it;
	if (it = std::find(modes.begin(), modes.end(), val); it != modes.end() || modes.empty())
		return true;

	for (it = modes.begin(); it != modes.end() && *it < val; it++);
	val = it == modes.begin() ? *it : *(it - 1);
	return false;
}
