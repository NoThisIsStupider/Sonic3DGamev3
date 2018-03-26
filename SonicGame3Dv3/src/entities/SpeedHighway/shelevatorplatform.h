#ifndef SHELEVATORPLATFORM_H
#define SHELEVATORPLATFORM_H

class TexturedModel;
class Source;
class SoundEmitter;

#include <list>
#include "../entities.h"
#include "../collideableobject.h"

class SH_ElevatorPlatform : public CollideableObject
{
private:
	static std::list<TexturedModel*> models;
	static CollisionModel* cmOriginal;

	CollisionModel* collideModelTransformed2;

	float timeOffset;
	float speed;

	int pointIDs[4];
	Vector3f pointPos[4];
	Vector3f pointDifferences[4];
	float pointLengths[4];

	Vector3f oldPos;

	SoundEmitter* elevatorPlatSoundEmitter = nullptr;
	Source* elevatorPlatSource = nullptr;

public:
	SH_ElevatorPlatform();
	SH_ElevatorPlatform(float x, float y, float z, float rotY, float speed, int point1ID, int point2ID, int point3ID, int point4ID, float timeOffset, int soundEmitterID);

	void step();

	std::list<TexturedModel*>* getModels();

	static void loadStaticModels();

	static void deleteStaticModels();
};
#endif
