#include "engine/world.h"
#include <ctime>

// RECORD

Record::Record(Piece* actor, Action action) :
	actor(actor),
	action(action)
{}

void Record::update(Piece* doActor, Action didActions) {
	actor = doActor;
	action |= didActions;
}

void Record::updateProtectionColors(bool on) const {
	if (actor)
		actor->setEmission(actorProtected() && on ? actor->getEmission() | BoardObject::EMI_DIM : actor->getEmission() & ~BoardObject::EMI_DIM);
}

// GAME

const array<uint16 (*)(uint16, svec2), Game::adjacentStraight.size()> Game::adjacentStraight = {
	[](uint16 id, svec2 lim) -> uint16 { return id / lim.x ? id - lim.x : UINT16_MAX; },				// up
	[](uint16 id, svec2 lim) -> uint16 { return id % lim.x ? id - 1 : UINT16_MAX; },					// left
	[](uint16 id, svec2 lim) -> uint16 { return id % lim.x != lim.x - 1 ? id + 1 : UINT16_MAX; },		// right
	[](uint16 id, svec2 lim) -> uint16 { return id / lim.x != lim.y - 1 ? id + lim.x : UINT16_MAX; }	// down
};

const array<uint16 (*)(uint16, svec2), Game::adjacentFull.size()> Game::adjacentFull = {
	[](uint16 id, svec2 lim) -> uint16 { return id / lim.x && id % lim.x ? id - lim.x - 1 : UINT16_MAX; },							// left up
	[](uint16 id, svec2 lim) -> uint16 { return id / lim.x ? id - lim.x : UINT16_MAX; },											// up
	[](uint16 id, svec2 lim) -> uint16 { return id / lim.x && id % lim.x != lim.x - 1  ? id - lim.x + 1 : UINT16_MAX; },			// right up
	[](uint16 id, svec2 lim) -> uint16 { return id % lim.x ? id - 1 : UINT16_MAX; },												// left
	[](uint16 id, svec2 lim) -> uint16 { return id % lim.x != lim.x - 1 ? id + 1 : UINT16_MAX; },									// right
	[](uint16 id, svec2 lim) -> uint16 { return id / lim.x != lim.y - 1 && id % lim.x ? id + lim.x - 1 : UINT16_MAX; },				// left down
	[](uint16 id, svec2 lim) -> uint16 { return id / lim.x != lim.y - 1 ? id + lim.x : UINT16_MAX; },								// down
	[](uint16 id, svec2 lim) -> uint16 { return id / lim.x != lim.y - 1 && id % lim.x != lim.x - 1 ? id + lim.x + 1 : UINT16_MAX; }	// right down
};

Game::Game() :
	randGen(createRandomEngine()),
	randDist(0, Com::Config::randomLimit - 1),
	ground(vec3(Com::Config::boardWidth / 2.f, -6.f, Com::Config::boardWidth / 2.f), vec3(0.f), vec3(1.f), World::scene()->mesh("ground"), World::scene()->material("ground"), World::scene()->texture("grass")),
	board(vec3(Com::Config::boardWidth / 2.f, 0.f, Com::Config::boardWidth / 2.f), vec3(0.f), vec3(1.f), World::scene()->mesh("table"), World::scene()->material("board"), World::scene()->texture("rock")),
	bgrid(vec3(0.f), vec3(0.f), vec3(1.f), &gridat, World::scene()->material("grid"), World::scene()->blank()),
	screen(vec3(Com::Config::boardWidth / 2.f, screenYUp, Com::Config::boardWidth / 2.f), vec3(0.f), vec3(1.f), World::scene()->mesh("screen"), World::scene()->material("screen"), World::scene()->texture("wall")),
	ffpad(gtop(svec2(UINT16_MAX), 0.001f), vec3(0.f), vec3(0.f), World::scene()->mesh("outline"), World::scene()->material("grid"), World::scene()->blank(), false, false)	// show indicates if favor is being used and pos informs tile type
{}

void Game::updateConfigValues() {
	boardHeight = config.homeSize.y * 2 + 1;
	objectSize = Com::Config::boardWidth / float(std::max(config.homeSize.x, boardHeight));
	tilesOffset = (Com::Config::boardWidth - objectSize * vec2(config.homeSize.x, boardHeight)) / 2.f;
	bobOffset = objectSize / 2.f + tilesOffset;
	boardBounds = vec4(tilesOffset.x, tilesOffset.y, tilesOffset.x + objectSize * config.homeSize.x, tilesOffset.y + objectSize * boardHeight);
	ffpad.setScl(vec3(objectSize, 1.f, objectSize));
}

vector<Object*> Game::initObjects(const Com::Config& cfg) {
	config = cfg;
	updateConfigValues();
	tiles.update(config);
	pieces.update(config);
	setBgrid();
	screen.setPos(vec3(screen.getPos().x, screen.getPos().y, Com::Config::boardWidth / 2.f - objectSize / 2.f));

	// prepare objects for setup
	setTiles(tiles.ene(), 0, false);
	setMidTiles();
	setTiles(tiles.own(), config.homeSize.y + 1, true);
	setPieces(pieces.own(), PI, World::scene()->material("ally"));
	setPieces(pieces.ene(), 0.f, World::scene()->material("enemy"));

	// collect array of references to all objects
	array<Object*, 4> others = { &ground, &board, &bgrid, &screen };
	sizet oi = others.size();
	vector<Object*> objs(others.size() + tiles.getSize() + 1 + pieces.getSize());
	std::copy(others.begin(), others.end(), objs.begin());
	setObjectAddrs(tiles.begin(), tiles.getSize(), objs, oi);
	objs[oi++] = &ffpad;
	setObjectAddrs(pieces.begin(), pieces.getSize(), objs, oi);
	return objs;
}

