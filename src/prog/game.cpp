#include "engine/world.h"
#include <ctime>

// GAME

Game::Game() :
	board(World::scene()),
	randGen(createRandomEngine()),
	randDist(0, Com::Config::randomLimit - 1)
{}

void Game::finishSetup() {
	firstTurn = true;
	ownRec = eneRec = Record();
	std::fill(favorsCount.begin(), favorsCount.end(), 0);
	std::fill(favorsLeft.begin(), favorsLeft.end(), board.config.favorLimit);
	availableFF = board.countAvailableFavors();
}

void Game::finishFavor(Favor next, Favor previous) {
	if (previous != Favor::none) {
		if (lastFavorUsed) {
			favorsCount[uint8(previous)]--;
			anyFavorUsed = true;
			lastFavorUsed = false;
		}
		ProgMatch* pm = World::state<ProgMatch>();
		if (pm->updateFavorIcon(previous, true); next == Favor::none)
			pm->selectFavorIcon(next);
	}
}

void Game::setNoEngage(Piece* piece) {
	ownRec.addProtect(piece, true);
	lastFavorUsed = true;
	finishFavor(Favor::none, Favor::conspire);
	board.setFavorInteracts(Favor::none, ownRec);
}

void Game::pieceMove(Piece* piece, svec2 dst, Piece* occupant, bool move) {
	ProgMatch* pm = World::state<ProgMatch>();
	svec2 pos = board.ptog(piece->getPos());
	Tile* stil = board.getTile(pos);
	Tile* dtil = board.getTile(dst);
	Favor favor = pm->favorIconSelect();
	Action action = move ? occupant ? ACT_SWAP : ACT_MOVE : ACT_ATCK;
	if (pos == dst)
		throw string();
	if (eneRec.info == Record::battleFail && action != ACT_MOVE)
		throw string("Only moving is allowed");

	checkActionRecord(piece, occupant, action, favor);
	switch (action) {
	case ACT_MOVE:
		if (!board.collectMoveTiles(piece, eneRec, favor).count(board.posToId(dst)))
			throw string("Can't move there");
		placePiece(piece, dst);
		break;
	case ACT_SWAP:
		if (ownRec.actors.count(occupant) || ownRec.assault.count(occupant))
			throw string("Can't switch with that piece");
		if (board.isEnemyPiece(occupant) && piece->getType() != Com::Piece::warhorse && favor != Favor::deceive)
			throw string("Piece can't switch with an enemy");
		if (favor != Favor::assault && favor != Favor::deceive && piece->getType() == Com::Piece::warhorse && board.isEnemyPiece(occupant)) {
			if (occupant->getType() == Com::Piece::spearmen)
				throw firstUpper(Com::pieceNames[uint8(piece->getType())]) + " can't switch with an enemy " + Com::pieceNames[uint8(occupant->getType())];
			if (dtil->getType() == Com::Tile::water)
				throw firstUpper(Com::pieceNames[uint8(piece->getType())]) + " can't switch onto " + Com::tileNames[uint8(dtil->getType())];
			if (dtil->isUnbreachedFortress())
				throw firstUpper(Com::pieceNames[uint8(piece->getType())]) + " can't switch onto a not breached " + Com::tileNames[uint8(dtil->getType())];
		}
		if (!board.collectMoveTiles(piece, eneRec, favor, true).count(board.posToId(dst)))
			throw string("Can't move there");
		placePiece(occupant, pos);
		placePiece(piece, dst);
		break;
	case ACT_ATCK:
		checkKiller(piece, occupant, dtil, true);
		if (piece->getType() != Com::Piece::throne) {
			if (stil->getType() == Com::Tile::mountain && piece->getType() != Com::Piece::rangers && piece->getType() != Com::Piece::dragon)
				throw firstUpper(Com::pieceNames[uint8(piece->getType())]) + " can't attack from a " + Com::tileNames[uint8(stil->getType())];
			if (dtil->getType() == Com::Tile::forest && stil->getType() != Com::Tile::forest && piece->getType() >= Com::Piece::lancer && piece->getType() <= Com::Piece::elephant)
				throw firstUpper(Com::pieceNames[uint8(piece->getType())]) + " must be on a " + Com::tileNames[uint8(dtil->getType())] + " to attack onto one";
			if (dtil->getType() == Com::Tile::forest && piece->getType() == Com::Piece::dragon)
				throw firstUpper(Com::pieceNames[uint8(piece->getType())]) + " can't attack onto a " + Com::tileNames[uint8(dtil->getType())];
			if (dtil->getType() == Com::Tile::water && piece->getType() != Com::Piece::spearmen && piece->getType() != Com::Piece::dragon)
				throw firstUpper(Com::pieceNames[uint8(piece->getType())]) + " can't attack onto " + Com::tileNames[uint8(dtil->getType())];
		}
		if (!board.collectEngageTiles(piece).count(board.posToId(dst)))
			throw string("Can't move there");
		doEngage(piece, pos, dst, occupant, dtil, action);
	}
	if (board.getPxpad()->show)
		changeTile(stil, Com::Tile::plains);
	if (concludeAction(piece, action, favor) && availableFF)
		World::scene()->setPopup(pm->createPopupFavorPick(availableFF));
	World::play("move");
}

