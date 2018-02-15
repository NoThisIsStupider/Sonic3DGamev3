#include <list>
#include <iostream>
#include <cmath>
#include <algorithm>

#include "entities.h"
#include "../models/models.h"
#include "../toolbox/vector.h"
#include "player.h"
#include "../renderEngine/renderEngine.h"
#include "../objLoader/objLoader.h"
#include "../engineTester/main.h"
#include "../toolbox/input.h"
#include "../toolbox/maths.h"
#include "camera.h"
#include "../collision/collisionchecker.h"
#include "../collision/triangle3d.h"
#include "../animation/body.h"
#include "../animation/limb.h"
#include "../animation/animationresources.h"
#include "skysphere.h"
#include "../entities/ring.h"
#include "maniasonicmodel.h"
#include "../audio/audioplayer.h"
#include "../audio/source.h"

std::list<TexturedModel*> Player::modelBody;
std::list<TexturedModel*> Player::modelHead;
std::list<TexturedModel*> Player::modelLeftHumerus;
std::list<TexturedModel*> Player::modelLeftForearm;
std::list<TexturedModel*> Player::modelLeftHand;
std::list<TexturedModel*> Player::modelLeftThigh;
std::list<TexturedModel*> Player::modelLeftShin;
std::list<TexturedModel*> Player::modelLeftFoot;
std::list<TexturedModel*> Player::modelRightHumerus;
std::list<TexturedModel*> Player::modelRightForearm;
std::list<TexturedModel*> Player::modelRightHand;
std::list<TexturedModel*> Player::modelRightThigh;
std::list<TexturedModel*> Player::modelRightShin;
std::list<TexturedModel*> Player::modelRightFoot;

ManiaSonicModel* Player::maniaSonic;

int Player::characterID = 6;

extern bool INPUT_JUMP;
extern bool INPUT_ACTION;
extern bool INPUT_ACTION2;
extern bool INPUT_SHOULDER;
extern bool INPUT_SELECT;
extern bool INPUT_SPECIAL;
extern bool INPUT_START;

extern bool INPUT_PREVIOUS_JUMP;
extern bool INPUT_PREVIOUS_ACTION;
extern bool INPUT_PREVIOUS_ACTION2;
extern bool INPUT_PREVIOUS_SHOULDER;
extern bool INPUT_PREVIOUS_SELECT;
extern bool INPUT_PREVIOUS_SPECIAL;
extern bool INPUT_PREVIOUS_START;

extern float INPUT_X;
extern float INPUT_Y;
extern float INPUT_X2;
extern float INPUT_Y2;
extern float INPUT_ZOOM;

Player::Player(float x, float y, float z)
{
	position.x = x;
	position.y = y;
	position.z = z;
	currNorm.x = 0;
	currNorm.y = 1;
	currNorm.z = 0;
	setVisible(false); //Our limbs are what will be visible
	Player::maniaSonic = nullptr;
	createLimbs();
}