template <class T>
void Game::setObjectAddrs(T* data, sizet size, vector<Object*>& dst, sizet& id) {
	for (sizet i = 0; i < size; i++)
		dst[id+i] = &data[i];
	id += size;
}

void Game::uninitObjects() {
	setTilesInteract(tiles.begin(), tiles.getSize(), Tile::Interact::ignore);
	disableOwnPiecesInteract(false);
}

void Game::prepareMatch() {
	// set interactivity and reassign callback events
	for (Tile* it = tiles.ene(); it != tiles.mid(); it++)
		it->show = true;
	for (Piece* it = pieces.ene(); it != pieces.end(); it++)
		it->show = pieceOnBoard(it);
	ownRec = eneRec = Record();

	// check for favor
	favorCount = 0;
	favorTotal = config.favorMax;
	for (Piece* throne = getOwnPieces(Com::Piece::throne); throne != pieces.ene() && favorCount < favorTotal; throne++)
		if (svec2 pos = ptog(throne->getPos()); getTile(pos)->getType() == Com::Tile::fortress) {
			throne->lastFortress = posToId(pos);
			favorCount++;
		}

	// rearange middle tiles
	vector<Com::Tile> mid(config.homeSize.x);
	for (uint16 i = 0; i < config.homeSize.x; i++)
		mid[i] = tiles.mid(i)->getType();
	rearangeMiddle(mid.data(), World::state<ProgSetup>()->rcvMidBuffer.data());
	for (uint16 i = 0; i < config.homeSize.x; i++)
		tiles.mid(i)->setType(mid[i]);
}

void Game::rearangeMiddle(Com::Tile* mid, Com::Tile* buf) {
	if (!myTurn)
		std::swap(mid, buf);
	bool fwd = myTurn == config.shiftLeft;
	for (uint16 x, i = fwd ? 0 : config.homeSize.x - 1, fm = btom<uint16>(fwd), rm = btom<uint16>(!fwd); i < config.homeSize.x; i += fm)
		if (buf[i] != Com::Tile::empty) {
			if (mid[i] == Com::Tile::empty)
				mid[i] = buf[i];
			else {
				if (config.shiftNear) {
					for (x = i + rm; x < config.homeSize.x && mid[x] != Com::Tile::empty; x += rm);
					if (x >= config.homeSize.x)
						for (x = i + fm; x < config.homeSize.x && mid[x] != Com::Tile::empty; x += fm);
				} else
					for (x = fwd ? 0 : config.homeSize.x - 1; x < config.homeSize.x && mid[x] != Com::Tile::empty; x += fm);
				mid[x] = buf[i];
			}
		}
	std::replace(mid, mid + config.homeSize.x, Com::Tile::empty, Com::Tile::fortress);
	if (!myTurn)
		std::copy(mid, mid + config.homeSize.x, buf);
}

void Game::checkOwnTiles() const {
	uint16 empties = 0;
	for (uint16 y = config.homeSize.y + 1; y < boardHeight; y++) {
		// collect information and check if the fortress isn't touching a border
		uint8 cnt[uint8(Com::Tile::fortress)] = { 0, 0, 0, 0 };
		for (uint16 x = 0; x < config.homeSize.x; x++) {
			if (Com::Tile type = tiles[y * config.homeSize.x + x].getType(); type < Com::Tile::fortress)
				cnt[uint8(type)]++;
			else if (outRange(svec2(x, y), svec2(1, config.homeSize.y + 1), svec2(config.homeSize.x - 2, boardHeight - 2)))
				throw firstUpper(Com::tileNames[uint8(Com::Tile::fortress)]) + " at " + toStr(x) + '|' + toStr(y - config.homeSize.y - 1) + " not allowed";
			else
				empties++;
		}
		// check diversity in each row
		for (uint8 i = 0; i < uint8(Com::Tile::fortress); i++)
			if (!cnt[i] && config.tileAmounts[i])
				throw firstUpper(Com::tileNames[i]) + " missing in row " + toStr(y);
	}
	if (empties > config.tileAmounts[uint8(Com::Tile::fortress)])
		throw string("Not all tiles were placed");
}

void Game::checkMidTiles() const {
	// collect information
	uint8 cnt[uint8(Com::Tile::fortress)] = { 0, 0, 0, 0 };
	for (uint16 i = 0; i < config.homeSize.x; i++)
		if (Com::Tile type = tiles.mid(i)->getType(); type < Com::Tile::fortress)
			cnt[uint8(type)]++;
	// check if all tiles were placed
	for (uint8 i = 0; i < uint8(Com::Tile::fortress); i++)
		if (cnt[i] < config.middleAmounts[i])
			throw firstUpper(Com::tileNames[i]) + " wasn't placed";
}

void Game::checkOwnPieces() const {
	for (const Piece* it = pieces.own(); it != pieces.ene(); it++)
		if (!it->show && it->getType() != Com::Piece::dragon)
			throw firstUpper(Com::pieceNames[uint8(it->getType())]) + " wasn't placed";
}

vector<uint16> Game::countTiles(const Tile* tiles, uint16 num, vector<uint16> cnt) {
	for (uint16 i = 0; i < num; i++)
		if (uint8(tiles[i].getType()) < cnt.size())
			cnt[uint8(tiles[i].getType())]--;
	return cnt;
}