void Game::pieceFire(Piece* killer, svec2 dst, Piece* victim) {
	svec2 pos = board.ptog(killer->getPos());
	Tile* stil = board.getTile(pos);
	Tile* dtil = board.getTile(dst);
	Favor favor = World::state<ProgMatch>()->favorIconSelect();
	if (pos == dst)
		throw string();
	if (eneRec.info == Record::battleFail)
		throw string("Only moving is allowed");

	checkActionRecord(killer, victim, ACT_FIRE, favor);
	checkKiller(killer, victim, dtil, false);
	if (stil->getType() == Com::Tile::forest || stil->getType() == Com::Tile::water)
		throw "Can't fire from " + string(stil->getType() == Com::Tile::forest ? "a " : "") + Com::tileNames[uint8(stil->getType())];
	if (dtil->getType() == Com::Tile::forest && killer->getType() != Com::Piece::trebuchet)
		throw firstUpper(Com::pieceNames[uint8(killer->getType())]) + " can't fire at a " + Com::tileNames[uint8(dtil->getType())];
	if (dtil->getType() == Com::Tile::mountain)
		throw string("Can't fire at a ") + Com::tileNames[uint8(dtil->getType())];

	if (!board.collectEngageTiles(killer).count(board.posToId(dst)))
		throw string("Can't fire there");
	for (svec2 m = deltaSingle(ivec2(dst) - ivec2(pos)), i = pos + m; i != dst; i += m)
		if (Com::Tile type = board.getTile(i)->getType(); type == Com::Tile::mountain)
			throw string("Can't fire over ") + Com::tileNames[uint8(type)] + 's';

	doEngage(killer, pos, dst, victim, dtil, ACT_FIRE);
	concludeAction(killer, ACT_FIRE, favor);
	World::play("ammo");
}

bool Game::concludeAction(Piece* piece, Action action, Favor favor) {
	if (favor == Favor::none)
		ownRec.update(piece, action);
	else if (lastFavorUsed = true; favor == Favor::assault) {
		if (ownRec.update(piece, action, false); (ownRec.assault[piece] & ACT_MS) == ACT_MS)
			finishFavor(Favor::none, favor);
	} else {
		if (favor == Favor::hasten)
			ownRec.update(piece, ACT_NONE);
		finishFavor(Favor::none, favor);
	}
	if (checkWin())
		return false;

	bool noFavors = std::none_of(favorsCount.begin(), favorsCount.end(), [](uint8 cnt) -> bool { return cnt; });
	if (Action termin = ownRec.actionsExhausted(); (termin != ACT_NONE && noFavors) || (termin & (ACT_AF | ACT_SPAWN)) || eneRec.info == Record::battleFail)
		endTurn();
	else {
		ProgMatch* pm = World::state<ProgMatch>();
		pm->updateSpawnIcon(false);
		pm->updateTurnIcon(true);
		pm->message->setText("Your turn");	// in case it got changed during the turn
		board.setFavorInteracts(pm->favorIconSelect(), ownRec);
		board.setPxpadPos(nullptr);
	}
	return true;
}