void Player::step()
{
	previousPos.set(&position);
	count++;
	setMovementInputs();
	adjustCamera();
	checkSkid();

	iFrame = std::max(0, iFrame-1);
	hitTimer = std::max(0, hitTimer-1);
	canMoveTimer = std::max(-1, canMoveTimer - 1);

	if (deadTimer == 0)
	{
		Global::shouldRestartLevel = true;
	}
	deadTimer = std::max(-1, deadTimer - 1);

	if (canMoveTimer == 0)
	{
		canMove = true;
	}

	if (jumpInput)
	{
		hoverCount = std::max(hoverCount - 1, 0);
	}
	else
	{
		hoverCount = 0;
	}

	if (!onPlane) //in air
	{
		xVelGround = 0;
		zVelGround = 0;
		applyFrictionAir();
		moveMeAir();
		limitMovementSpeedAir();
		yVel -= gravity;
	}
	else //on ground
	{
		hitTimer = 0;

		xVelAir = 0;
		zVelAir = 0;
		applyFriction(frictionGround);
		moveMeGround();
		limitMovementSpeed(normalSpeedLimit);
	}

	if (isBall)
	{
		xVelGround += currNorm.x*slopeAccel*1.25f;
		zVelGround += currNorm.z*slopeAccel*1.25f;
	}
	else
	{
		xVelGround += currNorm.x*slopeAccel;
		zVelGround += currNorm.z*slopeAccel;
	}

	float inputX = xVelGround;
	float inputY = zVelGround;
	float mag = sqrtf(inputX*inputX + inputY*inputY);

	float inputDir = (atan2f(inputY, inputX));
	Vector3f negNorm;
	negNorm.set(-currNorm.x, -currNorm.y, -currNorm.z);
	Vector3f mapped = mapInputs3(inputDir, mag, &negNorm);
	float Ax = mapped.x;
	float Ay = mapped.y;
	float Az = mapped.z;

	if (onPlane) //On Ground
	{
		isJumping = false;
		isBouncing = false;
		isStomping = false;
		homingAttackTimer = -1;
		float speed = sqrtf(xVelGround*xVelGround + zVelGround*zVelGround);
		if (currNorm.y <= 0.3f && speed < 1) //Arbitrary constants
		{
			wallStickTimer--;
		}
		else
		{
			wallStickTimer = wallStickTimerMax;
		}

		xVel = Ax;
		yVel = Ay;
		zVel = Az;
		xVelAir = 0;
		zVelAir = 0;
		hoverCount = hoverLimit;

		if (wallStickTimer < 0)
		{
			//Do a small 'pop off' off the wall
			xVelAir = xVel + currNorm.x * 1.5f;
			zVelAir = zVel + currNorm.z * 1.5f;
			yVel = yVel + currNorm.y * 1.5f;
			xVel = 0;
			zVel = 0;
			xVelGround = 0;
			zVelGround = 0;
			//isJumping = true;
			onPlane = false;
			currNorm.set(0, 1, 0);
		}

		if (jumpInput && !previousJumpInput)
		{
			increasePosition(currNorm.x * 2, currNorm.y * 2, currNorm.z * 2);
			xVelAir = xVel + currNorm.x*jumpPower;
			zVelAir = zVel + currNorm.z*jumpPower;
			yVel = yVel + currNorm.y*jumpPower;
			xVel = 0;
			zVel = 0;
			xVelGround = 0;
			zVelGround = 0;
			isJumping = true;
			onPlane = false;

			AudioPlayer::play(12, getPosition());
		}

		if (speed < 0.45f)
		{
			if (isBall)
			{
				spindashReleaseTimer = spindashReleaseTimerMax;
				spindashRestartDelay = spindashRestartDelayMax;
			}
			isBall = false;
		}

		if (isBall == false && spindashRestartDelay == 0)
		{
			canStartSpindash = true;
		}
		else
		{
			canStartSpindash = false;
		}

		if (spindashRestartDelay > 0)
		{
			if ((actionInput && !previousActionInput) || (action2Input && !previousAction2Input))
			{
				bufferedSpindashInput = true;
			}
		}

		if ((((actionInput && !previousActionInput) || (action2Input && !previousAction2Input)) && canStartSpindash) 
		  || (bufferedSpindashInput && (actionInput || action2Input) && canStartSpindash))
		{
			if (!isSpindashing)
			{
				storedSpindashSpeed = (float)sqrt(xVelGround*xVelGround + zVelGround*zVelGround);
			}
			isSpindashing = true;
		}

		if (!actionInput && !action2Input)
		{
			isSpindashing = false;
			bufferedSpindashInput = false;
		}

		if (isSpindashing)
		{
			spindashTimer = std::min(spindashTimer + 1, spindashTimerMax);
			if (spindashTimer == 1)
			{
				AudioPlayer::play(14, getPosition());
			}
			isSpindashing = true;
			calcSpindashAngle();
			if (spindashTimer > spindashDelay)
			{
				applyFriction(spindashFriction);
			}
		}
		else
		{
			if (spindashTimer > 0)
			{
				spindash(spindashTimer);
			}
			spindashTimer = 0;
			storedSpindashSpeed = 0;
		}

		if (((actionInput && !previousActionInput) || (action2Input && !previousAction2Input)))
		{
			if (isBall)
			{
				spindashReleaseTimer = spindashReleaseTimerMax;
				spindashRestartDelay = spindashRestartDelayMax;
			}

			isBall = false;
		}
	}
	else //In the air
	{
		if (jumpInput && !previousJumpInput && homingAttackTimer == -1 && (isJumping || isBall || isBouncing))
		{
			homingAttack();
		}

		if (actionInput && !previousActionInput && (isJumping || isBall) && !isBouncing && homingAttackTimer == -1 && !isStomping)
		{
			initiateBounce();
		}

		if (action2Input && !previousAction2Input && (isJumping || isBall) && !isBouncing && homingAttackTimer == -1 && !isStomping)
		{
			initiateStomp();
		}

		isSpindashing = false;
		canStartSpindash = false;
		bufferedSpindashInput = false;
		spindashReleaseTimer = 0;
		spindashRestartDelay = 0;
		spindashTimer = 0;
		storedSpindashSpeed = 0;
		wallStickTimer = wallStickTimerMax;
		if (hoverCount > 0)
		{
			yVel += hoverAccel;
		}

		//if (Keyboard.isKeyDown(Keyboard.KEY_J))
		//{
			//yVel += 0.5f;
		//}
	}

	spindashReleaseTimer = std::max(spindashReleaseTimer - 1, 0);
	spindashRestartDelay = std::max(spindashRestartDelay - 1, 0);
	if (homingAttackTimer > 0)
	{
		homingAttackTimer--;
	}

	onPlanePrevious = onPlane;
	CollisionChecker::setCheckPlayer();
	if (CollisionChecker::checkCollision(getX(), getY(), getZ(), (float)(getX() + (xVel + xVelAir + xDisp)), (float)(getY() + (yVel + yDisp)), (float)(getZ() + (zVel + zVelAir + zDisp))))
	{
		triCol = CollisionChecker::getCollideTriangle();
		colPos = CollisionChecker::getCollidePosition();
		bool bonked = false;
		if (onPlane == false) //Transition from air to ground
		{
			if (isBouncing)
			{
				bounceOffGround(&(triCol->normal), 0.75f);
				isBall = true;
				isBouncing = false;
				isStomping = false;
				homingAttackTimer = -1;
				hoverCount = 0;
			}
			else
			{
				//New idea: if the wall is very steep, check if we are approaching the wall at too flat of an angle.
				//If we are, do a small bounce off the wall sintead of sticking to it.

				bool canStick = true;

				//std::fprintf(stdout, "normal.y = %f\n", triCol->normal.y);

				//Is the wall steep?
				if (triCol->normal.y < 0.3) //Some arbitrary steepness constant
				{
					Vector3f approach(xVel + xVelAir + xDisp, yVel + yDisp, zVel + zVelAir + zDisp);
					Vector3f wallNorm(&(triCol->normal));

					approach.normalize();
					wallNorm.normalize();

					float similarity = abs(wallNorm.dot(&approach));

					//std::fprintf(stdout, "similarity = %f\n", similarity);

					if (similarity > 0.6) //Another arbitrary "how steep of an angle you can stick at" constant
					{
						canStick = false;
					}

					//Not sure if I like this...
					canStick = false;
				}

				//Not sure if I like the canStick thing or not...
				canStick = true;

				if (canStick)
				{
					Vector3f speeds = calculatePlaneSpeed((float)((xVel + xVelAir + xDisp)), (float)((yVel + yDisp)), (float)(zVel + zVelAir + zDisp), colPos, &(triCol->normal));
					xVelGround = speeds.x;
					zVelGround = speeds.z;
					isBall = false;
					onPlane = true;
				}
				else
				{
					//Vector3f speeds(xVel + xVelAir + xDisp, yVel + yDisp, zVel + zVelAir + zDisp);
					//Vector3f newSPeeds = projectOntoPlane(&speeds, &(triCol->normal));
					//xVelAir = speeds.x;
					//yVel = speeds.y;
					//zVelAir = speeds.z;
					
					bounceOffGround(&(triCol->normal), 0.6f);
					canMoveTimer = 8;
					canMove = false;
					isBall = true;
					bonked = true;

					setPosition(colPos);
					increasePosition(triCol->normal.x * 1.5f, triCol->normal.y * 1.5f, triCol->normal.z * 1.5f);
				}
			}

			if (isStomping)
			{
				if (stompSource != nullptr)
				{
					stompSource->stop();
				}
				AudioPlayer::play(17, getPosition());

				//createStompParticles();
			}
		}
		else //check if you can smoothly transition from previous triangle to this triangle
		{
			float dotProduct = currNorm.dot(&(triCol->normal));
			if (dotProduct < 0.6)
			{
				xVelGround = 0;
				zVelGround = 0;

				bonked = true;

				currNorm.set(0, 1, 0);


				/*
				Vector3f currDisp(xVel + xDisp, yVel + yDisp, zVel + zDisp);

				Vector3f newDisp = projectOntoPlane(&currDisp, &(triCol->normal));

				CollisionChecker::setCheckPlayer();
				if (CollisionChecker::checkCollision(getX(), getY(), getZ(), getX() + newDisp.x, getY() + newDisp.y, getZ() + newDisp.z))
				{
					setPosition(CollisionChecker::getCollidePosition());
					increasePosition(CollisionChecker::getCollideTriangle()->normal.x, CollisionChecker::getCollideTriangle()->normal.y, CollisionChecker::getCollideTriangle()->normal.z);
				}
				else
				{
					increasePosition(newDisp.x, newDisp.y, newDisp.z);

					CollisionChecker::setCheckPlayer();
					bool checkPassed = CollisionChecker::checkCollision(getX(), getY(), getZ(), getX() - currNorm.x*surfaceTension, getY() - currNorm.y*surfaceTension, getZ() - currNorm.z*surfaceTension);
					if (checkPassed)
					{
						float dotProduct = currNorm.dot(&(CollisionChecker::getCollideTriangle()->normal));
						if (dotProduct < 0.6) //It's a wall, pretend the collision check didn't see it
						{
							checkPassed = false;
						}
					}

					if (checkPassed)
					{
						setPosition(CollisionChecker::getCollidePosition());
						increasePosition(CollisionChecker::getCollideTriangle()->normal.x, CollisionChecker::getCollideTriangle()->normal.y, CollisionChecker::getCollideTriangle()->normal.z);
					}
					else
					{
						CollisionChecker::falseAlarm();
						airTimer++;

						xVelGround = 0;
						zVelGround = 0;
						xVelAir = xVel + xDisp;
						zVelAir = zVel + zDisp;
						yVel += yDisp;
						xVel = 0;
						zVel = 0;

						onPlane = false;
					}
				}
				*/
			}
			onPlane = true;
		}

		if (!bonked)
		{
			setPosition(colPos);
			currNorm.set(&(triCol->normal));
			increasePosition(currNorm.x * 1.0f, currNorm.y * 1.0f, currNorm.z * 1.0f);
		}

		airTimer = 0;
	}
	else
	{
		increasePosition((float)((xVel + xVelAir + xDisp)), (float)((yVel + yDisp)), (float)((zVel + zVelAir + zDisp)));

		//Check for if there's just a small gap "below" us (relative to the normal of the triangle)
		//NEW: If the second check does pass but it is a wall, we pretend that the check didnt pass
		//Also, if we are in the air, then we can just ignore this second check altogether
		bool checkPassed = false;
		CollisionChecker::setCheckPlayer();
		if (onPlane)
		{
			checkPassed = CollisionChecker::checkCollision(getX(), getY(), getZ(), getX() - currNorm.x*surfaceTension, getY() - currNorm.y*surfaceTension, getZ() - currNorm.z*surfaceTension);
		}
		if (checkPassed)
		{
			float dotProduct = currNorm.dot(&(CollisionChecker::getCollideTriangle()->normal));
			if (dotProduct < 0.6) //It's a wall, pretend the collision check didn't see it
			{
				checkPassed = false;
			}
		}

		//Check again
		if (checkPassed)
		{
			triCol = CollisionChecker::getCollideTriangle();
			colPos = CollisionChecker::getCollidePosition();

			bool bonked = false;
			//check if you can smoothly transition from previous triangle to this triangle
			float dotProduct = currNorm.dot(&(CollisionChecker::getCollideTriangle()->normal));
			if (dotProduct < 0.6)
			{
				xVelGround = 0;
				zVelGround = 0;

				bonked = true;
				currNorm.set(0, 1, 0);
			}

			if (!bonked)
			{
				setPosition(colPos);
				currNorm.set(&(triCol->normal));
				increasePosition(currNorm.x * 1.0f, currNorm.y * 1.0f, currNorm.z * 1.0f);
			}
			airTimer = 0;
			onPlane = true;
		}
		else
		{
			CollisionChecker::falseAlarm();
			airTimer++;
			if (onPlane) //transition from on plane to off plane
			{
				xVelGround = 0;
				zVelGround = 0;
				xVelAir = xVel + xDisp;
				zVelAir = zVel + zDisp;
				yVel += yDisp;
				xVel = 0;
				zVel = 0;
			}
			onPlane = false;
		}
	}

	if (airTimer == 1)
	{
		currNorm.set(0, 1, 0);
	}

	if (getY() < -5)
	{
		inWater = true;
		waterHeight = 0;
	}

	if (getY() < -100)
	{
		die();
	}

	if (!inWater && inWaterPrevious)
	{
		AudioPlayer::play(5, getPosition());
		//new Particle(ParticleResources.textureSplash, new Vector3f(getX(), waterHeight + 5, getZ()), new Vector3f(0, 0, 0), 0, 30, 0, 10, 0);
		yVel += 0.4f;
		/*
		float totXVel = xVel + xVelAir;
		float totZVel = zVel + zVelAir;
		int numBubbles = ((int)Math.abs(yVel * 8)) + 18;
		for (int i = 0; i < numBubbles; i++)
		{
			float xOff = (float)(7 * (Math.random() - 0.5));
			float zOff = (float)(7 * (Math.random() - 0.5));
			new Particle(
				ParticleResources.textureBubble,
				new Vector3f(getX() + xOff, waterHeight + 2, getZ() + zOff),
				new Vector3f((float)(Math.random() - 0.5f + totXVel*0.4f),
				(float)(Math.random()*0.3 + 0.2f + yVel*0.3),
					(float)(Math.random() - 0.5f + totZVel*0.4f)),
				0.05f, 60, 0, 4, 0);
		}
		*/
	}

	//underwater friction
	if (inWater && !inWaterPrevious)
	{
		AudioPlayer::play(5, getPosition());
		//new Particle(ParticleResources.textureSplash, new Vector3f(getX(), waterHeight + 5, getZ()), new Vector3f(0, 0, 0), 0, 30, 0, 10, 0);
		/*
		float totXVel = xVel + xVelAir;
		float totZVel = zVel + zVelAir;
		int numBubbles = ((int)Math.abs(yVel * 8)) + 18;
		for (int i = 0; i < numBubbles; i++)
		{
			float xOff = (float)(7 * (Math.random() - 0.5));
			float zOff = (float)(7 * (Math.random() - 0.5));
			new Particle(
				ParticleResources.textureBubble,
				new Vector3f(getX() + xOff, waterHeight + 2, getZ() + zOff),
				new Vector3f((float)(Math.random() - 0.5f + totXVel*0.4f),
				(float)(Math.random()*0.3 + 0.2f - yVel*0.3),
					(float)(Math.random() - 0.5f + totZVel*0.4f)),
				0.05f, 60, 0, 4, 0);
		}
		*/
		yVel = (float)fmax(yVel, -1);
		xVelGround *= 0.75f;
		zVelGround *= 0.75f;
		xVelAir *= 0.75f;
		zVelAir *= 0.75f;
	}

	if (inWater)
	{

		xVelGround *= 0.987f;
		zVelGround *= 0.987f;
		xVelAir *= 0.987f;
		zVelAir *= 0.987f;
	}

	if (specialInput && !previousSpecialInput && !isLightdashing)
	{
		isLightdashing = true;
	}

	if (isLightdashing)
	{
		attemptLightdash();
	}

	isGettingExternallyMoved = false;
	xDisp = 0;
	yDisp = 0;
	zDisp = 0;

	//adjustCamera();

	animate(); //idea : in animate, when jumping, center the camera at not head height. then add back in the 10 units or whatever you get from jumping

	inWaterPrevious = inWater;
	inWater = false;

	Global::gameSkySphere->setPosition(getX(), 0, getZ());

	//std::fprintf(stdout, "player speed = %f\n", sqrtf(xVelGround*xVelGround + zVelGround*zVelGround));
}

void Player::addLimbsToGame()
{
	Main_addEntity(myBody);
	Main_addEntity(myHead);
	Main_addEntity(myLeftHumerus);
	Main_addEntity(myLeftForearm);
	Main_addEntity(myLeftHand);
	Main_addEntity(myLeftThigh);
	Main_addEntity(myLeftShin);
	Main_addEntity(myLeftFoot);
	Main_addEntity(myRightHumerus);
	Main_addEntity(myRightForearm);
	Main_addEntity(myRightHand);
	Main_addEntity(myRightThigh);
	Main_addEntity(myRightShin);
	Main_addEntity(myRightFoot);
}

void Player::removeLimbsFromGame()
{
	Main_deleteEntity(myBody); myBody = nullptr;
	Main_deleteEntity(myHead); myHead = nullptr;
	Main_deleteEntity(myLeftHumerus); myLeftHumerus = nullptr;
	Main_deleteEntity(myLeftForearm); myLeftForearm = nullptr;
	Main_deleteEntity(myLeftHand); myLeftHand = nullptr;
	Main_deleteEntity(myLeftThigh); myLeftThigh = nullptr;
	Main_deleteEntity(myLeftShin); myLeftShin = nullptr;
	Main_deleteEntity(myLeftFoot); myLeftFoot = nullptr;
	Main_deleteEntity(myRightHumerus); myRightHumerus = nullptr;
	Main_deleteEntity(myRightForearm); myRightForearm = nullptr;
	Main_deleteEntity(myRightHand); myRightHand = nullptr;
	Main_deleteEntity(myRightThigh); myRightThigh = nullptr;
	Main_deleteEntity(myRightShin); myRightShin = nullptr;
	Main_deleteEntity(myRightFoot); myRightFoot = nullptr;
}