vector<uint16> Game::countOwnPieces() const {
	vector<uint16> cnt(config.pieceAmounts.begin(), config.pieceAmounts.end());
	for (const Piece* it = pieces.own(); it != pieces.ene(); it++)
		if (pieceOnHome(it))
			cnt[uint8(it->getType())]--;
	return cnt;
}

void Game::fillInFortress() {
	for (Tile* it = tiles.own(); it != tiles.end(); it++)
		if (it->getType() == Com::Tile::empty)
			it->setType(Com::Tile::fortress);
}

void Game::takeOutFortress() {
	for (Tile* it = tiles.own(); it != tiles.end(); it++)
		if (it->getType() == Com::Tile::fortress)
			it->setType(Com::Tile::empty);
}

void Game::setFfpadPos(bool force, svec2 pos) {
	if (ProgMatch* pm = World::state<ProgMatch>(); pm->favorIconOn() || force) {
		ffpad.setPos(gtop(pos, ffpad.getPos().y));
		ffpad.show = pm->favorIconSelect() != FavorAct::off && inRange(pos, svec2(0), svec2(config.homeSize.x - 1, boardHeight - 1)) && getTile(pos)->getType() < Com::Tile::fortress;
	}
}

Com::Tile Game::checkFavor() {
	return World::state<ProgMatch>()->favorIconSelect() != FavorAct::off ? getTile(ptog(ffpad.getPos()))->getType() : Com::Tile::empty;
}

void Game::pieceMove(Piece* piece, svec2 pos, Piece* occupant, bool forceSwitch) {
	svec2 spos = ptog(piece->getPos());
	Tile *stil = getTile(spos), *dtil = getTile(pos);
	Action action = occupant ? isOwnPiece(occupant) || forceSwitch ? ACT_SWAP : ACT_ATCK : ACT_MOVE;
	FavorAct fact = World::state<ProgMatch>()->favorIconSelect();
	Com::Tile favor = checkFavor();
	if (checkMove(piece, spos, occupant, pos, action, fact, favor); !survivalCheck(piece, occupant, stil, dtil, action, fact, favor)) {
		failSurvivalCheck(piece, action);
		return;
	}

	switch (action) {
	case ACT_MOVE:
		if (stil->isBreachedFortress())
			updateFortress(stil, false);
		placePiece(piece, pos);
		if (fact == FavorAct::now && favor == Com::Tile::plains)
			action = ACT_NONE;
		ownRec.update(piece, action);
		break;
	case ACT_SWAP:
		placePiece(occupant, spos);
		placePiece(piece, pos);
		if (fact == FavorAct::now && (favor == Com::Tile::forest || favor == Com::Tile::water))
			action = ACT_NONE;
		ownRec.update(piece, action);
		break;
	case ACT_ATCK: {
		bool okFort = dtil->isUnbreachedFortress();
		if (okFort)
			updateFortress(dtil, true);
		if (!okFort || piece->getType() == Com::Piece::throne) {
			removePiece(occupant);
			placePiece(piece, pos);
		}
		ownRec.update(piece, action); }
	}
	World::play("move");
	concludeAction(action, fact, favor);
}

void Game::pieceFire(Piece* killer, svec2 pos, Piece* piece) {
	svec2 kpos = ptog(killer->getPos());
	Tile* dtil = getTile(pos);
	FavorAct fact = World::state<ProgMatch>()->favorIconSelect();
	Com::Tile favor = checkFavor();
	if (checkFire(killer, kpos, piece, pos, fact, favor); !survivalCheck(killer, piece, getTile(kpos), dtil, ACT_FIRE, fact, favor)) {
		failSurvivalCheck(killer, ACT_FIRE);
		return;
	}

	if (dtil->isUnbreachedFortress())	// breach fortress
		updateFortress(dtil, true);
	else {								// regular fire
		if (dtil->isBreachedFortress())
			updateFortress(dtil, false);	// restore fortress
		removePiece(piece);
	}
	ownRec.update(killer, ACT_FIRE);
	World::play("ammo");
	concludeAction(ACT_FIRE, fact, favor);
}

void Game::concludeAction(Action last, FavorAct fact, Com::Tile favor) {
	if (checkWin())
		return;

	bool turnOver = ((ownRec.action & ACT_MS) == ACT_MS && !(ownRec.actor->getType() == Com::Piece::warhorse && last == ACT_SWAP)) || (ownRec.action & ACT_AF);
	bool extend = fact == FavorAct::on && (favor == Com::Tile::plains || favor == Com::Tile::forest || favor == Com::Tile::water);
	if (fact == FavorAct::now) {
		if (ownRec.action)
			turnOver = true;
		if (favorCount--; config.favorLimit)
			favorTotal--;
	}
	if (turnOver && !extend)
		endTurn();
	else {
		World::netcp()->sendData(sendb);
		disableOwnPiecesInteract(true, true);
		setPieceInteract(ownRec.actor, true, false, &Program::eventFavorStart, &Program::eventMove, ownRec.actor->firingDistance() ? &Program::eventFire : &Program::eventMove);

		ProgMatch* pm = World::state<ProgMatch>();
		pm->updateFavorIcon(false, favorCount, favorTotal);
		pm->updateTurnIcon(true);
		pm->message->setText("Your turn");
		if (fact == FavorAct::on) {
			if ((turnOver && extend) || favor == Com::Tile::mountain) {
				pm->selectFavorIcon(FavorAct::now);
				pm->updateFnowIcon();
			}
		} else if (pm->updateFnowIcon(); fact == FavorAct::now) {
			pm->selectFavorIcon(FavorAct::off);
			setFfpadPos(true);
		}
	}
}

