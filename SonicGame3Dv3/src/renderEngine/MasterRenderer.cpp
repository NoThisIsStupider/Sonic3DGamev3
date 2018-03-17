#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../entities/light.h"
#include "../entities/camera.h"
#include "../shaders/shaders.h"
#include "../entities/entities.h"
#include "../models/models.h"
#include "renderEngine.h"
#include "../toolbox/maths.h"
#include "../toolbox/matrix.h"
#include "../engineTester/main.h"
#include "skymanager.h"
#include "../water/waterrenderer.h"
#include "../particles/particlemaster.h"
#include "../shadows/shadowmapmasterrenderer.h"

#include <iostream>
#include <list>
#include <unordered_map>
#include <stdexcept>

ShaderProgram* shader;
EntityRenderer* renderer;
ShadowMapMasterRenderer* shadowMapRenderer;

std::unordered_map<TexturedModel*, std::list<Entity*>> entitiesMap;
std::unordered_map<TexturedModel*, std::list<Entity*>> entitiesMapPass2;
std::unordered_map<TexturedModel*, std::list<Entity*>> entitiesTransparentMap;

Matrix4f* projectionMatrix;

float VFOV = 60; //Vertical fov
const float NEAR_PLANE = 0.5f;
const float FAR_PLANE = 15000;

float RED = 0.9f;
float GREEN = 0.95f;
float BLUE = 1.0f;

void prepare();
void prepareTransparentRender();

void Master_init()
{
	if (Global::renderShadowsFar)
	{
		shader = new ShaderProgram("res/Shaders/entity/vertexShaderShadowFar.txt", "res/Shaders/entity/fragmentShaderShadowFar.txt");
	}
	else
	{
		shader = new ShaderProgram("res/Shaders/entity/vertexShader.txt", "res/Shaders/entity/fragmentShader.txt");
	}
	Global::countNew++;
	projectionMatrix = new Matrix4f;
	Global::countNew++;
	renderer = new EntityRenderer(shader, projectionMatrix);
	Master_makeProjectionMatrix();
	Global::countNew++;

	//if (Global::renderShadowsFar)
	{
		shadowMapRenderer = new ShadowMapMasterRenderer; Global::countNew++;
	}

	Master_disableCulling();
}

void Master_render(Camera* camera, float clipX, float clipY, float clipZ, float clipW)
{
	prepare();
	shader->start();
	shader->loadClipPlane(clipX, clipY, clipZ, clipW);
	RED = SkyManager::getFogRed();
	GREEN = SkyManager::getFogGreen();
	BLUE = SkyManager::getFogBlue();
	shader->loadSkyColour(RED, GREEN, BLUE);
	shader->loadLight(Global::gameLightSun);
	shader->loadViewMatrix(camera);
	shader->connectTextureUnits();

	renderer->renderNEW(&entitiesMap, shadowMapRenderer->getToShadowMapSpaceMatrix());
	renderer->renderNEW(&entitiesMapPass2, shadowMapRenderer->getToShadowMapSpaceMatrix());

	prepareTransparentRender();
	renderer->renderNEW(&entitiesTransparentMap, shadowMapRenderer->getToShadowMapSpaceMatrix());

	shader->stop();
}

void Master_processEntity(Entity* entity)
{
	if (entity->getVisible() == false)
	{
		return;
	}

	std::list<TexturedModel*>* modellist = entity->getModels();
	for (TexturedModel* entityModel : (*modellist))
	{
		std::list<Entity*>* list = &entitiesMap[entityModel];
		list->push_back(entity);
	}
}

void Master_processEntityPass2(Entity* entity)
{
	if (entity->getVisible() == false)
	{
		return;
	}

	std::list<TexturedModel*>* modellist = entity->getModels();
	for (TexturedModel* entityModel : (*modellist))
	{
		std::list<Entity*>* list = &entitiesMapPass2[entityModel];
		list->push_back(entity);
	}
}

void Master_processTransparentEntity(Entity* entity)
{
	if (entity->getVisible() == false)
	{
		return;
	}

	std::list<TexturedModel*>* modellist = entity->getModels();
	for (TexturedModel* entityModel : (*modellist))
	{
		std::list<Entity*>* list = &entitiesTransparentMap[entityModel];
		list->push_back(entity);
	}
}

void Master_clearEntities()
{
	entitiesMap.clear();
}

void Master_clearEntitiesPass2()
{
	entitiesMapPass2.clear();
}

void Master_clearTransparentEntities()
{
	entitiesTransparentMap.clear();
}

void prepare()
{
	glEnable(GL_MULTISAMPLE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(true);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(RED, GREEN, BLUE, 1);

	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, Master_getShadowMapTexture());
}

void prepareTransparentRender()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(false);
}

void Master_cleanUp()
{
	shader->cleanUp();
	delete shader;
	Global::countDelete++;
	delete renderer;
	Global::countDelete++;
	delete projectionMatrix;
	Global::countDelete++;

	shadowMapRenderer->cleanUp();
	delete shadowMapRenderer;
	Global::countDelete++;
}

void Master_enableCulling()
{
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
}

void Master_disableCulling()
{
	glDisable(GL_CULL_FACE);
}

extern unsigned int SCR_WIDTH;
extern unsigned int SCR_HEIGHT;

void Master_makeProjectionMatrix()
{
	int displayWidth;
	int displayHeight;
	glfwGetWindowSize(getWindow(), &displayWidth, &displayHeight);

	float aspectRatio = (float)displayWidth / (float)displayHeight;


	//FOV = 50;
	float y_scale = (float)((1.0f / tan(toRadians(VFOV / 2.0f))));
	float x_scale = y_scale / aspectRatio;


	//FOV = 88.88888;
	//float x_scale = (float)((1.0f / tan(toRadians(HFOV / 2.0f))));
	//float y_scale = x_scale * aspectRatio;


	float frustum_length = FAR_PLANE - NEAR_PLANE;

	projectionMatrix->m00 = x_scale;
	projectionMatrix->m11 = y_scale;
	projectionMatrix->m22 = -((FAR_PLANE + NEAR_PLANE) / frustum_length);
	projectionMatrix->m23 = -1;
	projectionMatrix->m32 = -((2 * NEAR_PLANE * FAR_PLANE) / frustum_length);
	projectionMatrix->m33 = 0;

	renderer->updateProjectionMatrix(projectionMatrix);

	if (Global::renderParticles)
	{
		ParticleMaster::updateProjectionMatrix(projectionMatrix);
	}

	if (Global::useHighQualityWater && Global::gameWaterRenderer != nullptr)
	{
		Global::gameWaterRenderer->updateProjectionMatrix(projectionMatrix);
	}
}

Matrix4f* Master_getProjectionMatrix()
{
	return projectionMatrix;
}

float Master_getVFOV()
{
	return VFOV;
}

float Master_getNearPlane()
{
	return NEAR_PLANE;
}

float Master_getFarPlane()
{
	return FAR_PLANE;
}

GLuint Master_getShadowMapTexture()
{
	return shadowMapRenderer->getShadowMap();
}

ShadowMapMasterRenderer* Master_getShadowRenderer()
{
	return shadowMapRenderer;
}

void Master_renderShadowMaps(Light* sun)
{
	shadowMapRenderer->render(&entitiesMap, sun);
}