void Player::setLimbsVisibility(bool visible)
{
	if (myBody == nullptr) return;
	myBody->setVisible(visible);
	myHead->setVisible(visible);
	myLeftHumerus->setVisible(visible);
	myLeftForearm->setVisible(visible);
	myLeftHand->setVisible(visible);
	myLeftThigh->setVisible(visible);
	myLeftShin->setVisible(visible);
	myLeftFoot->setVisible(visible);
	myRightHumerus->setVisible(visible);
	myRightForearm->setVisible(visible);
	myRightHand->setVisible(visible);
	myRightThigh->setVisible(visible);
	myRightShin->setVisible(visible);
	myRightFoot->setVisible(visible);
}

void Player::updateLimbs(int animIndex, float time)
{
	if (myBody == nullptr) return;
	myBody->animationIndex = animIndex;
	myHead->animationIndex = animIndex;
	myLeftHumerus->animationIndex = animIndex;
	myLeftForearm->animationIndex = animIndex;
	myLeftHand->animationIndex = animIndex;
	myLeftThigh->animationIndex = animIndex;
	myLeftShin->animationIndex = animIndex;
	myLeftFoot->animationIndex = animIndex;
	myRightHumerus->animationIndex = animIndex;
	myRightForearm->animationIndex = animIndex;
	myRightHand->animationIndex = animIndex;
	myRightThigh->animationIndex = animIndex;
	myRightShin->animationIndex = animIndex;
	myRightFoot->animationIndex = animIndex;
	myBody->update(time);
	myHead->update(time);
	myLeftHumerus->update(time);
	myLeftForearm->update(time);
	myLeftHand->update(time);
	myLeftThigh->update(time);
	myLeftShin->update(time);
	myLeftFoot->update(time);
	myRightHumerus->update(time);
	myRightForearm->update(time);
	myRightHand->update(time);
	myRightThigh->update(time);
	myRightShin->update(time);
	myRightFoot->update(time);
}

void Player::updateLimbsMatrix()
{
	if (myBody == nullptr) return;
	myBody->updateTransformationMatrix();
	myHead->updateTransformationMatrix();
	myLeftHumerus->updateTransformationMatrix();
	myLeftForearm->updateTransformationMatrix();
	myLeftHand->updateTransformationMatrix();
	myLeftThigh->updateTransformationMatrix();
	myLeftShin->updateTransformationMatrix();
	myLeftFoot->updateTransformationMatrix();
	myRightHumerus->updateTransformationMatrix();
	myRightForearm->updateTransformationMatrix();
	myRightHand->updateTransformationMatrix();
	myRightThigh->updateTransformationMatrix();
	myRightShin->updateTransformationMatrix();
	myRightFoot->updateTransformationMatrix();
}

void Player::createLimbs()
{
	if (Player::characterID == 0) //Classic Sonic
	{
		displayHeightOffset = 0;
		myBody = new Body(&modelBody); Global::countNew++;
		myHead = new Limb(&modelHead, 1.3f, 0, 0, myBody, nullptr); Global::countNew++;
		myLeftHumerus = new Limb(&modelLeftHumerus, 0.9f, 0, -0.9f, myBody, nullptr); Global::countNew++;
		myLeftForearm = new Limb(&modelLeftForearm, 0, -1.3f, 0, nullptr, myLeftHumerus); Global::countNew++;
		myLeftHand = new Limb(&modelLeftHand, 0, -1.3f, 0, nullptr, myLeftForearm); Global::countNew++;
		myLeftThigh = new Limb(&modelLeftThigh, -0.9f, 0, -0.3f, myBody, nullptr); Global::countNew++;
		myLeftShin = new Limb(&modelLeftShin, 0, -1.1f, 0, nullptr, myLeftThigh); Global::countNew++;
		myLeftFoot = new Limb(&modelLeftFoot, 0, -1.1f, 0, nullptr, myLeftShin); Global::countNew++;
		myRightHumerus = new Limb(&modelRightHumerus, 0.9f, 0, 0.9f, myBody, nullptr); Global::countNew++;
		myRightForearm = new Limb(&modelRightForearm, 0, -1.3f, 0, nullptr, myRightHumerus); Global::countNew++;
		myRightHand = new Limb(&modelRightHand, 0, -1.3f, 0, nullptr, myRightForearm); Global::countNew++;
		myRightThigh = new Limb(&modelRightThigh, -0.9f, 0, 0.3f, myBody, nullptr); Global::countNew++;
		myRightShin = new Limb(&modelRightShin, 0, -1.1f, 0, nullptr, myRightThigh); Global::countNew++;
		myRightFoot = new Limb(&modelRightFoot, 0, -1.1f, 0, nullptr, myRightShin); Global::countNew++;
	}
	else if (Player::characterID == 1) //Sonic Doll
	{
		displayHeightOffset = -0.525f;
		myBody = new Body(&modelBody); Global::countNew++;
		myHead = new Limb(&modelHead, 1.4f, -0.7f, 0, myBody, nullptr); Global::countNew++;
		myLeftHumerus = new Limb(&modelLeftHumerus, 0.9f, 0, -0.9f, myBody, nullptr); Global::countNew++;
		myLeftForearm = new Limb(&modelLeftForearm, 0, -0.92f, 0, nullptr, myLeftHumerus); Global::countNew++;
		myLeftHand = new Limb(&modelLeftHand, 0, -0.62f, 0, nullptr, myLeftForearm); Global::countNew++;
		myLeftThigh = new Limb(&modelLeftThigh, -0.9f, 0, -0.3f, myBody, nullptr); Global::countNew++;
		myLeftShin = new Limb(&modelLeftShin, 0, -1.07f, 0, nullptr, myLeftThigh); Global::countNew++;
		myLeftFoot = new Limb(&modelLeftFoot, 0, -1.23f, 0, nullptr, myLeftShin); Global::countNew++;
		myRightHumerus = new Limb(&modelRightHumerus, 0.9f, 0, 0.9f, myBody, nullptr); Global::countNew++;
		myRightForearm = new Limb(&modelRightForearm, 0, -0.92f, 0, nullptr, myRightHumerus); Global::countNew++;
		myRightHand = new Limb(&modelRightHand, 0, -0.62f, 0, nullptr, myRightForearm); Global::countNew++;
		myRightThigh = new Limb(&modelRightThigh, -0.9f, 0, 0.3f, myBody, nullptr); Global::countNew++;
		myRightShin = new Limb(&modelRightShin, 0, -1.07f, 0, nullptr, myRightThigh); Global::countNew++;
		myRightFoot = new Limb(&modelRightFoot, 0, -1.23f, 0, nullptr, myRightShin); Global::countNew++;
	}
	else if (Player::characterID == 2) //Mecha Sonic
	{
		displayHeightOffset = 0.8f;
		myBody =         new Body(&modelBody); Global::countNew++;
		myHead =         new Limb(&modelHead,         1.15f,  0,      0,     myBody,   nullptr);        Global::countNew++;
		myLeftHumerus =  new Limb(&modelLeftHumerus,  0.9f,   0,     -0.9f,  myBody,   nullptr);        Global::countNew++;
		myLeftForearm =  new Limb(&modelLeftForearm,  0,     -1.5f,   0,     nullptr,  myLeftHumerus);  Global::countNew++;
		myLeftHand =     new Limb(&modelLeftHand,     0,     -1.9f,   0,     nullptr,  myLeftForearm);  Global::countNew++;
		myLeftThigh =    new Limb(&modelLeftThigh,   -0.9f,   0,     -0.3f,  myBody,   nullptr);        Global::countNew++;
		myLeftShin =     new Limb(&modelLeftShin,     0,     -1.47f,  0,     nullptr,  myLeftThigh);    Global::countNew++;
		myLeftFoot =     new Limb(&modelLeftFoot,     0,     -1.21f,  0,     nullptr,  myLeftShin);     Global::countNew++;
		myRightHumerus = new Limb(&modelRightHumerus, 0.9f,   0,      0.9f,  myBody,   nullptr);        Global::countNew++;
		myRightForearm = new Limb(&modelRightForearm, 0,     -1.5f,   0,     nullptr,  myRightHumerus); Global::countNew++;
		myRightHand =    new Limb(&modelRightHand,    0,     -1.9f,   0,     nullptr,  myRightForearm); Global::countNew++;
		myRightThigh =   new Limb(&modelRightThigh,  -0.9f,   0,      0.3f,  myBody,   nullptr);        Global::countNew++;
		myRightShin =    new Limb(&modelRightShin,    0,     -1.47f,  0,     nullptr,  myRightThigh);   Global::countNew++;
		myRightFoot =    new Limb(&modelRightFoot,    0,     -1.21f,  0,     nullptr,  myRightShin);    Global::countNew++;
	}
	else if (Player::characterID == 3) //Dage 4 Aquatic
	{
		displayHeightOffset = 0;
		myBody =         new Body(&modelBody); Global::countNew++;
		myHead =         new Limb(&modelHead,         1.3f,   0,      0,     myBody,   nullptr);        Global::countNew++;
		myLeftHumerus =  new Limb(&modelLeftHumerus,  0.9f,   0,     -0.9f,  myBody,   nullptr);        Global::countNew++;
		myLeftForearm =  new Limb(&modelLeftForearm,  0,     -1.3f,   0,     nullptr,  myLeftHumerus);  Global::countNew++;
		myLeftHand =     new Limb(&modelLeftHand,     0,     -1.3f,   0,     nullptr,  myLeftForearm);  Global::countNew++;
		myLeftThigh =    new Limb(&modelLeftThigh,   -0.9f,   0,     -0.3f,  myBody,   nullptr);        Global::countNew++;
		myLeftShin =     new Limb(&modelLeftShin,     0,     -1.1f,   0,     nullptr,  myLeftThigh);    Global::countNew++;
		myLeftFoot =     new Limb(&modelLeftFoot,     0,     -1.1f,   0,     nullptr,  myLeftShin);     Global::countNew++;
		myRightHumerus = new Limb(&modelRightHumerus, 0.9f,   0,      0.9f,  myBody,   nullptr);        Global::countNew++;
		myRightForearm = new Limb(&modelRightForearm, 0,     -1.3f,   0,     nullptr,  myRightHumerus); Global::countNew++;
		myRightHand =    new Limb(&modelRightHand,    0,     -1.3f,   0,     nullptr,  myRightForearm); Global::countNew++;
		myRightThigh =   new Limb(&modelRightThigh,  -0.9f,   0,      0.3f,  myBody,   nullptr);        Global::countNew++;
		myRightShin =    new Limb(&modelRightShin,    0,     -1.1f,   0,     nullptr,  myRightThigh);   Global::countNew++;
		myRightFoot =    new Limb(&modelRightFoot,    0,     -1.1f,   0,     nullptr,  myRightShin);    Global::countNew++;
	}
	else if (Player::characterID == 4) //Mania Sonic
	{
		Player::maniaSonic = new ManiaSonicModel();
		Global::countNew++;
		Main_addEntity(Player::maniaSonic);

		displayHeightOffset = -0.25f;
		myBody =         new Body(&modelBody);
		myHead =         new Limb(&modelHead,         1.2f,  -0.3f,   0,     myBody,   nullptr);        Global::countNew++;
		myLeftHumerus =  new Limb(&modelLeftHumerus,  0.9f,	  0,     -0.9f,  myBody,   nullptr);        Global::countNew++;
		myLeftForearm =  new Limb(&modelLeftForearm,  0,     -1.3f,   0,     nullptr,  myLeftHumerus);  Global::countNew++;
		myLeftHand =     new Limb(&modelLeftHand,     0,	 -1.3f,   0,     nullptr,  myLeftForearm);  Global::countNew++;
		myLeftThigh =    new Limb(&modelLeftThigh,   -0.9f,   0,     -0.3f,  myBody,   nullptr);        Global::countNew++;
		myLeftShin =     new Limb(&modelLeftShin,     0,	 -1.3f,   0,     nullptr,  myLeftThigh);    Global::countNew++;
		myLeftFoot =     new Limb(&modelLeftFoot,     0,	 -1.1f,   0,     nullptr,  myLeftShin);     Global::countNew++;
		myRightHumerus = new Limb(&modelRightHumerus, 0.9f,   0,      0.9f,  myBody,   nullptr);        Global::countNew++;
		myRightForearm = new Limb(&modelRightForearm, 0,	 -1.3f,   0,     nullptr,  myRightHumerus); Global::countNew++;
		myRightHand =    new Limb(&modelRightHand,    0,	 -1.3f,   0,     nullptr,  myRightForearm); Global::countNew++;
		myRightThigh =   new Limb(&modelRightThigh,  -0.9f,   0,      0.3f,  myBody,   nullptr);        Global::countNew++;
		myRightShin =    new Limb(&modelRightShin,    0,	 -1.3f,   0,     nullptr,  myRightThigh);   Global::countNew++;
		myRightFoot =    new Limb(&modelRightFoot,    0,	 -1.1f,   0,     nullptr,  myRightShin);    Global::countNew++;
	}
	else if (Player::characterID == 5) //Amy
	{
		displayHeightOffset = -0.8f;
		myBody =         new Body(&modelBody); Global::countNew++;
		myHead =         new Limb(&modelHead,         1.4f,   0,      0,    myBody,  nullptr);        Global::countNew++;
		myLeftHumerus =  new Limb(&modelLeftHumerus,  1.1f,   0,     -0.9f, myBody,  nullptr);        Global::countNew++;
		myLeftForearm =  new Limb(&modelLeftForearm,  0,     -1.3f,   0,    nullptr, myLeftHumerus);  Global::countNew++;
		myLeftHand =     new Limb(&modelLeftHand,     0,     -1.3f,   0,    nullptr, myLeftForearm);  Global::countNew++;
		myLeftThigh =    new Limb(&modelLeftThigh,   -0.9f,   0,     -0.3f, myBody,  nullptr);        Global::countNew++;
		myLeftShin =     new Limb(&modelLeftShin,     0,     -1.1f,   0,    nullptr, myLeftThigh);    Global::countNew++;
		myLeftFoot =     new Limb(&modelLeftFoot,     0,     -1.1f,   0,    nullptr, myLeftShin);     Global::countNew++;
		myRightHumerus = new Limb(&modelRightHumerus, 1.1f,   0,      0.9f, myBody,  nullptr);        Global::countNew++;
		myRightForearm = new Limb(&modelRightForearm, 0,     -1.3f,   0,    nullptr, myRightHumerus); Global::countNew++;
		myRightHand =    new Limb(&modelRightHand,    0,     -1.3f,   0,    nullptr, myRightForearm); Global::countNew++;
		myRightThigh =   new Limb(&modelRightThigh,  -0.9f,   0,      0.3f, myBody,  nullptr);        Global::countNew++;
		myRightShin =    new Limb(&modelRightShin,    0,     -1.1f,   0,    nullptr, myRightThigh);   Global::countNew++;
		myRightFoot =    new Limb(&modelRightFoot,    0,     -1.1f,   0,    nullptr, myRightShin);    Global::countNew++;
	}
	else if (Player::characterID == 6) //WanamaDage
	{
		displayHeightOffset = 7.7f;
		myBody =         new Body(&modelBody); Global::countNew++;
		myHead =         new Limb(&modelHead,         4.835f,  0.715f,  0.0f,  myBody,  nullptr);        Global::countNew++;
		myLeftHumerus =  new Limb(&modelLeftHumerus,  3.963f,  0.28f,  -2.73f, myBody,  nullptr);        Global::countNew++;
		myLeftForearm =  new Limb(&modelLeftForearm,  0,      -3.7f,    0,     nullptr, myLeftHumerus);  Global::countNew++;
		myLeftHand =     new Limb(&modelLeftHand,     0,      -4.18f,   0,     nullptr, myLeftForearm);  Global::countNew++;
		myLeftThigh =    new Limb(&modelLeftThigh,   -3.893f,  0.42f,  -1.92f, myBody,  nullptr);        Global::countNew++;
		myLeftShin =     new Limb(&modelLeftShin,     0,      -4.42f,   0,     nullptr, myLeftThigh);    Global::countNew++;
		myLeftFoot =     new Limb(&modelLeftFoot,     0,      -3.75f,   0,     nullptr, myLeftShin);     Global::countNew++;
		myRightHumerus = new Limb(&modelRightHumerus, 3.963f,  0.28f,   2.73f, myBody,  nullptr);        Global::countNew++;
		myRightForearm = new Limb(&modelRightForearm, 0,      -3.7f,    0,     nullptr, myRightHumerus); Global::countNew++;
		myRightHand =    new Limb(&modelRightHand,    0,      -4.18f,   0,     nullptr, myRightForearm); Global::countNew++;
		myRightThigh =   new Limb(&modelRightThigh,  -3.893f,  0.42f,   1.92f, myBody,  nullptr);        Global::countNew++;
		myRightShin =    new Limb(&modelRightShin,    0,      -4.42f,   0,     nullptr, myRightThigh);   Global::countNew++;
		myRightFoot =    new Limb(&modelRightFoot,    0,      -3.75f,   0,     nullptr, myRightShin);    Global::countNew++;
	}

	AnimationResources::assignAnimationsHuman(myBody, myHead,
		myLeftHumerus, myLeftForearm, myLeftHand,
		myRightHumerus, myRightForearm, myRightHand,
		myLeftThigh, myLeftShin, myLeftFoot,
		myRightThigh, myRightShin, myRightFoot);

	addLimbsToGame();
}