void Game::placeDragon(svec2 pos, Piece* occupant) {
	if (Tile* til = getTile(pos); isHomeTile(til) && (til->getType() == Com::Tile::fortress || (til->getType() == Com::Tile::mountain && !occupant))) {		// tile needs to be a home fortress or unoccupied homeland mountain
		World::state<ProgMatch>()->decreaseDragonIcon();
		if (occupant)
			removePiece(occupant);
		for (Piece* pce = getOwnPieces(Com::Piece::dragon); pce->getType() == Com::Piece::dragon; pce++)
			if (!pieceOnBoard(pce)) {
				placePiece(pce, pos);
				break;
			}
		checkWin();
	}
}

void Game::setBgrid() {
	gridat.free();
	vector<Vertex> verts((config.homeSize.x + boardHeight + 2) * 2);
	vector<uint16> elems(verts.size());

	for (uint16 i = 0; i < elems.size(); i++)
		elems[i] = i;
	uint16 i = 0;
	for (uint16 x = 0; x <= config.homeSize.x; x++)
		for (uint16 y = 0; y <= boardHeight; y += boardHeight)
			verts[i++] = Vertex(gtop(svec2(x, y), -0.018f) + vec3(-(objectSize / 2.f), 0.f, -(objectSize / 2.f)), Camera::up, vec2(0.f));
	for (uint16 y = 0; y <= boardHeight; y++)
		for (uint16 x = 0; x <= config.homeSize.x; x += config.homeSize.x)
			verts[i++] = Vertex(gtop(svec2(x, y), -0.018f) + vec3(-(objectSize / 2.f), 0.f, -(objectSize / 2.f)), Camera::up, vec2(0.f));
	gridat = Mesh(verts, elems, GL_LINES);
}

void Game::setTiles(Tile* tils, uint16 yofs, bool inter) {
	sizet id = 0;
	for (uint16 y = yofs; y < yofs + config.homeSize.y; y++)
		for (uint16 x = 0; x < config.homeSize.x; x++)
			tils[id++] = Tile(gtop(svec2(x, y)), objectSize, Com::Tile::empty, nullptr, nullptr, nullptr, inter, inter);
}

void Game::setMidTiles() {
	for (uint16 i = 0; i < config.homeSize.x; i++)
		*tiles.mid(i) = Tile(gtop(svec2(i, config.homeSize.y)), objectSize, Com::Tile::empty, nullptr, nullptr, nullptr, false, false);
}

void Game::setPieces(Piece* pces, float rot, const Material* matl) {
	uint8 t = 0;
	for (uint16 i = 0, c = 0; i < pieces.getNum(); i++, c++) {
		for (; c >= config.pieceAmounts[t]; t++, c = 0);
		pces[i] = Piece(gtop(svec2(UINT16_MAX)), rot, objectSize, Com::Piece(t), nullptr, nullptr, nullptr, matl, false, false);
	}
}

void Game::setTilesInteract(Tile* tiles, uint16 num, Tile::Interact lvl, bool dim) {
	for (uint16 i = 0; i < num; i++)
		tiles[i].setInteractivity(lvl, dim);
}

void Game::setPieceInteract(Piece* piece, bool on, bool dim, GCall hgcall, GCall ulcall, GCall urcall) {
	piece->setRaycast(on, dim);
	piece->hgcall = hgcall;
	piece->ulcall = ulcall;
	piece->urcall = urcall;
}

void Game::setOwnPiecesVisible(bool on, bool event) {
	for (Piece* it = pieces.own(); it != pieces.ene(); it++)
		if (setPieceInteract(it, on, false, nullptr, on && event ? &Program::eventMovePiece : nullptr, nullptr); pieceOnHome(it))
			it->setActive(on);
}

void Game::disableOwnPiecesInteract(bool rigid, bool dim) {
	for (Piece* it = pieces.own(); it != pieces.ene(); it++)
		setPieceInteract(it, rigid, dim, nullptr, nullptr, nullptr);
}

Piece* Game::getPieces(Piece* pieces, Com::Piece type) {
	for (uint8 t = 0; t < uint8(type); t++)
		pieces += config.pieceAmounts[t];
	return pieces;
}

Piece* Game::findPiece(Piece* beg, Piece* end, svec2 pos) {
	Piece* pce = std::find_if(beg, end, [this, pos](const Piece& it) -> bool { return ptog(it.getPos()) == pos; });
	return pce != pieces.end() ? pce : nullptr;
}

