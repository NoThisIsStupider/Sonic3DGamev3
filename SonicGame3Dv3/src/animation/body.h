#ifndef BODY_H
#define BODY_H

class Vector3f;
class TexturedModel;

#include <list>
#include <vector>
#include "../entities/entities.h"
#include "animation.h"


class Body : public Entity
{
private:
	float baseX;
	float baseY;
	float baseZ;
	float baseRotY;
	float baseRotZ;
	float baseRotX;
	float prevTime;

	std::list<TexturedModel*>* myModels;

public:
	int animationIndex;
	float time;
	float deltaTime;

	std::vector<Animation>* animations;

	Body(std::list<TexturedModel*>* models);

	void step();

	void update(float time);

	void setBaseOrientation(Vector3f* basePosition, float rotY, float rotZ);

	void setBaseRotZ(float rotZ);

	std::list<TexturedModel*>* getModels();
};
#endif