std::list<TexturedModel*>* Player::getModels()
{
	return nullptr; //We should never be visible, so this should never be called anyway
}

void loadModelsHelper(std::list<TexturedModel*>* newModels, std::list<TexturedModel*>* destination)
{
	for (TexturedModel* newModel : (*newModels))
	{
		destination->push_back(newModel);
	}

	delete newModels;
	Global::countDelete++;
}

void Player::loadStaticModels()
{
	if (Player::characterID == 0)
	{
		if (Player::modelBody.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Body.obj"), &Player::modelBody); }
		if (Player::modelHead.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Head.obj"), &Player::modelHead); }
		if (Player::modelLeftHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Humerus.obj"), &Player::modelLeftHumerus); }
		if (Player::modelLeftForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Forearm.obj"), &Player::modelLeftForearm); }
		if (Player::modelLeftHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "LeftHand.obj"), &Player::modelLeftHand); }
		if (Player::modelLeftThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Thigh.obj"), &Player::modelLeftThigh); }
		if (Player::modelLeftShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Shin.obj"), &Player::modelLeftShin); }
		if (Player::modelLeftFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Foot.obj"), &Player::modelLeftFoot); }
		if (Player::modelRightHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Humerus.obj"), &Player::modelRightHumerus); }
		if (Player::modelRightForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Forearm.obj"), &Player::modelRightForearm); }
		if (Player::modelRightHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "RightHand.obj"), &Player::modelRightHand); }
		if (Player::modelRightThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Thigh.obj"), &Player::modelRightThigh); }
		if (Player::modelRightShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Shin.obj"), &Player::modelRightShin); }
		if (Player::modelRightFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Sonic/", "Foot.obj"), &Player::modelRightFoot); }
	}
	if (Player::characterID == 1) //Doll Sonic
	{
		if (Player::modelBody.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "Body.obj"), &Player::modelBody); }
		if (Player::modelHead.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "Head.obj"), &Player::modelHead); }
		if (Player::modelLeftHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "Humerus.obj"), &Player::modelLeftHumerus); }
		if (Player::modelLeftForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "Forearm.obj"), &Player::modelLeftForearm); }
		if (Player::modelLeftHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "HandL.obj"), &Player::modelLeftHand); }
		if (Player::modelLeftThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "Thigh.obj"), &Player::modelLeftThigh); }
		if (Player::modelLeftShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "Shin.obj"), &Player::modelLeftShin); }
		if (Player::modelLeftFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "FootL.obj"), &Player::modelLeftFoot); }
		if (Player::modelRightHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "Humerus.obj"), &Player::modelRightHumerus); }
		if (Player::modelRightForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "Forearm.obj"), &Player::modelRightForearm); }
		if (Player::modelRightHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "HandR.obj"), &Player::modelRightHand); }
		if (Player::modelRightThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "Thigh.obj"), &Player::modelRightThigh); }
		if (Player::modelRightShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "Shin.obj"), &Player::modelRightShin); }
		if (Player::modelRightFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicDoll/", "FootR.obj"), &Player::modelRightFoot); }
	}
	if (Player::characterID == 2) //Mecha Sonic
	{
		if (Player::modelBody.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "Body.obj"), &Player::modelBody); }
		if (Player::modelHead.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "Head.obj"), &Player::modelHead); }
		if (Player::modelLeftHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "Humerus.obj"), &Player::modelLeftHumerus); }
		if (Player::modelLeftForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "ForearmL.obj"), &Player::modelLeftForearm); }
		if (Player::modelLeftHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "HandL.obj"), &Player::modelLeftHand); }
		if (Player::modelLeftThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "Thigh.obj"), &Player::modelLeftThigh); }
		if (Player::modelLeftShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "ShinL.obj"), &Player::modelLeftShin); }
		if (Player::modelLeftFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "FootL.obj"), &Player::modelLeftFoot); }
		if (Player::modelRightHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "Humerus.obj"), &Player::modelRightHumerus); }
		if (Player::modelRightForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "ForearmR.obj"), &Player::modelRightForearm); }
		if (Player::modelRightHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "HandR.obj"), &Player::modelRightHand); }
		if (Player::modelRightThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "Thigh.obj"), &Player::modelRightThigh); }
		if (Player::modelRightShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "ShinR.obj"), &Player::modelRightShin); }
		if (Player::modelRightFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SilverSonic/", "FootR.obj"), &Player::modelRightFoot); }
	}
	else if(Player::characterID == 3) //Dage 4 Aquatic
	{
		if (Player::modelBody.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Body.obj"), &Player::modelBody); }
		if (Player::modelHead.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Head.obj"), &Player::modelHead); }
		if (Player::modelLeftHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Humerus.obj"), &Player::modelLeftHumerus); }
		if (Player::modelLeftForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Forearm.obj"), &Player::modelLeftForearm); }
		if (Player::modelLeftHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "LeftHand.obj"), &Player::modelLeftHand); }
		if (Player::modelLeftThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Thigh.obj"), &Player::modelLeftThigh); }
		if (Player::modelLeftShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Shin.obj"), &Player::modelLeftShin); }
		if (Player::modelLeftFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Foot.obj"), &Player::modelLeftFoot); }
		if (Player::modelRightHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Humerus.obj"), &Player::modelRightHumerus); }
		if (Player::modelRightForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Forearm.obj"), &Player::modelRightForearm); }
		if (Player::modelRightHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "RightHand.obj"), &Player::modelRightHand); }
		if (Player::modelRightThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Thigh.obj"), &Player::modelRightThigh); }
		if (Player::modelRightShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Shin.obj"), &Player::modelRightShin); }
		if (Player::modelRightFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Dage4Aquatic/", "Foot.obj"), &Player::modelRightFoot); }
	}
	else if (Player::characterID == 4) //Mania Sonic
	{
		ManiaSonicModel::loadStaticModels();

		if (Player::modelBody.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "Body.obj"), &Player::modelBody); }
		if (Player::modelHead.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "Head.obj"), &Player::modelHead); }
		if (Player::modelLeftHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "Humerus.obj"), &Player::modelLeftHumerus); }
		if (Player::modelLeftForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "Forearm.obj"), &Player::modelLeftForearm); }
		if (Player::modelLeftHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "HandLeft.obj"), &Player::modelLeftHand); }
		if (Player::modelLeftThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "Thigh.obj"), &Player::modelLeftThigh); }
		if (Player::modelLeftShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "Shin.obj"), &Player::modelLeftShin); }
		if (Player::modelLeftFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "ShoeLeft.obj"), &Player::modelLeftFoot); }
		if (Player::modelRightHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "Humerus.obj"), &Player::modelRightHumerus); }
		if (Player::modelRightForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "Forearm.obj"), &Player::modelRightForearm); }
		if (Player::modelRightHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "HandRight.obj"), &Player::modelRightHand); }
		if (Player::modelRightThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "Thigh.obj"), &Player::modelRightThigh); }
		if (Player::modelRightShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "Shin.obj"), &Player::modelRightShin); }
		if (Player::modelRightFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/SonicMania/", "ShoeRight.obj"), &Player::modelRightFoot); }
	}
	else if (Player::characterID == 5) //Amy
	{
		if (Player::modelBody.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Body.obj"), &Player::modelBody); }
		if (Player::modelHead.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Head.obj"), &Player::modelHead); }
		if (Player::modelLeftHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Humerus.obj"), &Player::modelLeftHumerus); }
		if (Player::modelLeftForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Forearm.obj"), &Player::modelLeftForearm); }
		if (Player::modelLeftHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "LeftHand.obj"), &Player::modelLeftHand); }
		if (Player::modelLeftThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Thigh.obj"), &Player::modelLeftThigh); }
		if (Player::modelLeftShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Shin.obj"), &Player::modelLeftShin); }
		if (Player::modelLeftFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Foot.obj"), &Player::modelLeftFoot); }
		if (Player::modelRightHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Humerus.obj"), &Player::modelRightHumerus); }
		if (Player::modelRightForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Forearm.obj"), &Player::modelRightForearm); }
		if (Player::modelRightHand.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "RightHand.obj"), &Player::modelRightHand); }
		if (Player::modelRightThigh.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Thigh.obj"), &Player::modelRightThigh); }
		if (Player::modelRightShin.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Shin.obj"), &Player::modelRightShin); }
		if (Player::modelRightFoot.size() == 0) { loadModelsHelper(loadObjModel("res/Models/Amy/", "Foot.obj"), &Player::modelRightFoot); }
	}
	else if (Player::characterID == 6) //WanamaDage
	{
		if (Player::modelBody.size() == 0) {         loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "Body.obj"), &Player::modelBody); }
		if (Player::modelHead.size() == 0) {         loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "Head.obj"), &Player::modelHead); }
		if (Player::modelLeftHumerus.size() == 0) {  loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "HumerusLeft.obj"), &Player::modelLeftHumerus); }
		if (Player::modelLeftForearm.size() == 0) {  loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "ForearmLeft.obj"), &Player::modelLeftForearm); }
		if (Player::modelLeftHand.size() == 0) {     loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "HandLeft.obj"), &Player::modelLeftHand); }
		if (Player::modelLeftThigh.size() == 0) {    loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "ThighLeft.obj"), &Player::modelLeftThigh); }
		if (Player::modelLeftShin.size() == 0) {     loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "ShinLeft.obj"), &Player::modelLeftShin); }
		if (Player::modelLeftFoot.size() == 0) {     loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "Shoe.obj"), &Player::modelLeftFoot); }
		if (Player::modelRightHumerus.size() == 0) { loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "HumerusRight.obj"), &Player::modelRightHumerus); }
		if (Player::modelRightForearm.size() == 0) { loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "ForearmRight.obj"), &Player::modelRightForearm); }
		if (Player::modelRightHand.size() == 0) {    loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "HandRight.obj"), &Player::modelRightHand); }
		if (Player::modelRightThigh.size() == 0) {   loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "ThighRight.obj"), &Player::modelRightThigh); }
		if (Player::modelRightShin.size() == 0) {    loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "ShinRight.obj"), &Player::modelRightShin); }
		if (Player::modelRightFoot.size() == 0) {    loadModelsHelper(loadObjModel("res/Models/WanamaDageLimbs/", "Shoe.obj"), &Player::modelRightFoot); }
	}
}