void Game::checkMove(Piece* piece, svec2 pos, Piece* occupant, svec2 dst, Action action, FavorAct fact, Com::Tile favor) {
	if (pos == dst)
		throw string();

	Tile *stil = getTile(pos), *dtil = getTile(dst);
	switch (checkFavorAction(stil, dtil, occupant, action, fact, favor); action) {
	case ACT_MOVE:
		if (fact == FavorAct::now && favor == Com::Tile::plains)
			checkMoveDestination(pos, dst, &Game::collectTilesBySingle);
		else {
			if (ownRec.action & ~ACT_SWAP)
				throw pastRecordAction();

			switch (piece->getType()) {
			case Com::Piece::spearman:
				checkMoveDestination(pos, dst, stil->getType() == Com::Tile::water && dtil->getType() == Com::Tile::water ? &Game::collectTilesByType : &Game::collectTilesBySingle);
				break;
			case Com::Piece::lancer:
				checkMoveDestination(pos, dst, stil->getType() == Com::Tile::plains && dtil->getType() == Com::Tile::plains ? &Game::collectTilesByType : &Game::collectTilesBySingle);
				break;
			case Com::Piece::dragon:
				checkMoveDestination(pos, dst, config.dragonSingle ? &Game::collectTilesByStraight : &Game::collectTilesByArea, config.dragonDist, spaceAvailibleDragon, config.dragonDiag ? adjacentFull.data() : adjacentStraight.data(), uint8(config.dragonDiag ? adjacentFull.size() : adjacentStraight.size()));
				break;
			default:
				checkMoveDestination(pos, dst, &Game::collectTilesBySingle);
			}
		}
		break;
	case ACT_SWAP:
		if (piece->getType() != Com::Piece::warhorse && (ownRec.action & ~ACT_MOVE) && !(fact == FavorAct::now && (favor == Com::Tile::forest || favor == Com::Tile::water)))
			throw string("You've already switched");
		if (piece->getType() == occupant->getType())
			throw string("Can't swap with a piece of the same type");

		if (!canForestFF(fact, favor, stil, dtil, occupant))
			checkMoveDestination(pos, dst, &Game::collectTilesBySingle);
		break;
	case ACT_ATCK:
		if (firstTurn)
			throw string("Can't attack on first turn");
		if (ownRec.action)
			throw pastRecordAction();

		if (piece->getType() != Com::Piece::throne) {
			checkAttack(piece, occupant, dtil);
			if (piece->getType() == Com::Piece::dragon && dtil->getType() == Com::Tile::forest)
				throw firstUpper(Com::pieceNames[uint8(piece->getType())]) + " can't attack a " + Com::tileNames[uint8(dtil->getType())];
		}
		checkMoveDestination(pos, dst, &Game::collectTilesBySingle);
	}
}

template <class F, class... A>
void Game::checkMoveDestination(svec2 pos, svec2 dst, F check, A... args) {
	uset<uint16> tcol;
	if ((this->*check)(tcol, posToId(pos), args...); !tcol.count(posToId(dst)))
		throw string("Can't move there");
}

void Game::collectTilesByStraight(uset<uint16>& tcol, uint16 pos, uint16 dlim, bool (*stepable)(uint16), uint16 (*const* vmov)(uint16, svec2), uint8 movSize) {
	tcol.insert(pos);
	for (uint8 m = 0; m < movSize; m++) {
		uint16 p = pos;
		for (uint16 i = 0; i < dlim; i++) {
			if (uint16 ni = vmov[m](p, svec2(config.homeSize.x, boardHeight)); ni < tiles.getSize() && stepable(ni))
				tcol.insert(p = ni);
			else
				break;
		}
	}
}

void Game::collectTilesByArea(uset<uint16>& tcol, uint16 pos, uint16 dlim, bool (*stepable)(uint16), uint16 (*const* vmov)(uint16, svec2), uint8 movSize) {
	vector<uint16> dist = Dijkstra::travelDist(pos, dlim, svec2(config.homeSize.x, boardHeight), stepable, vmov, movSize);
	for (uint16 i = 0; i < tiles.getSize(); i++)
		if (dist[i] <= dlim)
			tcol.insert(i);
}

void Game::collectTilesByType(uset<uint16>& tcol, uint16 pos) {
	collectAdjacentTilesByType(tcol, pos, tiles[pos].getType());
	collectTilesBySingle(tcol, pos);
}

void Game::collectAdjacentTilesByType(uset<uint16>& tcol, uint16 pos, Com::Tile type) {
	tcol.insert(pos);
	for (uint16 (*mov)(uint16, svec2) : adjacentFull)
		if (uint16 ni = mov(pos, svec2(config.homeSize.x, boardHeight)); ni < tiles.getSize() && tiles[ni].getType() == type && !tcol.count(ni) && spaceAvailibleAlly(ni))
			collectAdjacentTilesByType(tcol, ni, type);
}

bool Game::spaceAvailibleAlly(uint16 pos) {
	Piece* occ = findPiece(pieces.begin(), pieces.end(), idToPos(pos));
	return !occ || isOwnPiece(occ);
}

bool Game::spaceAvailibleDragon(uint16 pos) {
	Piece* occ = World::game()->findPiece(World::game()->pieces.begin(), World::game()->pieces.end(), World::game()->idToPos(pos));
	return !occ || World::game()->isOwnPiece(occ) || (occ->getType() != Com::Piece::dragon && !occ->firingDistance());
}

void Game::highlightMoveTiles(Piece* pce) {
	for (Tile& it : tiles)
		it.setEmission(it.getEmission() & ~BoardObject::EMI_HIGH);
	if (!pce)
		return;

	uint16 pos = posToId(ptog(pce->getPos()));
	FavorAct fact = World::state<ProgMatch>()->favorIconSelect();
	Com::Tile favor = checkFavor();
	uset<uint16> tcol;
	if (fact == FavorAct::now && favor == Com::Tile::plains)
		collectTilesBySingle(tcol, pos);
	else {
		if (canForestFF(fact, favor, &tiles[pos]))
			for (Piece* it = pieces.own(); it != pieces.ene(); it++)
				if (uint16 tp = posToId(ptog(it->getPos())); tp != pos && tiles[tp].getType() == Com::Tile::forest)
					tcol.insert(tp);

		switch (pce->getType()) {
		case Com::Piece::spearman:
			(this->*(tiles[pos].getType() == Com::Tile::water ? &Game::collectTilesByType : &Game::collectTilesBySingle))(tcol, pos);
			break;
		case Com::Piece::lancer:
			(this->*(tiles[pos].getType() == Com::Tile::plains ? &Game::collectTilesByType : &Game::collectTilesBySingle))(tcol, pos);
			break;
		case Com::Piece::dragon:
			(this->*(config.dragonSingle ? &Game::collectTilesByStraight : &Game::collectTilesByArea))(tcol, pos, config.dragonDist, spaceAvailibleDragon, config.dragonDiag ? adjacentFull.data() : adjacentStraight.data(), uint8(config.dragonDiag ? adjacentFull.size() : adjacentStraight.size()));
			break;
		default:
			collectTilesBySingle(tcol, pos);
		}
	}
	for (uint16 id : tcol)
		tiles[id].setEmission(tiles[id].getEmission() | BoardObject::EMI_HIGH);
}

