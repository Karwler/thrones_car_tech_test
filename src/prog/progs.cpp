#include "engine/world.h"

// PROGRAM TEXT

ProgState::Text::Text(string str, int height) :
	text(std::move(str)),
	length(strLen(text, height)),
	height(height)
{}

int ProgState::Text::strLen(const string& str, int height) {
	return World::fonts()->length(str, height) + Label::textMargin * 2;
}

int ProgState::findMaxLength(const vector<vector<string>*>& lists, int height) {
	int width = 0;
	for (vector<string>* it : lists)
		if (int len = findMaxLength(it->begin(), it->end(), height); len > width)
			width = len;
	return width;
}

Texture ProgState::makeTooltip(const string& text, int limit, int height) {
	return World::fonts()->render(text, height, uint(limit));
}

Texture ProgState::makeTooltip(const vector<string>& lines, int limit, int height) {
	if (lines.empty())
		return Texture();

	int width = 0;
	for (const string& str : lines)
		if (int len = Text::strLen(str, height); len > width) {
			if (len < limit)
				width = len;
			else {
				width = limit;
				break;
			}
		}
	string text = lines[0];
	for (sizet i = 1; i < lines.size(); i++)
		text += linend + lines[i];
	return World::fonts()->render(text, height, uint(width));
}

// PROGRAM STATE

void ProgState::eventEnter() {
	if (World::scene()->getPopup())
		World::program()->eventClosePopup();
}

bool ProgState::tryClosePopup() {
	if (World::scene()->getPopup()) {
		World::prun(World::scene()->getPopup()->ccall, nullptr);
		return true;
	}
	return false;
}

Layout* ProgState::createLayout() {
	return nullptr;
}

Popup* ProgState::createPopupMessage(string msg, BCall ccal, string ctxt) {
	Text ok(std::move(ctxt), superHeight);
	Text ms(std::move(msg), superHeight);
	vector<Widget*> bot = {
		new Widget(),
		new Label(ok.length, ok.text, ccal, nullptr, Texture(), Label::Alignment::center),
		new Widget()
	};
	vector<Widget*> con = {
		new Label(1.f, ms.text, nullptr, nullptr, Texture(), Label::Alignment::center),
		new Layout(1.f, std::move(bot), false, 0)
	};
	return new Popup(cvec2<Size>(ms.length, superHeight * 2 + Layout::defaultItemSpacing), std::move(con), ccal, ccal);
}

Popup* ProgState::createPopupChoice(string msg, BCall kcal, BCall ccal) {
	Text ms(std::move(msg), superHeight);
	vector<Widget*> bot = {
		new Label(1.f, "Yes", kcal, nullptr, Texture(), Label::Alignment::center),
		new Label(1.f, "No", ccal, nullptr, Texture(), Label::Alignment::center)
	};
	vector<Widget*> con = {
		new Label(1.f, ms.text, nullptr, nullptr, Texture(), Label::Alignment::center),
		new Layout(1.f, std::move(bot), false, 0)
	};
	return new Popup(cvec2<Size>(ms.length, superHeight * 2 + Layout::defaultItemSpacing), std::move(con), kcal, ccal);
}

pair<Popup*, Widget*> ProgState::createPopupInput(string msg, BCall kcal) {
	LabelEdit* ledit = new LabelEdit(1.f, "", kcal, &Program::eventClosePopup, kcal);
	vector<Widget*> bot = {
		new Label(1.f, "Ok", kcal, nullptr, Texture(), Label::Alignment::center),
		new Label(1.f, "Cancel", &Program::eventClosePopup, nullptr, Texture(), Label::Alignment::center)
	};
	vector<Widget*> con = {
		new Label(1.f, std::move(msg)),
		ledit,
		new Layout(1.f, std::move(bot), false, 0)
	};
	return pair(new Popup(cvec2<Size>(500, superHeight * 3 + Layout::defaultItemSpacing * 2), std::move(con), kcal, &Program::eventClosePopup), ledit);
}

// PROG MENU

void ProgMenu::eventEscape() {
	if (!tryClosePopup())
		World::window()->close();
}

Layout* ProgMenu::createLayout() {
	// server input
	Text srvt("Server:", superHeight);
	vector<Widget*> srv = {
		new Label(srvt.length, srvt.text),
		new LabelEdit(1.f, World::sets()->address, &Program::eventUpdateAddress, nullptr, &Program::eventConnectServer)
	};

	// port input and connect button
	vector<Widget*> prt = {
		new Label(srvt.length, "Port:"),
		new LabelEdit(1.f, toStr(World::sets()->port), &Program::eventUpdatePort, nullptr, &Program::eventConnectServer, Texture())
	};

	// net buttons
	vector<Widget*> con = {
		new Label(srvt.length, "Host", &Program::eventOpenHostMenu, nullptr, Texture(), Label::Alignment::center),
		new Label(1.f, "Connect", &Program::eventConnectServer, nullptr, Texture(), Label::Alignment::center)
	};

	// title
	int width = World::fonts()->length(srvt.text + "000.000.000.000", superHeight) + Layout::defaultItemSpacing + Label::textMargin * 4;
	const Texture* title = World::scene()->getTex("title");
	vector<Widget*> tit = {
		new Widget(),
		new Button(title->getRes().y * width / title->getRes().x, nullptr, nullptr, Texture(), title->getID(), vec4(1.f))
	};

	// middle buttons
	vector<Widget*> buts = {
		new Layout(1.f, std::move(tit), true, superSpacing),
		new Widget(0),
		new Layout(superHeight, std::move(srv), false),
		new Layout(superHeight, std::move(prt), false),
		new Layout(superHeight, std::move(con), false),
		new Widget(0),
		new Label(superHeight, "Settings", &Program::eventOpenSettings, nullptr, Texture(), Label::Alignment::center),
		new Label(superHeight, "Exit", &Program::eventExit, nullptr, Texture(), Label::Alignment::center),
		new Widget(0.9f)
	};

	// root layout
	vector<Widget*> cont = {
		new Widget(),
		new Layout(width, std::move(buts), true, superSpacing),
		new Widget()
	};
	return new Layout(1.f, cont, false, 0);
}