void Player::deleteStaticModels()
{
	std::fprintf(stdout, "Deleting player models...\n");
	for (auto model : Player::modelBody) {         model->deleteMe(); delete model; Global::countDelete++; } Player::modelBody.clear();
	for (auto model : Player::modelHead) {         model->deleteMe(); delete model; Global::countDelete++; } Player::modelHead.clear();
	for (auto model : Player::modelLeftHumerus) {  model->deleteMe(); delete model; Global::countDelete++; } Player::modelLeftHumerus.clear();
	for (auto model : Player::modelLeftForearm) {  model->deleteMe(); delete model; Global::countDelete++; } Player::modelLeftForearm.clear();
	for (auto model : Player::modelLeftHand) {     model->deleteMe(); delete model; Global::countDelete++; } Player::modelLeftHand.clear();
	for (auto model : Player::modelLeftThigh) {    model->deleteMe(); delete model; Global::countDelete++; } Player::modelLeftThigh.clear();
	for (auto model : Player::modelLeftShin) {     model->deleteMe(); delete model; Global::countDelete++; } Player::modelLeftShin.clear();
	for (auto model : Player::modelLeftFoot) {     model->deleteMe(); delete model; Global::countDelete++; } Player::modelLeftFoot.clear();
	for (auto model : Player::modelRightHumerus) { model->deleteMe(); delete model; Global::countDelete++; } Player::modelRightHumerus.clear();
	for (auto model : Player::modelRightForearm) { model->deleteMe(); delete model; Global::countDelete++; } Player::modelRightForearm.clear();
	for (auto model : Player::modelRightHand) {    model->deleteMe(); delete model; Global::countDelete++; } Player::modelRightHand.clear();
	for (auto model : Player::modelRightThigh) {   model->deleteMe(); delete model; Global::countDelete++; } Player::modelRightThigh.clear();
	for (auto model : Player::modelRightShin) {    model->deleteMe(); delete model; Global::countDelete++; } Player::modelRightShin.clear();
	for (auto model : Player::modelRightFoot) {    model->deleteMe(); delete model; Global::countDelete++; } Player::modelRightFoot.clear();
}

void Player::moveMeGround()
{
	Camera* cam = Global::gameCamera;

	if (isBall == false)
	{
		xVelGround += (float)(moveSpeedCurrent*cos(toRadians(cam->getYaw() + movementAngle)));
		zVelGround += (float)(moveSpeedCurrent*sin(toRadians(cam->getYaw() + movementAngle)));
	}
	else
	{
		xVelGround += (float)(moveSpeedCurrent*cos(toRadians(cam->getYaw() + movementAngle)))*0.8f;
		zVelGround += (float)(moveSpeedCurrent*sin(toRadians(cam->getYaw() + movementAngle)))*0.8f;
	}

	float currSpeed = (float)sqrt((xVelGround*xVelGround) + (zVelGround*zVelGround));
	float currDir = (float)toDegrees(atan2(zVelGround, xVelGround));

	if (moveSpeedCurrent > 0.01f && currSpeed > 0.5f)
	{
		float worldSpaceMovementAngle = cam->getYaw() + movementAngle;
		float diff = compareTwoAngles(worldSpaceMovementAngle, currDir);

		float newDiff = diff / 8;

		float newAngle = currDir + newDiff;

		if (fabs(diff) > 120)
		{
			xVelGround *= 0.97f;
			zVelGround *= 0.97f;
		}
		else
		{
			xVelGround = (currSpeed*cosf(toRadians(newAngle)));
			zVelGround = (currSpeed*sinf(toRadians(newAngle)));
		}
	}
}

void Player::setMovementInputs()
{
	jumpInput = INPUT_JUMP;
	actionInput = INPUT_ACTION;
	action2Input = INPUT_ACTION2;
	shoulderInput = INPUT_SHOULDER;
	selectInput = INPUT_SELECT;
	specialInput = INPUT_SPECIAL;

	previousJumpInput = INPUT_PREVIOUS_JUMP;
	previousActionInput = INPUT_PREVIOUS_ACTION;
	previousAction2Input = INPUT_PREVIOUS_ACTION2;
	previousShoulderInput = INPUT_PREVIOUS_SHOULDER;
	previousSelectInput = INPUT_PREVIOUS_SELECT;
	previousSpecialInput = INPUT_PREVIOUS_SPECIAL;

	movementInputX = INPUT_X;
	movementInputY = INPUT_Y;
	cameraInputX = INPUT_X2;
	cameraInputY = INPUT_Y2;

	zoomInput = INPUT_ZOOM;

	float inputMag = sqrtf(movementInputX*movementInputX + movementInputY*movementInputY);
	moveSpeedCurrent = moveAcceleration*inputMag;
	moveSpeedAirCurrent = moveAccelerationAir*inputMag;
	movementAngle = toDegrees(atan2f(movementInputY, movementInputX));

	if (canMove == false || hitTimer > 0 || deadTimer >= 0)
	{
		jumpInput = false;
		actionInput = false;
		action2Input = false;
		shoulderInput = false;
		selectInput = false;
		specialInput = false;
		previousJumpInput = false;
		previousActionInput = false;
		previousAction2Input = false;
		previousShoulderInput = false;
		previousSelectInput = false;
		previousSpecialInput = false;
		movementInputX = 0;
		movementInputY = 0;
		moveSpeedCurrent = 0;
		moveSpeedAirCurrent = 0;
		movementAngle = 0;
	}
}

void Player::setCanMove(bool newMove)
{
	this->canMove = newMove;
}