void Game::checkFire(Piece* killer, svec2 pos, Piece* victim, svec2 dst, FavorAct fact, Com::Tile favor) {
	if (pos == dst)
		throw string();
	if (firstTurn)
		throw string("Can't fire on first turn");
	if (ownRec.action)
		throw pastRecordAction();
	if (!victim)
		throw string("Can't fire at nobody");
	if (isOwnPiece(victim))
		throw string("Can't fire at an ally");

	Tile* dtil = getTile(dst);
	checkFavorAction(getTile(pos), dtil, victim, ACT_FIRE, fact, favor);
	checkAttack(killer, victim, dtil);
	if (victim->getType() == Com::Piece::ranger && dtil->getType() == Com::Tile::mountain)
		throw "Can't fire at a " + Com::pieceNames[uint8(victim->getType())] + " in the " + Com::tileNames[uint8(dtil->getType())] + 's';
	if ((killer->getType() == Com::Piece::crossbowman || killer->getType() == Com::Piece::catapult) && dtil->getType() == Com::Tile::forest)
		throw firstUpper(Com::pieceNames[uint8(killer->getType())]) + " can't fire at a " + Com::tileNames[uint8(dtil->getType())];
	if (uint8 dist = killer->firingDistance(); !collectTilesByDistance(pos, dist).count(posToId(dst)))
		throw firstUpper(Com::pieceNames[uint8(killer->getType())]) + " can only fire at a distance of " + toStr(dist);
}

uset<uint16> Game::collectTilesByDistance(svec2 pos, uint16 dist) {
	uset<uint16> tcol;
	for (svec2 it : { pos - dist, pos - svec2(0, dist), pos + svec2(dist, -dist), pos - svec2(dist, 0), pos + svec2(dist, 0), pos + svec2(-dist, dist), pos + svec2(0, dist), pos + dist })	// TODDO: move array?
		if (it.x < config.homeSize.x && it.y < boardHeight)
			tcol.insert(posToId(it));
	return tcol;
}

void Game::highlightFireTiles(Piece* pce) {
	for (Tile& it : tiles)
		it.setEmission(it.getEmission() & ~BoardObject::EMI_HIGH);
	if (!pce)
		return;
	if (uint8 dist = pce->firingDistance())
		for (uint16 id : collectTilesByDistance(ptog(pce->getPos()), dist))
			tiles[id].setEmission(tiles[id].getEmission() | BoardObject::EMI_HIGH);
}

void Game::checkFavorAction(Tile* src, Tile* dst, Piece* occupant, Action action, FavorAct fact, Com::Tile favor) {
	if (fact != FavorAct::now)
		return;
	if (favor == Com::Tile::plains && action != ACT_MOVE)
		throw string("Current Fate's Favor is limited to moving");
	if (favor == Com::Tile::forest && (action != ACT_SWAP || !canForestFF(fact, favor, src, dst, occupant)))
		throw string("Current Fate's Favor is limited to switching on forest");
	if (favor == Com::Tile::water && action != ACT_SWAP)
		throw string("Current Fate's Favor is limited to switching");
}

void Game::checkAttack(Piece* killer, Piece* victim, Tile* dtil) const {
	if (eneRec.isProtectedMember(victim))
		throw firstUpper(Com::pieceNames[uint8(victim->getType())]) + " is protected";
	if (victim->getType() == Com::Piece::elephant && dtil->getType() == Com::Tile::plains && killer->getType() != Com::Piece::dragon)
		throw firstUpper(Com::pieceNames[uint8(killer->getType())]) + " can't attack an " + Com::pieceNames[uint8(victim->getType())] + " on " + Com::tileNames[uint8(dtil->getType())];
}

bool Game::survivalCheck(Piece* piece, Piece* occupant, Tile* stil, Tile* dtil, Action action, FavorAct fact, Com::Tile favor) {
	if (piece->getType() == Com::Piece::throne && action == ACT_ATCK)
		return true;

	Com::Tile src = action != ACT_FIRE ? stil->getType() : Com::Tile::empty;				// don't check src when firing (only check dst)
	Com::Tile dst = action & (ACT_SWAP | ACT_FIRE) ? dtil->getType() : Com::Tile::empty;	// check dst when switching/firing (in addition to src when switching)
	if (!(dtil->isUnbreachedFortress() && (action & (ACT_ATCK | ACT_FIRE)))) {		// always run survival check when attacking a fortress
		if ((src != Com::Tile::mountain && src != Com::Tile::water && dst != Com::Tile::mountain && dst != Com::Tile::water) || piece->getType() == Com::Piece::dragon)
			return true;
		if ((piece->getType() == Com::Piece::ranger && src == Com::Tile::mountain && dst != Com::Tile::water) || (piece->getType() == Com::Piece::spearman && src == Com::Tile::water && dst != Com::Tile::mountain))
			return true;
	}
	if (fact == FavorAct::now && (favor == Com::Tile::mountain || (favor == Com::Tile::water && isOwnPiece(occupant))))
		return true;
	return randDist(randGen) < config.survivalPass;
}