// PROG HOST

ProgHost::ProgHost() :
	confs(Com::loadConfs(Com::defaultConfigFile))
{
	if (curConf = sizet(std::find_if(confs.begin(), confs.end(), [](const Com::Config& cf) -> bool { return cf.name == Com::Config::defaultName; }) - confs.begin()); curConf >= confs.size())
		curConf = 0;
	confs[curConf].checkValues();
}

void ProgHost::eventEscape() {
	if (!tryClosePopup())
		World::program()->eventExitGame();
}

Layout* ProgHost::createLayout() {
	vector<string> cfgNames(confs.size());
	for (sizet i = 0; i < confs.size(); i++)
		cfgNames[i] = confs[i].name;

	Text back("Back");
	Text port("Port:");
	vector<Widget*> top0 = {
		new Label(back.length, back.text, &Program::eventExitHost),
		new Label(1.f, "Open", &Program::eventHostServer, nullptr, Texture(), Label::Alignment::center),
		new Label(port.length, port.text),
		new LabelEdit(World::fonts()->length("00000", lineHeight), toStr(World::sets()->port), &Program::eventUpdatePort)
	};
	Text cfgt("Configuration: ");
	Text aleft(arrowLeft);
	Text aright(arrowRight);
	Text copy("Copy");
	Text newc("New");
	vector<Widget*> top1 = {
		new Label(cfgt.length, cfgt.text),
		new Label(aleft.length, aleft.text, &Program::eventSBPrev),
		new SwitchBox(1.f, cfgNames.data(), uint(cfgNames.size()), cfgNames[curConf], &Program::eventSwitchConfig, Texture(), Label::Alignment::center),
		new Label(aright.length, aright.text, &Program::eventSBNext),
		new Label(copy.length, copy.text, &Program::eventConfigCopyInput),
		new Label(newc.length, newc.text, &Program::eventConfigNewInput)
	};
	if (confs.size() > 1) {
		Text dele("Del");
		top1.insert(top1.begin() + 4, new Label(dele.length, dele.text, &Program::eventConfigDeleteInput));
	}

	Text reset("Reset");
	vector<string> txs = {
		"Board width",
		"Homeland height",
		"Survival pass",
		"Fate's Favors limit",
		"Dragon move limit",
		"Dragon step diagonal",
		"Multistage turns",
		"Fortresses captured",
		"Thrones killed",
		"Capturers",
		"Shift left",
		"Shift near"
	};
	vector<string> tips = {
		"Amount of tiles per row",
		"Amount of tiles per column minus one divided by two",
		"Chance of passing the survival check",
		"Maximum amount of fate's favors per player",
		"Maximum movement steps of a dragon",
		"Whether a dragon can make diagonal steps",
		"Whether a firing piece can move and fire in one turn",
		"Amount of enemy fortresses that need to be captured in order to win",
		"Amount of enemy thrones that need to be killed in order to win",
		"Pieces that are able to capture fortresses",
		"Whether to shift middle tiles to the left of the first player",
		"Whether to shift middle tiles to the closest free space",
		"Reset current config"
	};
	std::reverse(txs.begin(), txs.end());
	std::reverse(tips.begin(), tips.end());
	int descLength = findMaxLength(txs.begin(), txs.end());

	vector<vector<Widget*>> lines0 = { {
		new Label(descLength, popBack(txs)),
		inWidth = new LabelEdit(1.f, toStr(confs[curConf].homeWidth), &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		inHeight = new LabelEdit(1.f, toStr(confs[curConf].homeHeight), &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		inSurvivalSL = new Slider(1.f, confs[curConf].survivalPass, 0, 100, &Program::eventUpdateSurvivalSL, nullptr, makeTooltip(tips.back())),
		inSurvivalLE = new LabelEdit(Text::strLen("100%"), toStr(confs[curConf].survivalPass) + '%', &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		inFavors = new LabelEdit(1.f, toStr(confs[curConf].favorLimit), &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		inDragonDist = new LabelEdit(1.f, toStr(confs[curConf].dragonDist), &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		inDragonDiag = new CheckBox(lineHeight, confs[curConf].dragonDiag, &Program::eventUpdateConfig, &Program::eventUpdateConfig, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		inMultistage = new CheckBox(lineHeight, confs[curConf].multistage, &Program::eventUpdateConfig, &Program::eventUpdateConfig, makeTooltip(popBack(tips)))
	} };

	vector<vector<Widget*>> lines1(Com::tileMax);
	for (uint8 i = 0; i < Com::tileMax - 1; i++)
		lines1[i] = {
			new Label(descLength, firstUpper(Com::tileNames[i])),
			inTiles[i] = new LabelEdit(1.f, toStr(confs[curConf].tileAmounts[i]), &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip("Number of " + Com::tileNames[i] + " tiles per homeland"))
		};
	lines1.back() = {
		new Label(descLength, firstUpper(Com::tileNames[uint8(Com::Tile::fortress)])),
		outTileFortress = new Label(1.f, tileFortressString(confs[curConf]), nullptr, nullptr, makeTooltip({ "Number of " + Com::tileNames[uint8(Com::Tile::fortress)] + " tiles per homeland", "(calculated automatically to fill remaining free tiles)" }))
	};

	vector<vector<Widget*>> lines2(Com::tileMax);
	for (uint8 i = 0; i < Com::tileMax - 1; i++)
		lines2[i] = {
			new Label(descLength, firstUpper(Com::tileNames[i])),
			inMiddles[i] = new LabelEdit(1.f, toStr(confs[curConf].middleAmounts[i]), &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip("Number of " + Com::tileNames[i] + " tiles in middle row per player"))
	};
	lines2.back() = {
		new Label(descLength, firstUpper(Com::tileNames[uint8(Com::Tile::fortress)])),
		outMiddleFortress = new Label(1.f, middleFortressString(confs[curConf]), nullptr, nullptr, makeTooltip({ "Number of " + Com::tileNames[uint8(Com::Tile::fortress)] + " tiles in middle row", "(calculated automatically to fill remaining free tiles)" }))
	};

	vector<vector<Widget*>> lines3(Com::pieceMax + 1);
	for (uint8 i = 0; i < Com::pieceMax; i++)
		lines3[i] = {
			new Label(descLength, firstUpper(Com::pieceNames[i])),
			inPieces[i] = new LabelEdit(1.f, toStr(confs[curConf].pieceAmounts[i]), &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip("Number of " + Com::pieceNames[i] + " pieces per player"))
		};
	lines3.back() = {
		new Label(descLength, "Total"),
		outPieceTotal = new Label(1.f, pieceTotalString(confs[curConf]), nullptr, nullptr, makeTooltip("Total amount of pieces out of the maximum possible number of pieces per player"))
	};

	vector<vector<Widget*>> lines4 = { {
		new Label(descLength, popBack(txs)),
		inWinFortress = new LabelEdit(1.f, toStr(confs[curConf].winFortress), &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		inWinThrone = new LabelEdit(1.f, toStr(confs[curConf].winThrone), &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		inCapturers = new LabelEdit(1.f, confs[curConf].capturersString(), &Program::eventUpdateConfig, nullptr, &Program::eventUpdateConfig, makeTooltip(popBack(tips)))
	} };

	vector<vector<Widget*>> lines5 = { {
		new Label(descLength, popBack(txs)),
		inShiftLeft = new CheckBox(lineHeight, confs[curConf].shiftLeft, &Program::eventUpdateConfig, nullptr, makeTooltip(popBack(tips))),
		new Widget()
	}, {
		new Label(descLength, popBack(txs)),
		inShiftNear = new CheckBox(lineHeight, confs[curConf].shiftNear, &Program::eventUpdateConfig, nullptr, makeTooltip(popBack(tips))),
		new Widget()
	} };

	vector<vector<Widget*>> lineR = { {
		new Label(reset.length, reset.text, &Program::eventUpdateReset, nullptr, makeTooltip(popBack(tips))),
		new Widget()
	} };

	sizet id = 0;
	vector<Widget*> menu(lines0.size() + lines1.size() + lines2.size() + lines3.size() + lines4.size() + lines5.size() + lineR.size() + 5);
	setLines(menu, lines0, id);
	setTitle(menu, "Tile amounts", id);
	setLines(menu, lines1, id);
	setTitle(menu, "Middle amounts", id);
	setLines(menu, lines2, id);
	setTitle(menu, "Piece amounts", id);
	setLines(menu, lines3, id);
	setTitle(menu, "Winning conditions", id);
	setLines(menu, lines4, id);
	setTitle(menu, "Middle row rearranging", id);
	setLines(menu, lines5, id);
	setLines(menu, lineR, id);

	vector<Widget*> topb = {
		new Layout(lineHeight, std::move(top0), false),
		new Layout(lineHeight, std::move(top1), false)
	};
	vector<Widget*> cont = {
		new Layout(lineHeight * 2 + Layout::defaultItemSpacing, std::move(topb)),
		new ScrollArea(1.f, std::move(menu))
	};
	return new Layout(1.f, std::move(cont), true, superSpacing);
}

void ProgHost::setLines(vector<Widget*>& menu, vector<vector<Widget*>>& lines, sizet& id) {
	for (vector<Widget*>& it : lines)
		menu[id++] = new Layout(lineHeight, std::move(it), false);
}

void ProgHost::setTitle(vector<Widget*>& menu, string&& title, sizet& id) {
	int tlen = Text::strLen(title);
	vector<Widget*> line = {
		new Label(tlen, std::move(title)),
		new Widget()
	};
	menu[id++] = new Layout(lineHeight, std::move(line), false);
}

// PROG SETUP

ProgSetup::ProgSetup() :
	enemyReady(false),
	selected(0),
	lastHold(INT16_MIN),
	lastButton(0)
{}

void ProgSetup::eventEscape() {
	if (!tryClosePopup())
		stage > Stage::tiles && stage < Stage::ready ? World::program()->eventSetupBack() : World::scene()->setPopup(createPopupChoice("Exit game?", &Program::eventExitGame, &Program::eventClosePopup));
}

void ProgSetup::eventWheel(int ymov) {
	setSelected(cycle(selected, uint8(counters.size()), int8(-ymov)));
}

void ProgSetup::eventDrag(uint32 mStat) {
	uint8 curButton = mStat & SDL_BUTTON_LMASK ? SDL_BUTTON_LEFT : mStat & SDL_BUTTON_RMASK ? SDL_BUTTON_RIGHT : 0;
	BoardObject* bo = dynamic_cast<BoardObject*>(World::scene()->select);
	vec2s curHold = bo ? World::game()->ptog(bo->pos) : INT16_MIN;
	if (bo && curButton && (curHold != lastHold || curButton != lastButton)) {
		if (stage <= Stage::middles)
			curButton == SDL_BUTTON_LEFT ? World::program()->eventPlaceTileH() : World::program()->eventClearTile();
		else if (stage == Stage::pieces)
			curButton == SDL_BUTTON_LEFT ? World::program()->eventPlacePieceH() : World::program()->eventClearPiece();
	}
	lastHold = curHold;
	lastButton = curButton;
}

void ProgSetup::eventUndrag() {
	lastHold = INT16_MIN;
}

bool ProgSetup::setStage(ProgSetup::Stage stg) {
	switch (stage = stg) {
	case Stage::tiles:
		World::game()->setOwnTilesInteract(Tile::Interactivity::interact);
		World::game()->setMidTilesInteract(Tile::Interactivity::ignore, true);
		World::game()->setOwnPiecesVisible(false);
		counters = World::game()->countOwnTiles();
		break;
	case Stage::middles:
		World::game()->setOwnTilesInteract(Tile::Interactivity::ignore, true);
		World::game()->setMidTilesInteract(Tile::Interactivity::interact);
		World::game()->setOwnPiecesVisible(false);
		counters = World::game()->countMidTiles();
		break;
	case Stage::pieces:
		World::game()->setOwnTilesInteract(Tile::Interactivity::recognize);
		World::game()->setMidTilesInteract(Tile::Interactivity::ignore, true);
		World::game()->setOwnPiecesVisible(true);
		counters = World::game()->countOwnPieces();
		break;
	case Stage::ready:
		World::game()->setOwnTilesInteract(Tile::Interactivity::ignore);
		World::game()->setMidTilesInteract(Tile::Interactivity::ignore);
		World::game()->setOwnPiecesInteract(false);
		counters.clear();
		return true;
	}
	World::scene()->resetLayouts();
	setSelected(0);
	for (uint8 i = 0; i < counters.size(); i++)
		switchIcon(i, counters[i], stage != Stage::pieces);
	return false;
}

void ProgSetup::setSelected(uint8 sel) {
	static_cast<Draglet*>(icons->getWidget(selected + 1))->setSelected(false);
	selected = sel;
	static_cast<Draglet*>(icons->getWidget(selected + 1))->setSelected(true);
}

uint8 ProgSetup::findNextSelect(bool fwd) {
	uint8 m = btom<uint8>(fwd);
	for (uint8 i = selected + m; i < counters.size(); i += m)
		if (counters[i])
			return i;
	for (uint8 i = fwd ? 0 : uint8(counters.size() - 1); i != selected; i += m)
		if (counters[i])
			return i;
	return selected;
}

Layout* ProgSetup::createLayout() {
	// center piece
	vector<Widget*> midl = {
		new Widget(1.f),
		message = new Label(superHeight, "", nullptr, nullptr, Texture(), Label::Alignment::center, false),
		new Widget(6.f),
		icons = stage == Stage::pieces ? getPicons() : getTicons()
	};

	// root layout
	int sideLength;
	Layout* side = createSidebar(sideLength);
	vector<Widget*> cont = {
		side,
		new Layout(1.f, std::move(midl), true, 0),
		new Widget(sideLength)
	};
	return new Layout(1.f, std::move(cont), false, 0);
}

Layout* ProgSetup::getTicons() {
	vector<Widget*> tbot = { new Widget() };
	for (uint8 i = 0; i < 4; i++)
		tbot.push_back(new Draglet(iconSize, &Program::eventPlaceTileD, &Program::eventIconSelect, nullptr, World::scene()->blank(), vec4(World::scene()->material(Com::tileNames[i])->diffuse, 1.f)));
	tbot.push_back(new Widget());
	return new Layout(iconSize, std::move(tbot), false);
}

Layout* ProgSetup::getPicons() {
	vector<Widget*> pbot = { new Widget() };
	for (const string& it : Com::pieceNames)
		pbot.push_back(new Draglet(iconSize, &Program::eventPlacePieceD, &Program::eventIconSelect, nullptr, World::scene()->texture(it), vec4(1.f)));
	pbot.push_back(new Widget());
	return new Layout(iconSize, std::move(pbot), false);
}

Layout* ProgSetup::createSidebar(int& sideLength) const {
	vector<string> sidt = stage == Stage::tiles ? vector<string>({ "Exit", "Next" }) : vector<string>({ "Exit", "Back", "Next" });
	sideLength = findMaxLength(sidt.begin(), sidt.end());
	return new Layout(sideLength, stage == Stage::tiles ? vector<Widget*>({ new Label(lineHeight, popBack(sidt), &Program::eventSetupNext), new Label(lineHeight, popBack(sidt), &Program::eventExitGame) }) : vector<Widget*>({ new Label(lineHeight, popBack(sidt), &Program::eventSetupNext), new Label(lineHeight, popBack(sidt), &Program::eventSetupBack), new Label(lineHeight, popBack(sidt), &Program::eventExitGame) }));
}

void ProgSetup::incdecIcon(uint8 type, bool inc, bool isTile) {
	if (!inc && counters[type] == 1) {
		switchIcon(type, false, isTile);
		setSelected(findNextSelect(true));
	} else if (inc && !counters[type])
		switchIcon(type, true, isTile);
	counters[type] += btom<uint16>(inc);
}

void ProgSetup::switchIcon(uint8 type, bool on, bool isTile) {
	Draglet* ico = getIcon(type);
	ico->setDim(on ? 1.f : 0.5f);
	ico->setLcall(on ? isTile ? &Program::eventPlaceTileD : &Program::eventPlacePieceD : nullptr);
}

// PROG MATCH

void ProgMatch::eventEscape() {
	if (!tryClosePopup())
		World::program()->eventExitGame();
}

void ProgMatch::updateFavorIcon(bool on, uint8 cnt, uint8 tot) {
	favorIcon->setDim(on && cnt ? 1.f : 0.5f);
	favorIcon->setText("FF: " + toStr(cnt) + '/' + toStr(tot));
}

void ProgMatch::updateTurnIcon(bool on) {
	turnIcon->setLcall(on ? &Program::eventEndTurn : nullptr);
	turnIcon->setDim(on ? 1.f : 0.5f);
}

void ProgMatch::setDragonIcon(bool on) {
	if (dragonIcon) {
		if (Draglet* ico = static_cast<Draglet*>(dragonIcon->getWidget(0)); on) {
			ico->setLcall(&Program::eventPlaceDragon);
			ico->setDim(1.f);
		} else {
			ico->setLcall(nullptr);
			ico->setDim(0.5f);
		}
	}
}

void ProgMatch::deleteDragonIcon() {
	dragonIcon->getParent()->deleteWidget(dragonIcon->getID());
	dragonIcon = nullptr;
}

Layout* ProgMatch::createLayout() {
	// sidebar
	int sideLength = Text::strLen("FF: 00/00");
	vector<Widget*> left = {
		new Label(lineHeight, "Exit", &Program::eventExitGame),
		new Widget(0),
		favorIcon = new Label(lineHeight, "FF: 0/0"),	// text is updated after the icon has a parent
		turnIcon = new Label(lineHeight, "End turn")
	};
	updateTurnIcon(World::game()->getMyTurn());
	
	if (World::game()->ptog(World::game()->getOwnPieces(Com::Piece::dragon)->pos).hasNot(INT16_MIN))
		dragonIcon = nullptr;
	else {
		left.push_back(dragonIcon = new Layout(iconSize, { new Draglet(iconSize, nullptr, nullptr, nullptr, World::scene()->texture(Com::pieceNames[uint8(Com::Piece::dragon)])) }, false, 0));
		setDragonIcon(World::game()->getMyTurn());
	}

	// middle and root layout
	vector<Widget*> midl = {
		new Layout(sideLength, std::move(left)),
		new Widget(1.f)
	};
	vector<Widget*> cont = {
		new Layout(1.f, std::move(midl), false, 0),
		message = new Label(superHeight, World::game()->getMyTurn() ? Game::messageTurnGo : Game::messageTurnWait, nullptr, nullptr, Texture(), Label::Alignment::center, false),
		new Widget(Layout::defaultItemSpacing * 2)
	};
	updateFavorIcon(World::game()->getMyTurn(), World::game()->getFavorCount(), World::game()->getFavorTotal());
	return new Layout(1.f, std::move(cont), true, 0);
}

// PROG SETTINGS

void ProgSettings::eventEscape() {
	if (!tryClosePopup())
		World::program()->eventOpenMainMenu();
}

Layout* ProgSettings::createLayout() {
	// side bar
	vector<string> tps = {
		"Reset",
		"Info",
		"Back"
	};
	int optLength = findMaxLength(tps.begin(), tps.end());
	vector<Widget*> lft = {
		new Label(lineHeight, popBack(tps), &Program::eventOpenMainMenu),
		new Label(lineHeight, popBack(tps), &Program::eventOpenInfo),
		new Widget(0),
		new Label(lineHeight, popBack(tps), &Program::eventResetSettings)
	};

	// resolution list
	vector<vec2i> sizes = World::window()->displaySizes();
	vector<SDL_DisplayMode> modes = World::window()->displayModes();
	pixelformats.clear();
	for (const SDL_DisplayMode& it : modes)
		pixelformats.emplace(pixelformatName(it.format), it.format);

	vector<string> winsiz(sizes.size()), dmodes(modes.size());
	std::transform(sizes.begin(), sizes.end(), winsiz.begin(), [](vec2i size) -> string { return size.toString(rv2iSeparator); });
	std::transform(modes.begin(), modes.end(), dmodes.begin(), dispToFstr);
	vector<string> samples = { "0", "1", "2", "4" };

	// setting buttons, labels and action fields for labels
	Text aleft(arrowLeft);
	Text aright(arrowRight);
	Text aptx("Apply", lineHeight);
	vector<string> txs = {
		"Display",
		"Screen",
		"Size",
		"Mode",
		"VSync",
		"Multisamples",
		"Gamma",
		"Volume"
	};
	vector<string> tips = {
		"Keep 0 cause it doesn't really work",
		"Window or fullscreen (\"desktop\" is native resolution)",
		"Window size",
		"Fullscreen display properties",
		"Anti-Aliasing multisamples (requires restart)",
		"Brightness",
		"Audio volume",
		"Applies \"Display\", \"Screen\", \"Size\" and \"Mode\""
	};
	vector<string> vsyncTip = { "Immediate: off", "Synchronized: on", "Adaptive: on and smooth (works on fewer computers)" };
	std::reverse(txs.begin(), txs.end());
	std::reverse(tips.begin(), tips.end());
	sizet lnc = txs.size();
	int descLength = findMaxLength(txs.begin(), txs.end());
	int slleLength = Text::strLen("000");

	vector<Widget*> lx[] = { {
		new Label(descLength, popBack(txs)),
		display = new LabelEdit(1.f, toStr(World::sets()->display), nullptr, nullptr, nullptr, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		new Label(aleft.length, aleft.text, &Program::eventSBPrev, nullptr, makeTooltip(tips.back())),
		screen = new SwitchBox(1.f, Settings::screenNames.data(), uint(Settings::screenNames.size()), Settings::screenNames[uint8(World::sets()->screen)], nullptr, makeTooltip(tips.back())),
		new Label(aright.length, aright.text, &Program::eventSBNext, nullptr, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		new Label(aleft.length, aleft.text, &Program::eventSBPrev, nullptr, makeTooltip(tips.back())),
		winSize = new SwitchBox(1.f, winsiz.data(), uint(winsiz.size()), World::sets()->size.toString(rv2iSeparator), nullptr, makeTooltip(tips.back())),
		new Label(aright.length, aright.text, &Program::eventSBNext, nullptr, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		new Label(aleft.length, aleft.text, &Program::eventSBPrev, nullptr, makeTooltip(tips.back())),
		dspMode = new SwitchBox(1.f, dmodes.data(), uint(dmodes.size()), dispToFstr(World::sets()->mode), nullptr, makeTooltip(tips.back())),
		new Label(aright.length, aright.text, &Program::eventSBNext, nullptr, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		new Label(aleft.length, aleft.text, &Program::eventSBPrev, nullptr, makeTooltip(vsyncTip)),
		new SwitchBox(1.f, Settings::vsyncNames.data(), uint(Settings::vsyncNames.size()), Settings::vsyncNames[uint8(int8(World::sets()->vsync)+1)], &Program::eventSetVsync, makeTooltip(vsyncTip)),
		new Label(aright.length, aright.text, &Program::eventSBNext, nullptr, makeTooltip(vsyncTip))
	}, {
		new Label(descLength, popBack(txs)),
		new Label(aleft.length, aleft.text, &Program::eventSBPrev, nullptr, makeTooltip(tips.back())),
		new SwitchBox(1.f, samples.data(), uint(samples.size()), toStr(World::sets()->msamples), &Program::eventSetSamples, makeTooltip(tips.back())),
		new Label(aright.length, aright.text, &Program::eventSBNext, nullptr, makeTooltip(popBack(tips)))
	}, {
		new Label(descLength, popBack(txs)),
		new Slider(1.f, int(World::sets()->gamma * gammaStepFactor), 0, int(Settings::gammaMax * gammaStepFactor), &Program::eventSetGammaSL, nullptr, makeTooltip(tips.back())),
		new LabelEdit(slleLength, toStr(World::sets()->gamma), &Program::eventSetGammaLE, nullptr, nullptr, makeTooltip(popBack(tips)))
	}, World::audio() ? vector<Widget*>({
		new Label(descLength, popBack(txs)),
		new Slider(1.f, World::sets()->avolume, 0, SDL_MIX_MAXVOLUME, &Program::eventSetVolumeSL, nullptr, makeTooltip(tips.back())),
		new LabelEdit(slleLength, toStr(World::sets()->avolume), &Program::eventSetVolumeLE, nullptr, nullptr, makeTooltip(popBack(tips)))
	}) : vector<Widget*>({
		new Label(descLength, popBack(txs)),
		new Label(1.f, "missing audio device")
	}), };
	vector<Widget*> lns(lnc + 2);
	for (sizet i = 0; i < lnc; i++)
		lns[i] = new Layout(lineHeight, std::move(lx[i]), false);
	lns[lnc] = new Widget(0);
	lns[lnc+1] = new Layout(lineHeight, { new Label(aptx.length, aptx.text, &Program::eventApplySettings, nullptr, makeTooltip(popBack(tips))) }, false);

	// root layout
	vector<Widget*> cont = {
		new Layout(optLength, std::move(lft), true),
		new ScrollArea(1.f, std::move(lns))
	};
	return new Layout(1.f, cont, false, superSpacing);
}

SDL_DisplayMode ProgSettings::currentMode() const {
	SDL_DisplayMode mode = strToDisp(dspMode->getText());
	if (string::const_reverse_iterator sit = std::find_if(dspMode->getText().rbegin(), dspMode->getText().rend(), [](char c) -> bool { return isSpace(c); }); sit != dspMode->getText().rend())
		if (umap<string, uint32>::const_iterator pit = pixelformats.find(string(sit.base(), dspMode->getText().end())); pit != pixelformats.end())
			mode.format = pit->second;
	return mode;
}

// PROG INFO

const array<string, ProgInfo::powerNames.size()> ProgInfo::powerNames = {
	"UNKNOWN",
	"ON_BATTERY",
	"NO_BATTERY",
	"CHARGING",
	"CHARGED"
};

void ProgInfo::eventEscape() {
	if (!tryClosePopup())
		World::program()->eventOpenSettings();
}

Layout* ProgInfo::createLayout() {
	// side bar
	vector<string> tps = {
		"Back",
		"Menu"
	};
	int optLength = findMaxLength(tps.begin(), tps.end());
	vector<Widget*> lft = {
		new Label(lineHeight, popBack(tps), &Program::eventOpenMainMenu),
		new Label(lineHeight, popBack(tps), &Program::eventOpenSettings)
	};

	// information block
	vector<string> titles = {
		WindowSys::title,
		"CPU",
		"RAM",
		"Audio",
		"Video",
		"Display",
		"Power",
		"Playback",
		"Recording",
		"Audio Drivers",
		"Video Drivers",
		"Video Displays",
		"Render Drivers"
	};
	std::reverse(titles.begin(), titles.end());
	vector<string> args = {
		"Platform",			// program
		"Path",
		"SDL Version",
		"Logical cores",	// CPU
		"L1 line size",
		"Features",
		"Size",				// RAM
		"Current",			// audio
		"Current",			// video
		"State",			// power
		"Time"
	};
	std::reverse(args.begin(), args.end());
	vector<string> dispArgs = {
		"Index",
		"Name",
		"Resolution",
		"Refresh rate",
		"Pixel format",
		"DPI diagonal",
		"DPI horizontal",
		"DPI vertical",
		"Modes"
	};
	vector<string> rendArgs = {
		"Index",
		"Name",
		"Flags",
		"Max tex size",
		"Tex formats"
	};
	int argWidth = findMaxLength({ &args, &dispArgs, &rendArgs });

	vector<Widget*> lines;
	appendProgram(lines, argWidth, args, titles);
	appendCPU(lines, argWidth, args, titles);
	appendRAM(lines, argWidth, args, titles);
	appendCurrent(lines, argWidth, args, titles, SDL_GetCurrentAudioDriver);
	appendCurrent(lines, argWidth, args, titles, SDL_GetCurrentVideoDriver);
	appendCurrentDisplay(lines, argWidth, dispArgs, titles);
	appendPower(lines, argWidth, args, titles);
	appendDevices(lines, argWidth, titles, SDL_GetNumAudioDevices, SDL_GetAudioDeviceName, SDL_FALSE);
	appendDevices(lines, argWidth, titles, SDL_GetNumAudioDevices, SDL_GetAudioDeviceName, SDL_TRUE);
	appendDrivers(lines, argWidth, titles, SDL_GetNumAudioDrivers, SDL_GetAudioDriver);
	appendDrivers(lines, argWidth, titles, SDL_GetNumVideoDrivers, SDL_GetVideoDriver);
	appendDisplays(lines, argWidth, findMaxLength(dispArgs.begin(), dispArgs.end()), dispArgs, titles);
	appendRenderers(lines, argWidth, rendArgs, titles);

	// root layout
	vector<Widget*> cont = {
		new Layout(optLength, std::move(lft), true),
		new ScrollArea(1.f, std::move(lines))
	};
	return new Layout(1.f, cont, false, superSpacing);
}

void ProgInfo::appendProgram(vector<Widget*>& lines, int width, vector<string>& args, vector<string>& titles) {
	lines.insert(lines.end(), {
		new Label(superHeight, popBack(titles)),
		new Layout(lineHeight, { new Label(width, popBack(args)), new Label(1.f, SDL_GetPlatform()) }, false)
	});

	if (char* basePath = SDL_GetBasePath()) {
		lines.push_back(new Layout(lineHeight, { new Label(width, std::move(args.back())), new Label(1.f, basePath) }, false));
		SDL_free(basePath);
	}
	args.pop_back();

	SDL_version ver;
	SDL_GetVersion(&ver);
	Text vernum(toStr(ver.major) + '.' + toStr(ver.minor) + '.' + toStr(ver.patch));
	Text revnum(toStr(SDL_GetRevisionNumber()));

	lines.insert(lines.end(), {
		new Layout(lineHeight, { new Label(width, popBack(args)), new Label(vernum.length, std::move(vernum.text)), new Label(revnum.length, std::move(revnum.text)), new Label(1.f, SDL_GetRevision()) }, false),
		new Widget(lineHeight)
	});
}

void ProgInfo::appendCPU(vector<Widget*>& lines, int width, vector<string>& args, vector<string>& titles) {
	vector<string> features;
	if (SDL_Has3DNow())
		features.emplace_back("3DNow");
	if (SDL_HasAVX())
		features.emplace_back("AVX");
	if (SDL_HasAVX2())
		features.emplace_back("AVX2");
	if (SDL_HasAltiVec())
		features.emplace_back("AltiVec");
	if (SDL_HasMMX())
		features.emplace_back("MMX");
	if (SDL_HasRDTSC())
		features.emplace_back("RDTSC");
	if (SDL_HasSSE())
		features.emplace_back("SSE");
	if (SDL_HasSSE2())
		features.emplace_back("SSE2");
	if (SDL_HasSSE3())
		features.emplace_back("SSE3");
	if (SDL_HasSSE41())
		features.emplace_back("SSE41");
	if (SDL_HasSSE42())
		features.emplace_back("SSE42");

	lines.insert(lines.end(), {
		new Label(superHeight, popBack(titles)),
		new Layout(lineHeight, { new Label(width, popBack(args)), new Label(1.f, toStr(SDL_GetCPUCount())) }, false),
		new Layout(lineHeight, { new Label(width, popBack(args)), new Label(1.f, toStr(SDL_GetCPUCacheLineSize()) + 'B') }, false)
	});
	if (!features.empty()) {
		lines.push_back(new Layout(lineHeight, { new Label(width, popBack(args)), new Label(1.f, std::move(features[0])) }, false));
		for (sizet i = 1; i < features.size(); i++)
			lines.push_back(new Layout(lineHeight, { new Widget(width), new Label(1.f, std::move(features[i])) }, false));
		features.clear();
	} else
		lines.push_back(new Layout(lineHeight, { new Label(width, popBack(args)), new Widget() }, false));
	lines.push_back(new Widget(lineHeight));
}

void ProgInfo::appendRAM(vector<Widget*>& lines, int width, vector<string>& args, vector<string>& titles) {
	lines.insert(lines.end(), {
		new Label(superHeight, popBack(titles)),
		new Layout(lineHeight, { new Label(width, popBack(args)), new Label(1.f, toStr(SDL_GetSystemRAM()) + "MB") }, false),
		new Widget(lineHeight)
	});
}

void ProgInfo::appendCurrent(vector<Widget*>& lines, int width, vector<string>& args, vector<string>& titles, const char* (*value)()) {
	lines.push_back(new Label(superHeight, popBack(titles)));
	if (const char* name = value())
		lines.push_back(new Layout(lineHeight, { new Label(width, popBack(args)), new Label(1.f, name) }, false));
	else
		lines.push_back(new Layout(lineHeight, { new Label(width, popBack(args)), new Button() }, false));
	lines.push_back(new Widget(lineHeight));
}

void ProgInfo::appendCurrentDisplay(vector<Widget*>& lines, int width, const vector<string>& args, vector<string>& titles) {
	lines.push_back(new Label(superHeight, popBack(titles)));
	if (int i = World::window()->displayID(); i >= 0)
		appendDisplay(lines, i, width, args);
	lines.push_back(new Widget(lineHeight));
}

void ProgInfo::appendDisplay(vector<Widget*>& lines, int i, int width, const vector<string>& args) {
	lines.push_back(new Layout(lineHeight, { new Label(width, args[0]), new Label(1.f, toStr(i)) }, false));
	if (const char* name = SDL_GetDisplayName(i))
		lines.push_back(new Layout(lineHeight, { new Label(width, args[1]), new Label(1.f, name) }, false));
	if (SDL_DisplayMode mode; !SDL_GetDesktopDisplayMode(i, &mode))
		lines.insert(lines.end(), {
			new Layout(lineHeight, { new Label(width, args[2]), new Label(1.f, toStr(mode.w) + " x " + toStr(mode.h)) }, false),
			new Layout(lineHeight, { new Label(width, args[3]), new Label(1.f, toStr(mode.refresh_rate) + "Hz") }, false),
			new Layout(lineHeight, { new Label(width, args[4]), new Label(1.f, pixelformatName(mode.format)) }, false)
		});
	if (float ddpi, hdpi, vdpi; !SDL_GetDisplayDPI(i, &ddpi, &hdpi, &vdpi))
		lines.insert(lines.end(), {
			new Layout(lineHeight, { new Label(width, args[5]), new Label(1.f, toStr(ddpi)) }, false),
			new Layout(lineHeight, { new Label(width, args[6]), new Label(1.f, toStr(hdpi)) }, false),
			new Layout(lineHeight, { new Label(width, args[7]), new Label(1.f, toStr(vdpi)) }, false)
		});
}

void ProgInfo::appendPower(vector<Widget*>& lines, int width, vector<string>& args, vector<string>& titles) {
	int secs, pct;
	SDL_PowerState power = SDL_GetPowerInfo(&secs, &pct);
	Text tprc(pct >= 0 ? toStr(pct) + '%' : "inf");

	lines.insert(lines.end(), {
		new Label(superHeight, popBack(titles)),
		new Layout(lineHeight, { new Label(width, popBack(args)), new Label(1.f, powerNames[power]) }, false),
		new Layout(lineHeight, { new Label(width, popBack(args)), new Label(tprc.length, std::move(tprc.text)), new Label(1.f, secs >= 0 ? toStr(secs) + 's' : "inf") }, false),
		new Widget(lineHeight)
	});
}

void ProgInfo::appendDevices(vector<Widget*>& lines, int width, vector<string>& titles, int (*limit)(int), const char* (*value)(int, int), int arg) {
	lines.push_back(new Label(superHeight, popBack(titles)));
	for (int i = 0; i < limit(arg); i++)
		if (const char* name = value(i, arg))
			lines.push_back(new Layout(lineHeight, { new Label(width, toStr(i)), new Label(1.f, name) }, false));
	lines.push_back(new Widget(lineHeight));
}

void ProgInfo::appendDrivers(vector<Widget*>& lines, int width, vector<string>& titles, int (*limit)(), const char* (*value)(int)) {
	lines.push_back(new Label(superHeight, popBack(titles)));
	for (int i = 0; i < limit(); i++)
		if (const char* name = value(i))
			lines.push_back(new Layout(lineHeight, { new Label(width, toStr(i)), new Label(1.f, name) }, false));
	lines.push_back(new Widget(lineHeight));
}

void ProgInfo::appendDisplays(vector<Widget*>& lines, int argWidth, int dispWidth, const vector<string>& args, vector<string>& titles) {
	lines.push_back(new Label(superHeight, popBack(titles)));
	for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
		appendDisplay(lines, i, argWidth, args);

		lines.push_back(new Layout(lineHeight, { new Label(1.f, args[8]) }, false));
		for (int j = 0; j < SDL_GetNumDisplayModes(i); j++) {
			lines.push_back(new Layout(lineHeight, { new Widget(argWidth / 2), new Label(dispWidth, args[0]), new Label(1.f, toStr(j)) }, false));
			if (SDL_DisplayMode mode; !SDL_GetDisplayMode(i, j, &mode))
				lines.insert(lines.end(), {
					new Layout(lineHeight, { new Widget(argWidth / 2), new Label(dispWidth, args[2]), new Label(1.f, toStr(mode.w) + " x " + toStr(mode.h)) }, false),
					new Layout(lineHeight, { new Widget(argWidth / 2), new Label(dispWidth, args[3]), new Label(1.f, toStr(mode.refresh_rate)) }, false),
					new Layout(lineHeight, { new Widget(argWidth / 2), new Label(dispWidth, args[4]), new Label(1.f, pixelformatName(mode.format)) }, false)
				});
			if (j < SDL_GetNumDisplayModes(i) - 1)
				lines.push_back(new Widget(lineHeight / 2));
		}
		if (i < SDL_GetNumVideoDisplays() - 1)
			lines.push_back(new Widget(lineHeight / 2));
	}
	lines.push_back(new Widget(lineHeight));
}

void ProgInfo::appendRenderers(vector<Widget*>& lines, int width, const vector<string>& args, vector<string>& titles) {
	lines.push_back(new Label(superHeight, popBack(titles)));
	for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
		lines.push_back(new Layout(lineHeight, { new Label(width, args[0]), new Label(1.f, toStr(i)) }, false));
		if (SDL_RendererInfo info; !SDL_GetRenderDriverInfo(i, &info)) {
			lines.push_back(new Layout(lineHeight, { new Label(width, args[1]), new Label(1.f, info.name) }, false));

			vector<string> flags;
			if (info.flags & SDL_RENDERER_SOFTWARE)
				flags.emplace_back("SOFTWARE");
			if (info.flags & SDL_RENDERER_ACCELERATED)
				flags.emplace_back("ACCELERATED");
			if (info.flags & SDL_RENDERER_PRESENTVSYNC)
				flags.emplace_back("PRESENTVSYNC");
			if (info.flags & SDL_RENDERER_TARGETTEXTURE)
				flags.emplace_back("TARGETTEXTURE");

			if (!flags.empty()) {
				lines.push_back(new Layout(lineHeight, { new Label(width, args[2]), new Label(1.f, std::move(flags[0])) }, false));
				for (sizet j = 1; j < flags.size(); j++)
					lines.push_back(new Layout(lineHeight, { new Widget(width), new Label(1.f, std::move(flags[j])) }, false));
			} else
				lines.push_back(new Layout(lineHeight, { new Label(width, args[2]), new Widget() }, false));

			lines.push_back(new Layout(lineHeight, { new Label(width, args[3]), new Label(1.f, toStr(info.max_texture_width) + " x " + toStr(info.max_texture_height)) }, false));
			if (info.num_texture_formats) {
				lines.push_back(new Layout(lineHeight, { new Label(width, args[4]), new Label(1.f, pixelformatName(info.texture_formats[0])) }, false));
				for (uint32 i = 1; i < info.num_texture_formats; i++)
					lines.push_back(new Layout(lineHeight, { new Widget(width), new Label(1.f, pixelformatName(info.texture_formats[i])) }, false));
			} else
				lines.push_back(new Layout(lineHeight, { new Label(width, args[4]), new Label(1.f, toStr(i)) }, false));
		}
		if (i < SDL_GetNumRenderDrivers() - 1)
			lines.push_back(new Widget(lineHeight / 2));
	}
}
