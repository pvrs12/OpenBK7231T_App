#ifdef WINDOWS
#include "Simulation.h"
#include "Shape.h"
#include "Object.h"
#include "Wire.h"
#include "Junction.h"
#include "Simulator.h"
#include "PrefabManager.h"


void CSimulation::removeJunctions(class CShape *s) {
	CJunction *j = dynamic_cast<CJunction*>(s);
	if (j != 0) {
		removeJunction(j);
	}
	CWire *w = dynamic_cast<CWire*>(s);
	if (w) {
		for (int i = 0; i < w->getJunctionsCount(); i++) {
			removeJunction(w->getJunction(i));
		}
	}
	for (int i = 0; i < s->getShapesCount(); i++) {
		CShape *s2 = s->getShape(i);
		removeJunctions(s2);
	}
}
void CSimulation::removeJunction(class CJunction *ju) {
	junctions.remove(ju);
}
int CSimulation::drawTextStats(int h) {
	h = drawText(10, h, "Objects %i, wires %i", objects.size(), wires.size());
	return h;
}
void CSimulation::recalcBounds() {
	for (int i = 0; i < objects.size(); i++) {
		objects[i]->recalcBoundsAll();
	}
}
void CSimulation::drawSim() {
	for (int i = 0; i < objects.size(); i++) {
		objects[i]->drawWithChildren(0);
	}
	for (int i = 0; i < wires.size(); i++) {
		wires[i]->drawWire();
	}
}
class CShape *CSimulation::findShapeByBoundsPoint(const class Coord &p) {
	for (int i = 0; i < objects.size(); i++) {
		if (objects[i]->hasWorldPointInside(p))
			return objects[i];
	}
	for (int i = 0; i < wires.size(); i++) {
		CShape *r = wires[i]->findSubPart(p);
		if (r)
			return r;
	}
	return 0;
}
class CWire *CSimulation::addWire(const class Coord &a, const class Coord &b) {
	if (a.dist(b) < 5) {
		return 0;
	}
	class CWire *cw = new CWire(a, b);
	wires.push_back(cw);
	matchJunction(cw);
	registerJunctions(cw);
	return cw;
}
bool CSimulation::isJunctionOnList(class CJunction *ju) {
	for (int i = 0; i < junctions.size(); i++) {
		if (junctions[i] == ju)
			return true;
	}
	return false;
}
void CSimulation::registerJunction(class CJunction *ju) {
	junctions.push_back(ju);
}
void CSimulation::registerJunctions(class CWire *w) {
	for (int i = 0; i < w->getJunctionsCount(); i++) {
		registerJunction(w->getJunction(i));
	}
}
void CSimulation::registerJunctions(class CShape *s) {
	if (s == 0) {
		return;
	}
	CJunction *j = dynamic_cast<CJunction*>(s);
	if (j != 0) {
		registerJunction(j);
	}
	for (int i = 0; i < s->getShapesCount(); i++) {
		CShape *s2 = s->getShape(i);
		registerJunctions(s2);
	}
}
CObject * CSimulation::addObject(CObject *o) {
	if (o == 0) {
		return 0;
	}
	objects.push_back(o);
	registerJunctions(o);
	return o;
}
void CSimulation::createDemo() {
	CObject *wb3s = addObject(sim->getPfbs()->instantiatePrefab("WB3S"));
	wb3s->setPosition(300, 200);
	addObject(sim->getPfbs()->instantiatePrefab("Button"))->setPosition(500, 260)->rotateDegreesAroundSelf(180);
	addObject(sim->getPfbs()->instantiatePrefab("Test"))->setPosition(500, 400);
	addObject(sim->getPfbs()->instantiatePrefab("GND"))->setPosition(600, 420);
	addWire(Coord(460, 260), Coord(380, 260));
	addWire(Coord(540, 260), Coord(600, 260));
	addWire(Coord(600, 400), Coord(600, 260));


	addWire(Coord(500, 220), Coord(380, 220));
	addWire(Coord(500, 220), Coord(500, 160));
	addObject(sim->getPfbs()->instantiatePrefab("Bulb"))->setPosition(500, 140)->rotateDegreesAroundSelf(90);
	addWire(Coord(500, 60), Coord(500, 120));
	addObject(sim->getPfbs()->instantiatePrefab("VDD"))->setPosition(500, 60);

	addWire(Coord(560, 240), Coord(380, 240));
	addObject(sim->getPfbs()->instantiatePrefab("Button"))->setPosition(600, 240)->rotateDegreesAroundSelf(180);
	addWire(Coord(640, 240), Coord(700, 240));
	addWire(Coord(700, 400), Coord(700, 240));
	addObject(sim->getPfbs()->instantiatePrefab("GND"))->setPosition(700, 420);
	//addObject(new CObject(new CCircle(800, 500, 100)));
	CObject *bulb2 = addObject(sim->getPfbs()->instantiatePrefab("Bulb"));
	bulb2->setPosition(440, 140)->rotateDegreesAroundSelf(90);

	addObject(sim->getPfbs()->instantiatePrefab("LED_CW"))->setPosition(560, 140)->rotateDegreesAroundSelf(90);
	addObject(sim->getPfbs()->instantiatePrefab("LED_RGB"))->setPosition(760, 140)->rotateDegreesAroundSelf(90);
	
	addWire(Coord(440, 60), Coord(440, 120));
	addWire(Coord(440, 200), Coord(440, 160));
	addWire(Coord(440, 200), Coord(380, 200));
	addObject(sim->getPfbs()->instantiatePrefab("VDD"))->setPosition(440, 60);

	CObject *bulb2_copy = bulb2->cloneObject();
	bulb2_copy->setPosition(640, 140);
	addObject(bulb2_copy);

	if (0) {
		CObject *wb3s_copy = wb3s->cloneObject();
		wb3s_copy->setPosition(640, 440);
		addObject(wb3s_copy);
	}

	matchAllJunctions();
	recalcBounds();
}
CShape *CObject::cloneShape() {
	CObject *no = new CObject();
	this->cloneShapeTo(no);
	return no;
}
class CObject *CObject::cloneObject() {
	CShape *sh = cloneShape();
	return dynamic_cast<CObject*>(sh);
}
void CSimulation::matchAllJunctions() {
	for (int i = 0; i < wires.size(); i++) {
		CWire *w = wires[i];
		matchJunction(w);
	}
}