void Game::failSurvivalCheck(Piece* piece, Action action) {
	ProgMatch* pm = World::state<ProgMatch>();
	pm->selectFavorIcon(FavorAct::off);
	pm->updateFavorIcon(false, favorCount, favorTotal);
	pm->updateFnowIcon();

	if (piece->getType() != Com::Piece::throne && (action == ACT_ATCK || config.survivalMode == Com::Config::Survival::kill)) {
		removePiece(piece);
		World::netcp()->sendData(sendb);
	} else if (ownRec.action || config.survivalMode == Com::Config::Survival::finish)
		endTurn();
	else
		setPieceInteract(piece, true, true, nullptr, nullptr, nullptr);
	pm->message->setText("Failed survival check");
}

bool Game::checkWin() {
	if (checkThroneWin(getOwnPieces(Com::Piece::throne))) {	// no need to check if enemy throne has occupied own fortress
		doWin(false);
		return true;
	}
	if (checkThroneWin(getPieces(pieces.ene(), Com::Piece::throne)) || checkFortressWin()) {
		doWin(true);
		return true;
	}
	return false;
}

bool Game::checkThroneWin(Piece* thrones) {
	if (uint16 c = config.winThrone)
		for (uint16 i = 0; i < config.pieceAmounts[uint8(Com::Piece::throne)]; i++)
			if (!thrones[i].show && !--c)
				return true;
	return false;
}

bool Game::checkFortressWin() {
	if (uint16 cnt = config.winFortress)											// if there's a fortress quota
		for (Tile* tit = tiles.ene(); tit != tiles.mid(); tit++)					// iterate enemy tiles
			if (Piece* pit = pieces.own(); tit->getType() == Com::Tile::fortress)	// if tile is an enemy fortress
				for (uint8 pi = 0; pi < Com::pieceMax; pit += config.pieceAmounts[pi++])	// iterate piece types
					if (config.capturers[pi])										// if the piece type is a capturer
						for (uint16 i = 0; i < config.pieceAmounts[pi]; i++)		// iterate pieces of that type
							if (tileId(tit) == posToId(ptog(pit[i].getPos())) && !--cnt)	// if such a piece is on the fortress
								return true;										// decrement fortress counter and win if 0
	return false;
}

void Game::doWin(bool win) {
	ownRec.action |= win ? ACT_WIN : ACT_LOOSE;
	endTurn();
	World::program()->finishMatch(win);
}

void Game::prepareTurn() {
	setTilesInteract(tiles.begin(), tiles.getSize(), Tile::Interact(myTurn));
	for (Piece* it = pieces.own(); it != pieces.ene(); it++)
		setPieceInteract(it, myTurn && pieceOnBoard(it), false, &Program::eventFavorStart, & Program::eventMove, it->firingDistance() ? &Program::eventFire : &Program::eventMove);
	for (Piece* it = pieces.ene(); it != pieces.end(); it++)
		setPieceInteract(it, myTurn && pieceOnBoard(it), false, nullptr, nullptr, nullptr);
	(myTurn ? eneRec : ownRec).updateProtectionColors(true);

	ProgMatch* pm = World::state<ProgMatch>();
	pm->message->setText(myTurn ? "Your turn" : "Opponent's turn");
	pm->updateFavorIcon(myTurn, favorCount, favorTotal);
	pm->updateFnowIcon(myTurn, favorCount);
	pm->selectFavorIcon(FavorAct::off);
	pm->updateTurnIcon(false);
	pm->setDragonIcon(myTurn);
	setFfpadPos(true);
}

void Game::endTurn() {
	sendb.pushHead(Com::Code::record);
	sendb.push(uint8(ownRec.action));
	sendb.push(inversePieceId(ownRec.actor));
	World::netcp()->sendData(sendb);

	firstTurn = myTurn = false;
	prepareTurn();
}

void Game::recvRecord(uint8* data) {
	uint16 ai = SDLNet_Read16(data + 1);
	eneRec = Record(ai < pieces.getSize() ? &pieces[ai] : nullptr, Action(data[0]));
	ownRec = Record();
	if (eneRec.action & ACT_MOVE)
		World::play("move");
	else if (eneRec.action & ACT_FIRE)
		World::play("ammo");
	myTurn = true;
	prepareTurn();

	if (eneRec.action & (ACT_WIN | ACT_LOOSE))
		World::program()->finishMatch(eneRec.action & ACT_LOOSE);
}

void Game::sendConfig(bool onJoin) {
	uint ofs;
	if (onJoin) {
		ofs = sendb.preallocate(Com::Code::cnjoin, Com::dataHeadSize + 1 + Com::Config::dataSize);
		ofs = sendb.write(uint8(true), ofs);
	} else
		ofs = sendb.preallocate(Com::Code::config);
	World::state<ProgRoom>()->confs[World::program()->curConfig].toComData(&sendb[ofs]);
	World::netcp()->sendData(sendb);
}