void Player::checkSkid()
{
	bool prevSkid = isSkidding;
	isSkidding = false;
	if (currNorm.y > 0.8)
	{
		float degAngle = movementAngle;

		degAngle = (float)fmod(-degAngle - cameraYawTarget, 360);

		if (degAngle < 0)
		{
			degAngle += 360;
		}

		float myAngle = getRotY();

		if (myAngle < 0)
		{
			myAngle += 360;
		}

		if ((abs(myAngle - degAngle) <= 220) && abs(myAngle - degAngle) >= 140)
		{
			if (movementInputX != 0 || movementInputY != 0)
			{
				isSkidding = true;
				float currSpeed = xVel*xVel + zVel*zVel;
				if (!prevSkid && onPlane && currSpeed > 2)
				{
					AudioPlayer::play(13, getPosition());
				}
			}
		}
	}
}

void Player::applyFrictionAir()
{
	float mag = (float)sqrt((xVelGround*xVelGround) + (zVelGround*zVelGround));
	if (mag != 0)
	{
		int before = sign(zVelGround);
		zVelGround -= (((frictionAir)*(zVelGround)) / (mag));
		int after = sign(zVelGround);
		if (before != after)
		{
			zVelGround = 0;
		}
		before = sign(xVelGround);
		xVelGround -= (((frictionAir)*(xVelGround)) / (mag));
		after = sign(xVelGround);
		if (before != after)
		{
			xVelGround = 0;
		}
	}

	mag = (float)sqrt((xVelAir*xVelAir) + (zVelAir*zVelAir));
	if (mag != 0)
	{
		int before = sign(zVelAir);
		zVelAir -= (((frictionAir)*(zVelAir)) / (mag));
		int after = sign(zVelAir);
		if (before != after)
		{
			zVelAir = 0;
		}
		before = sign(xVelAir);
		xVelAir -= (((frictionAir)*(xVelAir)) / (mag));
		after = sign(xVelAir);
		if (before != after)
		{
			xVelAir = 0;
		}
	}
}

void Player::moveMeAir()
{
	Camera* cam = Global::gameCamera;
	xVelAir += (float)(moveSpeedAirCurrent*cos(toRadians(cam->getYaw() + movementAngle)));
	zVelAir += (float)(moveSpeedAirCurrent*sin(toRadians(cam->getYaw() + movementAngle)));

	float currSpeed = (float)sqrt((xVelAir*xVelAir) + (zVelAir*zVelAir));
	float currDir = (float)toDegrees(atan2(zVelAir, xVelAir));

	if (moveSpeedAirCurrent > 0.01 && currSpeed > 0.5)
	{
		float worldSpaceMovementAngle = cam->getYaw() + movementAngle;
		float diff = compareTwoAngles(worldSpaceMovementAngle, currDir);

		float newDiff = diff / 8;

		float newAngle = currDir + newDiff;

		xVelAir = (float)(currSpeed*cos(toRadians(newAngle)));
		zVelAir = (float)(currSpeed*sin(toRadians(newAngle)));

		float factor = 1 - (abs(diff) / 900);
		xVelAir *= factor;
		zVelAir *= factor;
	}
}

void Player::limitMovementSpeedAir()
{
	float myspeed = (float)sqrt(xVelAir*xVelAir + zVelAir*zVelAir);
	if (myspeed > airSpeedLimit)
	{
		xVelAir = (xVelAir*((myspeed - slowDownAirRate) / (myspeed)));
		zVelAir = (zVelAir*((myspeed - slowDownAirRate) / (myspeed)));
	}
}

void Player::limitMovementSpeed(float limit)
{
	if (isBall)
	{
		limit = limit*0.1f;
	}

	float myspeed = (float)sqrt(xVelGround*xVelGround + zVelGround*zVelGround);
	if (myspeed > limit)
	{
		xVelGround = xVelGround*((myspeed - slowDownRate) / (myspeed));
		zVelGround = zVelGround*((myspeed - slowDownRate) / (myspeed));
	}
}

void Player::applyFriction(float frictionToApply)
{
	if (isBall)
	{
		frictionToApply *= 0.11f;
	}

	float mag = (float)sqrt((xVelGround*xVelGround) + (zVelGround*zVelGround));
	if (mag != 0)
	{
		int before = sign(zVelGround);
		zVelGround -= (((frictionToApply)*(zVelGround)) / (mag));
		int after = sign(zVelGround);
		if (before != after)
		{
			zVelGround = 0;
		}
		before = sign(xVelGround);
		xVelGround -= (((frictionToApply)*(xVelGround)) / (mag));
		after = sign(xVelGround);
		if (before != after)
		{
			xVelGround = 0;
		}
	}
}

void Player::calcSpindashAngle()
{
	Camera* cam = Global::gameCamera;
	float inputMag = (float)sqrt(movementInputX*movementInputX + movementInputY*movementInputY);
	if (inputMag >= 0.3)
	{
		spindashAngle = (float)((toRadians(-cam->getYaw() - movementAngle)));
	}
	else
	{
		spindashAngle = (float)((toRadians(getRotY())));
	}
}

void Player::spindash(int timer)
{
	float mag = spindashPower*timer;
	float dx = (float)cos(spindashAngle)*mag;
	float dz = -(float)sin(spindashAngle)*mag;

	float xspd = xVelGround;
	float zspd = zVelGround;
	float totalSpd = (float)sqrt(xspd*xspd + zspd*zspd);

	float factor = (float)std::fmin(1, 6.5f / totalSpd);

	factor = (float)std::fmax(0.9f, factor);

	xVelGround += dx*factor;
	zVelGround += dz*factor;

	float newSpeed = (float)sqrt(xVelGround*xVelGround + zVelGround*zVelGround);

	if (totalSpd > 1 && newSpeed < storedSpindashSpeed)
	{
		xVelGround = (float)cos(spindashAngle)*storedSpindashSpeed;
		zVelGround = -(float)sin(spindashAngle)*storedSpindashSpeed;
	}

	isBall = true;
	AudioPlayer::play(15, getPosition());
	storedSpindashSpeed = 0;
}

void Player::homingAttack()
{
	Camera* cam = Global::gameCamera;

	float homingPower = 5.5f;

	float currSpeed = sqrt(xVelAir*xVelAir + zVelAir*zVelAir);

	homingPower = std::max(currSpeed, homingPower);

	float angle = -cam->getYaw() - movementAngle;
	if (moveSpeedCurrent < 0.05)
	{
		angle = getRotY();
	}

	//if (characterID == 2)
	{
		bool targetNearest = false;
		float closestMatchingAngle = 360;
		float stickAngle = -cam->getYaw() - movementAngle;

		if (moveSpeedCurrent < 0.05)
		{
			angle = getRotY();
			targetNearest = true;
		}

		Entity* closest = nullptr;
		float dist = 12000; //Homing attack range. Distance squared
		float myX = position.x;
		float myZ = position.z;
		float myY = position.y;

		extern std::unordered_map<Entity*, Entity*> gameEntities;

		for (auto e : gameEntities)
		{
			if (e.first->canHomingAttackOn())
			{
				float xDiff = e.first->getX() - myX;
				float zDiff = e.first->getZ() - myZ;
				float yDiff = myY - e.first->getY();
				float thisDist = xDiff*xDiff + zDiff*zDiff + yDiff*yDiff;

				if (targetNearest)
				{
					if (yDiff > -6 && thisDist < dist)
					{
						dist = thisDist;
						closest = e.first;
					}
				}
				else
				{
					if (thisDist < dist)
					{
						//calculate angle to this target
						float toTargetAngle = toDegrees(atan2f(zDiff, -xDiff)) + 180;
						//calculate difference in angles
						float angleDiff = abs(compareTwoAngles(stickAngle, toTargetAngle));

						if (angleDiff < 60)
						{
							if (yDiff > -6 && angleDiff < closestMatchingAngle)
							{
								closestMatchingAngle = angleDiff;
								closest = e.first;
							}
						}
					}
				}
			}
		}

		if (closest != nullptr)
		{
			
			float xDiff = -(myX - closest->getX());
			float zDiff = -(myZ - closest->getZ());
			float yDiff = -(myY - closest->getY());

			Vector3f unitDir(xDiff, yDiff + 3.5f, zDiff);
			unitDir.normalize();
			homingPower = 6.7f;

			unitDir.x *= homingPower;
			unitDir.y *= homingPower;
			unitDir.z *= homingPower;

			xVelAir = unitDir.x;
			yVel    = unitDir.y;
			zVelAir = unitDir.z;
			
		}
		else
		{
			xVelAir =  (float)cos(toRadians(angle))*homingPower;
			zVelAir = -(float)sin(toRadians(angle))*homingPower;
		}
	}
	//else
	{
		//xVelAir = (float)cos(toRadians(angle))*homingPower;
		//zVelAir = -(float)sin(toRadians(angle))*homingPower;
	}

	homingAttackTimer = homingAttackTimerMax;
	isBall = false;
	isJumping = true;
	isBouncing = false;
	isStomping = false;
	AudioPlayer::play(11, getPosition());
}

void Player::initiateBounce()
{
	if (yVel >= -4.5f)
	{
		yVel = -4.5f;
	}
	else
	{
		yVel -= 1;
	}

	hoverCount = 0;

	isBouncing = true;
	isStomping = false;
	isJumping = false;
	isBall = false;
}

void Player::initiateStomp()
{
	if (yVel >= -4.0f)
	{
		yVel = -4.0f;
	}
	else
	{
		yVel -= 2.0f;
	}

	hoverCount = 0;

	isBouncing = false;
	isStomping = true;
	isJumping = false;
	isBall = false;

	stompSource = AudioPlayer::play(16, getPosition());
}

void Player::bounceOffGround(Vector3f* surfaceNormal, float b)
{
	Vector3f V = Vector3f(xVelAir, yVel, zVelAir);
	Vector3f N = Vector3f(surfaceNormal);

	Vector3f Vnew = bounceVector(&V, &N, b);

	xVelAir = Vnew.x;
	yVel = Vnew.y;
	zVelAir = Vnew.z;

	xVel = 0;
	zVel = 0;

	//isBall = true;
	//isBouncing = false;
	//isStomping = false;
	//homingAttackTimer = -1;
	//hoverCount = 0;
	AudioPlayer::play(8, getPosition());
}

//attempt to continue a lightdash
//if lightdash cant continue, sets isLightdashing
//to false
void Player::attemptLightdash()
{
	if (Global::gameClock % 2 != 0)
	{
		return;
	}
	//find nearest ring
	Entity* closest = nullptr;
	float dist = 2000; //distance squared
	float myX = getX();
	float myZ = getZ();
	float myY = getY();

	extern std::unordered_map<Entity*, Entity*> gameEntities;

	for (auto e : gameEntities)
	{
		if (e.first->canLightdashOn())
		{
			float xDiff = e.first->getX() - myX;
			float zDiff = e.first->getZ() - myZ;
			float yDiff = e.first->getY() - myY;
			float newDist = xDiff*xDiff + zDiff*zDiff + yDiff*yDiff;
			if (newDist < dist)
			{
				dist = newDist;
				closest = e.first;
			}
		}
	}


	//check nearest distance
	//System.out.println("dist = "+dist);
	//passes threshold?
	if (dist < 2000)
	{
		onPlane = false;

		xVel = 0;
		zVel = 0;
		xVelGround = 0;
		zVelGround = 0;

		//set xVelAir and zVelAir to the direction to the ring
		Vector3f diff(closest->getX() - myX, closest->getY() - myY, closest->getZ() - myZ);
		diff.normalize();

		xVelAir = diff.x * 5;
		zVelAir = diff.z * 5;
		yVel = diff.y * 5;

		//move to ring location
		setX(closest->getX());
		setY(closest->getY());
		setZ(closest->getZ());
	}
	else
	{
		//doesnt pass threshold?
		isLightdashing = false;
	}
}