void CSimulation::matchJunctionsOf_r(class CShape *s) {
	for (int j = 0; j < s->getShapesCount(); j++) {
		CShape *ch = s->getShape(j);
		if (ch->isJunction()) {
			// this could be done much better, dont rematch all, just add new
			CJunction *jun = dynamic_cast<CJunction*>(ch);
			matchJunction(jun);
		}
		else {
			matchJunctionsOf_r(ch);
		}
	}
}
void CSimulation::matchJunction(class CWire *w) {
	for (int j = 0; j < w->getJunctionsCount(); j++) {
		CJunction *oj = w->getJunction(j);
		matchJunction(oj);
	}
}
void CSimulation::tryMatchJunction(class CJunction *jn, class CJunction *oj) {
	if (oj == 0)
		return;
	if (jn == 0)
		return;
	if (oj == jn)
		return;
	// cannot connect two objects directly without wire
	// (in Eagle, such connection will automatically create wire, but let's keep it simple)
	if (oj->isWireJunction() == false && jn->isWireJunction() == false)
		return;
	if (oj->absPositionDistance(jn) < 1) {
		jn->addLink(oj);
		oj->addLink(jn);
	}
}
void CSimulation::matchJunction_r(class CShape *sh, class CJunction *jn) {
	for (int i = 0; i < sh->getShapesCount(); i++) {
		CShape *s = sh->getShape(i);
		tryMatchJunction(jn, dynamic_cast<CJunction*>(s));
	}
}

void CSimulation::matchJunction(class CJunction *jn) {
	jn->clearLinks();
	for (int i = 0; i < wires.size(); i++) {
		CWire *w = wires[i];
		for (int j = 0; j < w->getJunctionsCount(); j++) {
			CJunction *oj = w->getJunction(j);
			tryMatchJunction(jn, oj);
		}
	}
	for (int i = 0; i < objects.size(); i++) {
		CShape *s = objects[i];
		matchJunction_r(s, jn);
	}
}
void CSimulation::destroyObject(CShape *s) {
	CJunction *j = 0;
	CEdge *ed = 0;
	CShape *p = s->getParent();
	CWire *w = dynamic_cast<CWire*>(p);
	if (w) {
		removeJunctions(w);
		j = dynamic_cast<CJunction*>(s);
		if (j) {
		}
		ed = dynamic_cast<CEdge*>(s);
		if (ed) {
			delete w;
			wires.remove(w);
		}
	}
	else {
		removeJunctions(s);
		CObject *o = dynamic_cast<CObject*>(s);
		if (o != 0) {
			delete o;
			objects.remove(o);
		}
	}

}

#endif