void Game::placeDragon(Piece* dragon, svec2 pos, Piece* occupant) {
	if (Tile* til = board.getTile(pos); board.isHomeTile(til) && til->getType() == Com::Tile::fortress) {
		dragon->setInteractivity(dragon->show, false, &Program::eventPieceStart, &Program::eventMove, &Program::eventEngage);
		World::state<ProgMatch>()->decreaseDragonIcon();
		if (occupant)
			removePiece(occupant);
		placePiece(dragon, pos);
		checkWin();
	} else {
		dragon->cancelDrag();
		dragon->show = false;
		World::state<ProgMatch>()->setDragonIcon(true);	// reset selected
	}
}

void Game::rebuildTile(Piece* throne, bool reinit) {
	if (reinit)
		board.restorePiecesInteract(ownRec);
	breachTile(board.getTile(board.ptog(throne->getPos())), false);
}

void Game::establishTile(Piece* throne, bool reinit) {
	changeTile(board.establishTile(throne, reinit, ownRec, World::state<ProgMatch>()), board.ownCity ? Com::Tile::city : Com::Tile::farm);
}

void Game::spawnPiece(Com::Piece type, Tile* tile, bool reinit) {
	if (Piece* pce = board.findSpawnablePiece(type, tile, reinit, ownRec)) {
		placePiece(pce, board.idToPos(board.tileId(tile)));
		concludeAction(pce, ACT_SPAWN, Favor::none);
	}
}

void Game::checkActionRecord(Piece* piece, Piece* occupant, Action action, Favor favor) {
	if (favor == Favor::assault) {
		if (!(action & ACT_MS))
			throw firstUpper(favorNames[uint8(favor)]) + " is limited to moving and switching";
		if (ownRec.actors.count(occupant))
			throw string("Can't switch with that piece");
		if (umap<Piece*, Action>::iterator it = ownRec.assault.find(piece); it != ownRec.assault.end() && (it->second & ~(action == ACT_MOVE ? ACT_SWAP : ACT_MOVE)))
			throw "Piece can't " + string(action == ACT_MOVE ? "move" : "switch");
	} else {
		if (favor == Favor::none && action == ACT_MOVE && eneRec.info != Record::battleFail)
			if (umap<Piece*, Action>::iterator it = ownRec.actors.find(piece); ownRec.actors.size() >= 2 || (ownRec.actors.size() == 1 && (it != ownRec.actors.end() ? it->second & ~ACT_SWAP : ownRec.actors.begin()->second & ~ACT_MOVE)))
				throw string("Piece can't move");
		if (favor == Favor::none && action == ACT_SWAP)
			if (umap<Piece*, Action>::iterator it = ownRec.actors.find(piece); it != ownRec.actors.end() ? (it->first->getType() != Com::Piece::warhorse && (it->second & ~ACT_MOVE)) || ownRec.actors.size() >= 2 : !ownRec.actors.empty())
				throw string("Piece can't switch");
		if (favor == Favor::hasten && action != ACT_MOVE)
			throw firstUpper(favorNames[uint8(favor)]) + " is limited to moving";
		if (favor == Favor::deceive && (action != ACT_SWAP || board.isOwnPiece(occupant)))
			throw firstUpper(favorNames[uint8(favor)]) + " is limited to switching enemy pieces";
		if (action == ACT_SWAP && ownRec.assault.count(occupant))
			throw string("Can't switch with an ") + favorNames[uint8(Favor::assault)] + "piece";
	}
}

void Game::checkKiller(Piece* killer, Piece* victim, Tile* dtil, bool attack) {
	string action = attack ? "attack" : "fire";
	if (firstTurn)
		throw "Can't " + action + " on first turn";
	if (anyFavorUsed)
		throw "Can't " + action + " after a fate's favor";
	if (eneRec.protects.count(killer))
		throw "Piece can't " + action + " during this turn";
	if (!ownRec.actors.empty())
		throw "Piece can't " + action;

	if (victim) {
		umap<Piece*, bool>::iterator protect = eneRec.protects.find(victim);
		if (protect != eneRec.protects.end() && (protect->second || killer->getType() != Com::Piece::throne))
			throw string("Piece is protected during this turn");
		if (victim->getType() == Com::Piece::elephant && dtil->getType() == Com::Tile::plains && killer->getType() != Com::Piece::dragon && killer->getType() != Com::Piece::throne)
			throw firstUpper(Com::pieceNames[uint8(killer->getType())]) + " can't attack an " + Com::pieceNames[uint8(victim->getType())] + " on " + Com::tileNames[uint8(dtil->getType())];
	} else if (board.config.gameType != Com::Config::GameType::homefront || (dtil->getType() != Com::Tile::fortress && dtil != board.eneFarm) || dtil->getBreached())
		throw "Can't " + (attack ? action : action + " at") + " nothing";
}

