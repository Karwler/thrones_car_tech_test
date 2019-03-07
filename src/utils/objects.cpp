#include "engine/world.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// CAMERA

Camera::Camera(const vec3& pos, const vec3& lat, double fov, double znear, double zfar) :
	pos(pos),
	lat(lat),
	fov(fov),
	znear(znear),
	zfar(zfar)
{}

void Camera::update() const {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fov, vec2d(World::winSys()->windowSize()).ratio(), znear, zfar);
	gluLookAt(double(pos.x), double(pos.y), double(pos.z), double(lat.x), double(lat.y), double(lat.z), 0.0, 1.0, 0.0);
	glMatrixMode(GL_MODELVIEW);
}

void Camera::updateUI() {
	vec2d res = World::winSys()->windowSize();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, res.x, res.y, 0.0, 0.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

vec3 Camera::direction(const vec2i& mPos) const {
	vec2 res = World::winSys()->windowSize().glm();
	vec2 mouse = mPos.glm() / (res / 2.f) - 1.f;

	mat4 proj = glm::perspective(glm::radians(float(fov)), res.x / res.y, float(znear), float(zfar));
	mat4 view = glm::lookAt(pos, lat, vec3(0.f, 1.f, 0.f));
	return glm::normalize(glm::inverse(proj * view) * vec4(mouse.x, -mouse.y, 1.f, 1.f));
}

// VERTEX

Vertex::Vertex(const vec3& pos, const vec2& tuv) :
	pos(pos),
	tuv(tuv)
{}

// OBJECT

Object::Object(const vec3& pos, const vec3& rot, const vec3& scl, const vector<Vertex>& verts, const vector<ushort>& elems, const Texture* tex, SDL_Color color, Info mode) :
	pos(pos),
	rot(rot),
	scl(scl),
	verts(verts),
	elems(elems),
	tex(tex),
	color(color),
	mode(mode)
{}

void Object::draw() const {
	if (!(mode & INFO_SHOW))
		return;

	if (tex && (mode & INFO_TEXTURE)) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, tex->getID());
	} else
		glDisable(GL_TEXTURE_2D);
	glColor4ubv(reinterpret_cast<const GLubyte*>(&color));
	
	setTransform();
	glBegin(!(mode & INFO_WIREFRAME) ? GL_TRIANGLES : GL_LINE_STRIP);
	for (ushort id : elems) {
		glTexCoord2fv(glm::value_ptr(verts[id].tuv));
		glVertex3fv(glm::value_ptr(verts[id].pos));
	}
	glEnd();
}

mat4 Object::getTransform() const {
	mat4 trans = glm::translate(mat4(1.f), pos);
	trans = glm::rotate(trans, rot.x, vec3(1.f, 0.f, 0.f));
	trans = glm::rotate(trans, rot.y, vec3(0.f, 1.f, 0.f));
	trans = glm::rotate(trans, rot.z, vec3(0.f, 0.f, 1.f));
	return glm::scale(trans, scl);
}

void Object::setTransform() const {
	glLoadIdentity();
	glTranslatef(pos.x, pos.y, pos.z);
	glRotatef(rot.x, 1.f, 0.f, 0.f);
	glRotatef(rot.y, 0.f, 1.f, 0.f);
	glRotatef(rot.z, 0.f, 0.f, 1.f);
	glScalef(scl.x, scl.y, scl.z);
}

// GAME OBJECT

const vector<ushort> BoardObject::squareElements = {
	0, 1, 2,
	2, 3, 0
};

const vector<Vertex> BoardObject::squareVertices = {
	Vertex(vec3(-0.5f, 0.f, 0.5f), vec2(0.f, 1.f)),
	Vertex(vec3(-0.5f, 0.f, -0.5f), vec2(0.f, 0.f)),
	Vertex(vec3(0.5f, 0.f, -0.5f), vec2(1.f, 0.f)),
	Vertex(vec3(0.5f, 0.f, 0.5f), vec2(1.f, 1.f))
};

BoardObject::BoardObject(vec2b pos, float poz, const Texture* tex, SDL_Color color, Info mode) :
	Object(btop(pos, poz), vec3(0.f), vec3(1.f), squareVertices, squareElements, tex, color, mode)
{}

// TILE

const array<SDL_Color, Tile::colors.size()> Tile::colors = {
	SDL_Color({60, 60, 60, 255}),
	SDL_Color({40, 255, 40, 255}),
	SDL_Color({0, 160, 0, 255}),
	SDL_Color({120, 120, 120, 255}),
	SDL_Color({40, 200, 255, 255}),
	SDL_Color({140, 70, 20, 255})
};

Tile::Tile(vec2b pos, float poz, Type type) :
	BoardObject(pos, poz, nullptr, colors[uint8(type)], (type != Type::empty ? INFO_FILL : INFO_LINES) | INFO_RAYCAST),
	type(type)
{}

void Tile::setType(Type newType) {
	type = newType;
	color = colors[uint8(type)];
	mode = type != Type::empty ? INFO_FILL : INFO_LINES;
}

// PIECE

const array<string, Piece::names.size()> Piece::names = {
	"ranger",
	"spearman",
	"crossbowman",
	"catapult",
	"trebuchet",
	"lancer",
	"warhorse",
	"elephant",
	"dragon",
	"throne"
};

Piece::Piece(vec2b pos, Type type) :
	BoardObject(pos, 0.01f, World::winSys()->texture(names[uint8(type)])),
	type(type)
{}