void Player::adjustCamera()
{
	if (deadTimer == -1)
	{
		cameraRadius += (cameraRadiusTarget - cameraRadius) / 20;

		cameraRadiusTarget += zoomInput;
		cameraRadiusTarget = fmax(cameraRadiusZoom, cameraRadiusTarget);
		cameraRadiusTarget = fmin(cameraRadiusZoomOut, cameraRadiusTarget);

		Camera* cam = Global::gameCamera;
		//Vector3f* camPos = cam->getPosition();

		//if (!firstPerson && !MainGameLoop.freeCamera)
		//{
			//float xDiff = camPos->x - position.x;
			//float zDiff = camPos->z - position.z;
			//float angleRad = (float)toRadians(getRotY());
			//float newZ = (float)(-(zDiff)*cos(angleRad) - (xDiff)*sin(angleRad));
			//float newX = (float)((xDiff)*Math.cos(angleRad) - (zDiff)*Math.sin(angleRad));

			//cameraYawTarget -= (newZ / 65);
		//}


		cameraPitchTarget += cameraInputY;
		cameraYawTarget += cameraInputX;


		cam->setYaw(cam->getYaw() + (cameraYawTarget - cam->getYaw()) / cameraLaziness);
		cam->setPitch(cam->getPitch() + (cameraPitchTarget - cam->getPitch()) / cameraLaziness);



		Vector3f headPos(
			position.x + currNorm.x*headHeight,
			position.y + currNorm.y*headHeight,
			position.z + currNorm.z*headHeight);

		Vector3f camPos(
			headPos.x + (float)(cos(toRadians(cam->getYaw() + 90))*(cameraRadius*(cos(toRadians(cam->getPitch()))))),
			headPos.y - (float)(sin(toRadians(cam->getPitch() + 180))*cameraRadius),
			headPos.z + (float)(sin(toRadians(cam->getYaw() + 90))*(cameraRadius*(cos(toRadians(cam->getPitch()))))));


		if (CollisionChecker::checkCollision(position.x, position.y, position.z, headPos.x, headPos.y, headPos.z) == true)
		{
			Vector3f* colPos = CollisionChecker::getCollidePosition();

			Vector3f diff;

			diff.x = colPos->x - getX();
			diff.y = colPos->y - getY();
			diff.z = colPos->z - getZ();

			float newHeadHeight = diff.length() - 1;

			//camPos.set(getX() + currNorm.x*newHeadHeight,
			//	getY() + currNorm.y*newHeadHeight,
			//	getZ() + currNorm.z*newHeadHeight);
		}
		else if (CollisionChecker::checkCollision(headPos.x, headPos.y, headPos.z, camPos.x, camPos.y, camPos.z) == true)
		{
			Vector3f* colPos = CollisionChecker::getCollidePosition();

			Vector3f diff;

			diff.x = colPos->x - headPos.x;
			diff.y = colPos->y - headPos.y;
			diff.z = colPos->z - headPos.z;

			float newRadius = diff.length() - 4;

			diff.normalize();
			diff.scale(newRadius);

			camPos.set(
				headPos.x + diff.x,
				headPos.y + diff.y,
				headPos.z + diff.z);

		}


		cam->setPosition(&camPos);

		//if (zoomInput > 0)
		{
			//firstPerson = true;
		}
		//else if (zoomInput < 0)
		{
			//firstPerson = false;
		}

		//if (firstPerson == true)
		{
			//cam.setPosition(headPos);
		}
		/*
		Vector3f normOpp(currNorm);
		normOpp.neg();

		Vector3f newCamPos = mapCamera(toRadians(cameraYawTarget+90), 0, cameraRadius, &normOpp);

		float newYaw = atan2f(newCamPos.z, newCamPos.x);
		float newPitch = atan2f(newCamPos.y, sqrtf(newCamPos.x*newCamPos.x + newCamPos.z*newCamPos.z));

		newCamPos.set(
			headPos.x + newCamPos.x,
			headPos.y + newCamPos.y,
			headPos.z + newCamPos.z);

		cam->setPosition(&newCamPos);

		cam->setYaw(toDegrees(newYaw)-90);
		cam->setPitch(toDegrees(newPitch));
		*/
	}
	else
	{
		Camera* cam = Global::gameCamera;

		float xDiff = (cam->getPosition()->getX() - getX());
		float yDiff = (cam->getPosition()->getY() - getY());
		float zDiff = (cam->getPosition()->getZ() - getZ());


		float newYaw = atan2f(zDiff, xDiff);
		float newPitch = atan2f(yDiff, sqrtf(xDiff*xDiff + zDiff*zDiff));

		cam->setYaw(toDegrees(newYaw)-90);
		cam->setPitch(toDegrees(newPitch));
	}
}

void Player::setCameraAngles(float newYaw, float newPitch)
{
	cameraYawTarget = newYaw;
	cameraPitchTarget = newPitch;
	Camera* cam = Global::gameCamera;
	cam->setYaw(newYaw);
	cam->setPitch(newPitch);
}

void Player::setCameraTargetYaw(float yaw)
{
	cameraYawTarget = yaw;

	Camera* cam = Global::gameCamera;

	float diff = compareTwoAngles(yaw, cam->getYaw());

	cam->setYaw(yaw - diff);
}

void Player::setCameraTargetPitch(float pitch)
{
	cameraPitchTarget = pitch;
}