void Game::doEngage(Piece* killer, svec2 pos, svec2 dst, Piece* victim, Tile* dtil, Action action) {
	if (killer->getType() == Com::Piece::warhorse)
		ownRec.addProtect(killer, false);

	if (dtil->isUnbreachedFortress() && killer->getType() != Com::Piece::throne) {
		if (killer->getType() == Com::Piece::dragon)
			if (dst -= deltaSingle(ivec2(dst) - ivec2(pos)); dst != pos && board.findOccupant(dst))
				throw string("No space beside ") + Com::tileNames[uint8(Com::Tile::fortress)];

		if ((!victim || board.isEnemyPiece(victim)) && randDist(randGen) >= board.config.battlePass) {
			if (eneRec.addProtect(killer, false); action == ACT_ATCK) {
				ownRec.info = Record::battleFail;
				ownRec.lastAct = pair(killer, ACT_ATCK);
				endTurn();
			}
			throw string("Battle lost");
		}
		if (breachTile(dtil); killer->getType() == Com::Piece::dragon)
			placePiece(killer, dst);
	} else {
		if ((dtil == board.eneFarm || dtil == board.ownFarm) && !dtil->getBreached())
			breachTile(dtil);
		if (victim)
			removePiece(victim);
		if (action == ACT_ATCK)
			placePiece(killer, dst);
	}
}

bool Game::checkWin() {
	if (board.checkThroneWin(board.getPieces().own(), board.ownPieceAmts)) {	// no need to check if enemy throne has occupied own fortress
		doWin(false);
		return true;
	}
	if (board.checkThroneWin(board.getPieces().ene(), board.enePieceAmts) || board.checkFortressWin()) {
		doWin(true);
		return true;
	}
	return false;
}

void Game::doWin(bool win) {
	ownRec.info = win ? Record::win : Record::loose;
	endTurn();
	World::program()->finishMatch(win);
}

void Game::surrender() {
	sendb.pushHead(Com::Code::record, Com::dataHeadSize + uint16(sizeof(uint8) + sizeof(uint16) * 2));
	sendb.push(uint8(Record::loose));
	sendb.push({ uint16(UINT16_MAX), uint16(0) });
	World::netcp()->sendData(sendb);
	World::program()->finishMatch(false);
}

void Game::prepareTurn(bool fcont) {
	bool xmov = eneRec.info == Record::battleFail;	// should only occur when myTurn is true
	Board::setTilesInteract(board.getTiles().begin(), board.getTiles().getSize(), Tile::Interact(myTurn));
	board.prepareTurn(myTurn, xmov, fcont, ownRec, eneRec);
	if (myTurn && !fcont)
		anyFavorUsed = lastFavorUsed = false;

	ProgMatch* pm = World::state<ProgMatch>();
	pm->message->setText(myTurn ? xmov ? "Opponent lost a battle" : "Your turn" : "Opponent's turn");
	for (uint8 i = 0; i < favorMax; i++)
		pm->updateFavorIcon(Favor(i), myTurn && !xmov);
	pm->updateRebuildIcon(myTurn && !xmov);
	pm->updateEstablishIcon(myTurn && !xmov);
	pm->updateSpawnIcon(myTurn && !xmov);
	pm->updateTurnIcon(xmov || fcont);
	pm->setDragonIcon(myTurn && !xmov);
}

void Game::endTurn() {
	sendb.pushHead(Com::Code::record, Com::dataHeadSize + uint16(sizeof(uint8) + sizeof(uint16) * (2 + ownRec.protects.size())));	// 2 for last actor and protects size
	sendb.push(uint8(ownRec.info | (eneRec.info & Record::battleFail)));
	sendb.push({ board.inversePieceId(ownRec.lastAct.first), uint16(ownRec.protects.size()) });
	for (auto& [pce, prt] : ownRec.protects)
		sendb.push(uint16((board.inversePieceId(pce) & 0x7FFF) | (uint16(prt) << 15)));
	World::netcp()->sendData(sendb);

	firstTurn = myTurn = false;
	board.setPxpadPos(nullptr);
	prepareTurn(false);
}