void Game::sendStart() {
	std::default_random_engine randGen = createRandomEngine();
	uint8 first = uint8(std::uniform_int_distribution<uint>(0, 1)(randGen));

	uint ofs = sendb.preallocate(Com::Code::start);
	ofs = sendb.write(first, ofs);
	World::state<ProgRoom>()->confs[World::program()->curConfig].toComData(&sendb[ofs]);
	World::netcp()->sendData(sendb);

	firstTurn = myTurn = !first;		// basically simulate recvConfig but without reinitializing config
	World::program()->eventOpenSetup();
}

void Game::recvStart(uint8* data) {
	firstTurn = myTurn = data[0];
	recvConfig(data + 1);
	World::program()->eventOpenSetup();
}

void Game::sendSetup() {
	uint tcnt = tileCompressionSize();
	uint pcnt = pieces.getNum() * sizeof(uint16);
	uint ofs = sendb.preallocate(Com::Code::setup, Com::dataHeadSize + tcnt + pcnt);

	std::fill_n(&sendb[ofs], tcnt, 0);
	for (uint16 i = 0; i < tiles.getExtra(); i++)
		sendb[i/2+ofs] |= compressTile(i);
	ofs += tcnt;

	for (uint16 i = 0; i < pieces.getNum(); i++)
		SDLNet_Write16(pieceOnBoard(pieces.own(i)) ? invertId(posToId(ptog(pieces.own(i)->getPos()))) : UINT16_MAX, &sendb[i*sizeof(uint16)+ofs]);
	World::netcp()->sendData(sendb);
}

void Game::recvConfig(uint8* data) {
	config.fromComData(data);
	updateConfigValues();
}

void Game::recvSetup(uint8* data) {
	for (uint16 i = 0; i < tiles.getHome(); i++)
		tiles[i].setTypeSilent(decompressTile(data, i));
	for (uint16 i = 0; i < config.homeSize.x; i++)
		World::state<ProgSetup>()->rcvMidBuffer[i] = decompressTile(data, tiles.getHome() + i);
	for (uint16 ofs = tileCompressionSize(), i = 0; i < pieces.getNum(); i++) {
		uint16 id = SDLNet_Read16(data + i * sizeof(uint16) + ofs);
		pieces.ene(i)->setPos(gtop(id < tiles.getHome() ? idToPos(id) : svec2(UINT16_MAX)));
	}

	if (ProgSetup* ps = World::state<ProgSetup>(); ps->getStage() == ProgSetup::Stage::ready)
		World::program()->eventOpenMatch();
	else {
		ps->enemyReady = true;
		ps->message->setText(ps->getStage() == ProgSetup::Stage::pieces ? "Opponent ready" : "Hurry the fuck up!");
	}
}

void Game::placePiece(Piece* piece, svec2 pos) {
	if (piece->getType() == Com::Piece::throne && getTile(pos)->getType() == Com::Tile::fortress)
		if (uint16 fid = posToId(pos); piece->lastFortress != fid)
			if (piece->lastFortress = fid; favorCount < favorTotal)
				favorCount++;

	piece->updatePos(pos, true);
	sendb.pushHead(Com::Code::move);
	sendb.push(vector<uint16>{ inversePieceId(piece), invertId(posToId(pos)) });
}

void Game::removePiece(Piece* piece) {
	piece->updatePos();
	sendb.pushHead(Com::Code::kill);
	sendb.push(inversePieceId(piece));
}

void Game::updateFortress(Tile* fort, bool breached) {
	fort->setBreached(breached);
	sendb.pushHead(Com::Code::breach);
	sendb.push(uint8(breached));
	sendb.push(inverseTileId(fort));
}

string Game::pastRecordAction() const {
	string msg = "You've already ";
	if (ownRec.action & ACT_MOVE)
		return msg + "moved";
	if (ownRec.action & ACT_SWAP)
		return msg + "switched";
	if (ownRec.action & ACT_ATCK)
		return msg + "attacked";
	if (ownRec.action & ACT_FIRE)
		return msg + "fired";
	return msg + "acted";
}

std::default_random_engine Game::createRandomEngine() {
	std::default_random_engine randGen;
	try {
		std::random_device rd;
		randGen.seed(rd());
	} catch (...) {
		randGen.seed(std::random_device::result_type(time(nullptr)));
		std::cerr << "failed to use random_device" << std::endl;
	}
	return randGen;
}
#ifdef DEBUG
vector<Object*> Game::initDummyObjects(const Com::Config& cfg) {
	vector<Object*> objs = initObjects(cfg);

	uint8 t = 0;
	vector<uint16> amts(config.tileAmounts.begin(), config.tileAmounts.end());
	for (uint16 x = 0; x < config.homeSize.x; x++)
		for (uint16 y = config.homeSize.y + 1; y < boardHeight; y++) {
			if (outRange(svec2(x, y), svec2(1, config.homeSize.y + 1), svec2(config.homeSize.x - 2, boardHeight - 2)) || !amts[uint8(Com::Tile::fortress)]) {
				for (; !amts[t]; t++);
				tiles[y * config.homeSize.x + x].setType(Com::Tile(t));
				amts[t]--;
			} else
				amts.back()--;
		}

	t = 0;
	amts.assign(config.middleAmounts.begin(), config.middleAmounts.end());
	for (uint16 i = 0; t < amts.size(); i++) {
		for (; t < amts.size() && !amts[t]; t++);
		if (t < amts.size()) {
			tiles.mid(i)->setType(Com::Tile(t));
			amts[t]--;
		}
	}

	for (uint16 i = 0; i < pieces.getNum(); i++)
		pieces.own(i)->setPos(gtop(svec2(i % config.homeSize.x, config.homeSize.y + 1 + i / config.homeSize.x)));
	return objs;
}
#endif