void Player::animate()
{
	float xrel = xVelGround + xVelAir;
	float zrel = zVelGround + zVelAir;
	float ang = atan2f(zrel, xrel);
	Vector3f normOpp(currNorm);
	normOpp.neg();
	Vector3f ans = mapInputs3(ang, 10, &normOpp);
	float yrot = toDegrees(atan2f(-ans.z, ans.x));
	float dist = sqrtf(ans.x*ans.x + ans.z*ans.z);
	float zrot = toDegrees(atan2f(ans.y, dist));

	float twistAngle = toDegrees(atan2f(-zVelGround - zVelAir, xVelGround + xVelAir));
	float nX = currNorm.x;
	float nY = currNorm.y;
	float nZ = currNorm.z;
	float normHLength = sqrtf(nX*nX + nZ*nZ);
	float pitchAngle = toDegrees(atan2f(nY, normHLength));
	float yawAngle = toDegrees(atan2f(-nZ, nX));
	float diff = compareTwoAngles(twistAngle, yawAngle);

	Vector3f displayPos(
		getX() + nX*displayHeightOffset,
		getY() + nY*displayHeightOffset,
		getZ() + nZ*displayHeightOffset);

	Vector3f dspOff(0, 0, 0);

	if (onPlane)
	{
		if (isBouncing ||
			isBall ||
			isSpindashing ||
			spindashReleaseTimer > 0)
		{
			dspOff.set(currNorm.x*2.5f, currNorm.y*2.5f, currNorm.z*2.5f);
		}
		else
		{
			dspOff.set(-currNorm.x * 1, -currNorm.y * 1, -currNorm.z * 1);
		}
	}

	float dspX = getX() + dspOff.x;
	float dspY = getY() + dspOff.y;
	float dspZ = getZ() + dspOff.z;

	Vector3f displayPosPrev(previousPos);
	displayPosPrev.x += nX*displayHeightOffset + dspOff.x;
	displayPosPrev.y += nY*displayHeightOffset + dspOff.y;
	displayPosPrev.z += nZ*displayHeightOffset + dspOff.z;

	float mySpeed = sqrtf(xrel*xrel + zrel*zrel);


	setLimbsVisibility(true);
	if (Player::maniaSonic != nullptr) { Player::maniaSonic->setVisible(true); }

	if (firstPerson)
	{
		setLimbsVisibility(false);
		if (Player::maniaSonic != nullptr) { Player::maniaSonic->setVisible(false); }
	}
	else
	{
		if (iFrame % 4 == 2 || iFrame % 4 == 3)
		{
			setLimbsVisibility(false);
			if (Player::maniaSonic != nullptr) { Player::maniaSonic->setVisible(false); }
		}
	}

	if (mySpeed > 0.01)
	{
		if (myBody != nullptr) myBody->setBaseOrientation(&displayPos, diff, yawAngle, pitchAngle, 0);
		if (Player::maniaSonic != nullptr)
		{
			Player::maniaSonic->setOrientation(dspX, dspY, dspZ, diff, yawAngle, pitchAngle, 0);
		}
		setRotY(yrot);
		setRotZ(zrot);
	}
	else
	{
		if (myBody != nullptr) myBody->setBaseOrientation(&displayPos, 0, getRotY(), 90, 0);
		if (Player::maniaSonic != nullptr)
		{
			Player::maniaSonic->setOrientation(dspX, dspY, dspZ, 0, getRotY(), 90, 0);
		}
	}

	float modelIncreaseVal = (mySpeed)*0.3f;
	modelRunIndex += modelIncreaseVal;
	if (modelRunIndex >= 20)
	{
		modelRunIndex = (float)fmod(modelRunIndex, 20);
	}
	if (mySpeed == 0)
	{
		modelRunIndex = 0;
	}

	//footsteps
	if (onPlane)
	{
		if (isBall ||
			isSpindashing ||
			spindashReleaseTimer > 0)
		{
			//if (MainGameLoop.gameClock % 2 == 0)
			{
				//spawnFootsetpParticles();
			}
		}
		else if ((modelRunIndex < 10 && modelRunIndex + modelIncreaseVal >= 10 ||
			      modelRunIndex < 20 && modelRunIndex + modelIncreaseVal >= 20))
		{
			//spawnFootsetpParticles();
			if (getY() > -0.5f)
			{
				//AudioSources.play(Math.max(triCol.sound, 0), getPosition(), 0.8f + mySpeed*0.05f + (float)Math.random()*0.4f);
			}
			else
			{
				//AudioSources.play(4, getPosition(), 0.8f + mySpeed*0.05f + (float)Math.random()*0.4f);
			}
		}
	}


	if (homingAttackTimer > 0)
	{
		float height = 2;
		Vector3f offset(currNorm.x*height, currNorm.y*height, currNorm.z*height);
		Vector3f prevPos(previousDisplayPos);
		prevPos = prevPos + offset;
		Vector3f newPos(displayPos);
		newPos = newPos + offset;
		//createSpindashTrails(prevPos, newPos, 5, 20);
	}

	if (isStomping)
	{
		float h = sqrtf(xVelAir*xVelAir + zVelAir*zVelAir);
		float zr = (toDegrees(atan2f(yVel, h)));
		if (myBody != nullptr) myBody->setBaseOrientation(&displayPos, 0, twistAngle, pitchAngle, zr + 90);
		if (Player::maniaSonic != nullptr) { Player::maniaSonic->setVisible(false); }

		float height = 2;
		Vector3f offset(currNorm.x*height, currNorm.y*height, currNorm.z*height);
		Vector3f prevPos(previousDisplayPos);
		prevPos = prevPos + offset;
		Vector3f newPos(displayPos);
		newPos = newPos + offset;
		//createSpindashTrails(prevPos, newPos, 5, 20);
		updateLimbs(3, 100);
	}
	else if (isBouncing)
	{
		if (myBody != nullptr) myBody->setBaseOrientation(&displayPos, 0, twistAngle, pitchAngle, -count * 60);
		if (Player::maniaSonic != nullptr)
		{
			Player::maniaSonic->setOrientation(dspX, dspY, dspZ, 0, twistAngle, pitchAngle, -count*60);
			Player::maniaSonic->animate(12, 0);
			setLimbsVisibility(false);
		}
		float height = 2;
		Vector3f offset(currNorm.x*height, currNorm.y*height, currNorm.z*height);
		Vector3f prevPos(previousDisplayPos);
		prevPos = prevPos + offset;
		Vector3f newPos(displayPos);
		newPos = newPos + offset;
		//createSpindashTrails(prevPos, newPos, 5, 20);
		updateLimbs(12, 0);
	}
	else if (isJumping)
	{
		if (myBody != nullptr) myBody->setBaseOrientation(&displayPos, 0, twistAngle, pitchAngle, -count * 35);
		if (Player::maniaSonic != nullptr)
		{
			Player::maniaSonic->setOrientation(dspX, dspY, dspZ, 0, twistAngle, pitchAngle, -count*35);
			Player::maniaSonic->animate(12, 0);
			setLimbsVisibility(false);
		}
		updateLimbs(12, 0);
	}
	else if (isBall)
	{
		if (myBody != nullptr) myBody->setBaseOrientation(dspX, dspY, dspZ, diff, yawAngle, pitchAngle, -count * 70);
		if (Player::maniaSonic != nullptr)
		{
			Player::maniaSonic->setOrientation(dspX, dspY, dspZ, diff, yawAngle, pitchAngle, -count*70);
			Player::maniaSonic->animate(12, 0);
			setLimbsVisibility(false);
		}
		float height = 2;
		Vector3f offset(currNorm.x*height, currNorm.y*height, currNorm.z*height);
		Vector3f prevPos(previousDisplayPos);
		prevPos = prevPos + offset;
		Vector3f newPos(displayPos);
		newPos = newPos + offset;
		//createSpindashTrails(prevPos, newPos, 5, 20);
		updateLimbs(12, 0);
	}
	else if (isSpindashing)
	{
		float twistAngleNew = toDegrees(spindashAngle);
		float diffNew = compareTwoAngles(twistAngleNew, yawAngle);

		setRotY(toDegrees(spindashAngle));
		float zrotoff = -(spindashTimer*spindashTimer*0.8f);
		if (spindashTimer >= spindashTimerMax)
		{
			zrotoff = -(count * 50);
		}
		if (myBody != nullptr) myBody->setBaseOrientation(dspX, dspY, dspZ, diffNew, yawAngle, pitchAngle, zrotoff);
		updateLimbs(12, 0);
		if (Player::maniaSonic != nullptr)
		{
			Player::maniaSonic->setOrientation(dspX, dspY, dspZ, diffNew, yawAngle, pitchAngle, zrotoff);
			setLimbsVisibility(false);
			Player::maniaSonic->animate(12, 0);
		}
	}
	else if (spindashReleaseTimer > 0)
	{
		float zrotoff = (spindashReleaseTimer*spindashReleaseTimer*0.8f);
																		  //zrotoff = -count*40; //different look, might look better?
		if (myBody != nullptr) myBody->setBaseOrientation(dspX, dspY, dspZ, diff, yawAngle, pitchAngle, zrotoff);
		updateLimbs(12, 0);
		if (Player::maniaSonic != nullptr)
		{
			Player::maniaSonic->setOrientation(dspX, dspY, dspZ, diff, yawAngle, pitchAngle, zrotoff);
			setLimbsVisibility(false);
			Player::maniaSonic->animate(12, 0);
		}
	}
	else if (onPlane && mySpeed < 0.01)
	{
		if (Player::maniaSonic != nullptr) { Player::maniaSonic->setVisible(false); }
		float time = (float)fmod((count * 1.0f), 100);
		updateLimbs(0, time);
	}
	else if (isSkidding)
	{
		if (Player::maniaSonic != nullptr) { Player::maniaSonic->setVisible(false); }
		if (myBody != nullptr) myBody->setBaseOrientation(&displayPos, diff, yawAngle, pitchAngle, 0);
		updateLimbs(8, 0);
	}
	else if (hitTimer > 0)
	{
		if (myBody != nullptr) myBody->setBaseOrientation(&displayPos, diff, yawAngle, pitchAngle, 0);
		if (Player::maniaSonic != nullptr) { Player::maniaSonic->setVisible(false); }
		updateLimbs(11, 0);
	}
	else
	{
		if (myBody != nullptr) myBody->setBaseOrientation(&displayPos, diff, yawAngle, pitchAngle, 0);
		float time = 10 * modelRunIndex * 0.5f;
		updateLimbs(1, time);
		if (Player::maniaSonic != nullptr)
		{
			Player::maniaSonic->setOrientation(dspX, dspY, dspZ, diff, yawAngle, pitchAngle, 0);
			setLimbsVisibility(false);

			if (mySpeed < 3.9f)
			{
				Player::maniaSonic->animate(15, time);
			}
			else
			{
				Player::maniaSonic->animate(1, time);
			}
		}
	}
	
	//switch (MainGameLoop.levelID)
	//{
	//case MainGameLoop.levelIDs.SHD:
		//float radius2 = MainGameLoop.snowRadius * 2;
		//float radius = MainGameLoop.snowRadius;
		//float basex = getX() - radius;
		//float basey = getY();
		//float basez = getZ() - radius;
		//int density = MainGameLoop.snowDensity;
		//for (int i = 0; i < density; i++)
		//{
			//new Particle(ParticleResources.textureSnowball,
				//new Vector3f(basex + radius2*(float)Math.random(),
					//basey + radius*(float)Math.random(),
					//basez + radius2*(float)Math.random()),
				//new Vector3f(0.25f*7.5f, -0.4f*7.5f, 0.15f*7.5f), 0, 80, 0, (float)Math.random() + 0.75f, -0.02f);//original y vel = -0.5
		//}
		//break;

	//default:
		//break;
	//}

	previousDisplayPos.set(&displayPos);
	updateLimbsMatrix();
}

void Player::goUp()
{
	yVel += 0.5f;
}

bool Player::isPlayer()
{
	return true;
}

void Player::setGroundSpeed(float newXspd, float newZspd)
{
	xVelGround = newXspd;
	zVelGround = newZspd;
}

float Player::getxVel()
{
	return xVel;
}

void Player::setxVel(float xVel)
{
	this->xVel = xVel;
}

float Player::getyVel()
{
	return yVel;
}

void Player::setyVel(float yVel)
{
	this->yVel = yVel;
}

float Player::getzVel()
{
	return zVel;
}

void Player::setzVel(float zVel)
{
	this->zVel = zVel;
}

void Player::setxVelAir(float xVelAir)
{
	this->xVelAir = xVelAir;
}

float Player::getXVelAir()
{
	return xVelAir;
}

void Player::setzVelAir(float zVelAir)
{
	this->zVelAir = zVelAir;
}

float Player::getZVelAir()
{
	return zVelAir;
}

void Player::setHoverCount(int newCount)
{
	hoverCount = newCount;
}

void Player::setOnPlane(bool on)
{
	onPlane = on;
}

float Player::getHitboxHorizontal()
{
	return 6;
}

float Player::getHitboxVertical()
{
	return 6;
}

void Player::stopMoving()
{
	xVel = 0;
	yVel = 0;
	zVel = 0;
	xVelAir = 0;
	zVelAir = 0;
	xVelGround = 0;
	zVelGround = 0;
}

void Player::setInWater(float height)
{
	inWater = true;
	waterHeight = height;
}

void Player::increaseGroundSpeed(float dx, float dz)
{
	xVelGround += dx;
	zVelGround += dz;
}

float Player::getSpeed()
{
	float xSpd = xVel + xVelAir + xVelGround;
	float zSpd = zVel + zVelAir + zVelGround;

	return sqrtf(xSpd*xSpd + zSpd*zSpd);
}

void Player::takeDamage(Vector3f* damageSource)
{
	if (iFrame == 0)
	{
		float xDiff = damageSource->x - getX();
		float zDiff = damageSource->z - getZ();
		float newDirection = atan2f(zDiff, -xDiff);
		xVelAir = (1.5f*cos(newDirection));
		zVelAir = (-1.5f*sin(newDirection));
		xVel = 0;
		zVel = 0;
		xVelGround = 0;
		zVelGround = 0;
		onPlane = false;
		yVel = 1.5f;
		iFrame = 120;
		hitTimer = 120;
		isJumping = false;
		isSpindashing = false;
		isSkidding = false;
		isBall = false;
		isBouncing = false;
		isStomping = false;
		isLightdashing = false;
		spindashReleaseTimer = 0;
		spindashRestartDelay = 0;

		int ringsToScatter = Global::gameRingCount;
		Global::gameRingCount = 0;

		if (ringsToScatter == 0)
		{
			die();
		}
		else
		{
			AudioPlayer::play(10, getPosition());
		}
		while (ringsToScatter > 0)
		{
			float spoutSpd = 3.5f;
			float anglH = (float)(M_PI * 2 * random());
			float anglV = (toRadians(nextGaussian() * 42 + 90));

			float yspd = (spoutSpd*sin(anglV));
			float hpt = (spoutSpd*cos(anglV));

			float xspd = (hpt*cos(anglH));
			float zspd = (hpt*sin(anglH));

			Ring* ring = new Ring(getX(), getY()+5, getZ(), xspd, yspd, zspd);
			Global::countNew++;

			Main_addEntity(ring);

			ringsToScatter--;
		}
	}
}

void Player::rebound(Vector3f* source)
{
	if (onPlane == false)
	{
		if (characterID == 2) //Mecha Sonic
		{
			yVel = 2.1f;
			xVelAir = 0;
			zVelAir = 0;
			setX(source->x);
			setZ(source->z);
			setY(source->y + 3.5f);
			homingAttackTimer = -1;
			hoverCount = hoverLimit / 2;
		}
		else
		{
			if (yVel >= 0) //no rebound
			{
				yVel += 1;
			}
			else if (jumpInput) //rebound
			{
				yVel = 0.2f + yVel*-1;
				if (yVel < 2)
				{
					yVel = 2;
				}
			}
			else //rebound
			{
				yVel = yVel*-1;
				if (yVel > 1)
				{
					yVel = 1;
				}
			}
			homingAttackTimer = -1;
		}
	}
}

bool Player::isVulnerable()
{
	return !(homingAttackTimer > 0 ||
		isBouncing ||
		isJumping ||
		isBall ||
		isSpindashing ||
		isStomping);
}

void Player::die()
{
	if (deadTimer == -1)
	{
		AudioPlayer::play(9, getPosition());
		deadTimer = 180;
	}
}