void Game::recvRecord(const uint8* data) {
	Record::Info info = Record::Info(*data);
	bool fcont = ownRec.info == Record::battleFail && info == Record::battleFail;
	if (fcont)
		ownRec.info = eneRec.info = Record::none;	// response to failed attack, meaning keep old records when continuing a turn but battleFail no longer needed
	else {
		uint16 ai = Com::read16(++data);
		uint16 ptCnt = Com::read16(data += sizeof(uint16));
		umap<Piece*, bool> protect(ptCnt);
		while (ptCnt--) {
			uint16 id = Com::read16(data += sizeof(uint16));
			protect.emplace(&board.getPieces()[id & 0x7FFF], id & 0x8000);
		}
		eneRec = Record(pair(ai < board.getPieces().getSize() ? &board.getPieces()[ai] : nullptr, ACT_NONE), std::move(protect), info);
		ownRec = Record();
	}

	if (eneRec.info == Record::win || eneRec.info == Record::loose)
		World::program()->finishMatch(eneRec.info == Record::loose);
	else {
		myTurn = true;
		prepareTurn(fcont);
		if (board.getPxpad()->show)
			World::scene()->setPopup(World::state()->createPopupChoice("Destroy forest at " + toStr(board.ptog(board.getPxpad()->getPos()), "|") + '?', &Program::eventKillDestroy, &Program::eventCancelDestroy));
	}
}

void Game::sendConfig(bool onJoin) {
	uint ofs;
	if (onJoin) {
		ofs = sendb.allocate(Com::Code::cnjoin, Com::dataHeadSize + 1 + Com::Config::dataSize);
		ofs = sendb.write(uint8(true), ofs);
	} else
		ofs = sendb.allocate(Com::Code::config);
	World::state<ProgRoom>()->confs[World::program()->curConfig].toComData(&sendb[ofs]);
	World::netcp()->sendData(sendb);
}

void Game::sendStart() {
	myTurn = uint8(std::uniform_int_distribution<uint>(0, 1)(randGen));
	uint ofs = sendb.allocate(Com::Code::start);
	ofs = sendb.write(uint8(!myTurn), ofs);
	World::state<ProgRoom>()->confs[World::program()->curConfig].toComData(&sendb[ofs]);
	World::netcp()->sendData(sendb);
	World::program()->eventOpenSetup();
}

void Game::recvStart(const uint8* data) {
	myTurn = data[0];
	recvConfig(data + 1);
	World::program()->eventOpenSetup();
}

void Game::sendSetup() {
	uint tcnt = board.tileCompressionSize();
	uint ofs = sendb.allocate(Com::Code::setup, uint16(Com::dataHeadSize + tcnt + Com::pieceMax * sizeof(uint16) + board.getPieces().getNum() * sizeof(uint16)));
	std::fill_n(&sendb[ofs], tcnt, 0);
	for (uint16 i = 0; i < board.getTiles().getExtra(); i++)
		sendb[i/2+ofs] |= board.compressTile(i);
	ofs += tcnt;

	for (uint8 i = 0; i < Com::pieceMax; ofs = sendb.write(board.ownPieceAmts[i++], ofs));
	for (uint16 i = 0; i < board.getPieces().getNum(); i++)
		sendb.write(board.pieceOnBoard(board.getPieces().own(i)) ? board.invertId(board.posToId(board.ptog(board.getPieces().own(i)->getPos()))) : uint16(UINT16_MAX), i * sizeof(uint16) + ofs);
	World::netcp()->sendData(sendb);
}

void Game::recvConfig(const uint8* data) {
	board.config.fromComData(data);
	board.updateConfigValues();
}

void Game::recvSetup(const uint8* data) {
	// set tiles and pieces
	for (uint16 i = 0; i < board.getTiles().getHome(); i++)
		board.getTiles()[i].setType(board.decompressTile(data, i));
	for (uint16 i = 0; i < board.config.homeSize.x; i++)
		World::state<ProgSetup>()->rcvMidBuffer[i] = board.decompressTile(data, board.getTiles().getHome() + i);

	uint16 ofs = board.tileCompressionSize();
	for (uint8 i = 0; i < Com::pieceMax; i++, ofs += sizeof(uint16))
		board.enePieceAmts[i] = Com::read16(data + ofs);
	uint8 t = 0;
	for (uint16 i = 0, c = 0; i < board.getPieces().getNum(); i++, c++) {
		uint16 id = Com::read16(data + i * sizeof(uint16) + ofs);
		for (; c >= board.enePieceAmts[t]; t++, c = 0);
		board.getPieces().ene(i)->setType(Com::Piece(t));
		board.getPieces().ene(i)->setPos(board.gtop(id < board.getTiles().getHome() ? board.idToPos(id) : svec2(UINT16_MAX)));
	}

	// document thrones' fortresses
	for (Piece* throne = board.getPieces(board.getPieces().ene(), board.enePieceAmts, Com::Piece::throne); throne != board.getPieces().end(); throne++)
		if (uint16 pos = board.posToId(board.ptog(throne->getPos())); board.getTiles()[pos].getType() == Com::Tile::fortress)
			throne->lastFortress = pos;

	// finish up
	if (ProgSetup* ps = World::state<ProgSetup>(); ps->getStage() == ProgSetup::Stage::ready)
		World::program()->eventOpenMatch();
	else {
		ps->enemyReady = true;
		ps->message->setText(ps->getStage() == ProgSetup::Stage::pieces ? "Opponent ready" : "Hurry the fuck up!");
	}
}

void Game::recvMove(const uint8* data) {
	Piece& pce = board.getPieces()[Com::read16(data)];
	uint16 pos = Com::read16(data + sizeof(uint16));
	if (pce.updatePos(board.idToPos(pos)); pce.getType() == Com::Piece::throne && board.getTiles()[pos].getType() == Com::Tile::fortress)
		pce.lastFortress = pos;
}

void Game::placePiece(Piece* piece, svec2 pos) {
	if (uint16 fid = board.posToId(pos); piece->getType() == Com::Piece::throne && board.getTiles()[fid].getType() == Com::Tile::fortress && piece->lastFortress != fid)
		if (piece->lastFortress = fid; availableFF < std::accumulate(favorsLeft.begin(), favorsLeft.end(), uint16(0)))
			availableFF++;

	piece->updatePos(pos, true);
	sendb.pushHead(Com::Code::move);
	sendb.push({ board.inversePieceId(piece), board.invertId(board.posToId(pos)) });
	World::netcp()->sendData(sendb);
}

void Game::recvKill(const uint8* data) {
	Piece* pce = &board.getPieces()[Com::read16(data)];
	if (board.isOwnPiece(pce))
		board.setPxpadPos(pce);
	pce->updatePos();
}

void Game::removePiece(Piece* piece) {
	piece->updatePos();
	sendb.pushHead(Com::Code::kill);
	sendb.push(board.inversePieceId(piece));
	World::netcp()->sendData(sendb);
}

void Game::breachTile(Tile* tile, bool yes) {
	tile->setBreached(true);
	sendb.pushHead(Com::Code::breach);
	sendb.push(board.inverseTileId(tile));
	sendb.push(uint8(yes));
	World::netcp()->sendData(sendb);
}

void Game::recvTile(const uint8* data) {
	uint16 pos = Com::read16(data);
	if (Com::Tile type = Com::Tile(data[sizeof(uint16)]); type == Com::Tile::farm || type == Com::Tile::city) {
		board.getTiles()[pos].setType(board.getTiles()[pos].getType(), type);
		(type == Com::Tile::farm ? board.eneFarm : board.eneCity) = &board.getTiles()[pos];
	} else if (board.getTiles()[pos].setType(type); type == Com::Tile::fortress)
		if (Piece* pce = board.findPiece(board.getPieces().begin(), board.getPieces().end(), board.idToPos(pos)); pce && pce->getType() == Com::Piece::throne)
			pce->lastFortress = pos;
}

void Game::changeTile(Tile* tile, Com::Tile type) {
	if (type != Com::Tile::farm && type != Com::Tile::city)
		tile->setType(type);
	else
		tile->setType(tile->getType(), type);
	sendb.pushHead(Com::Code::tile);
	sendb.push(board.inverseTileId(tile));
	sendb.push(uint8(type));
	World::netcp()->sendData(sendb);
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
