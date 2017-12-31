#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "renderEngine.h"
#include "../entities/entities.h"
#include "../shaders/shaders.h"
#include "../toolbox/matrix.h"
#include "../toolbox/vector.h"
#include "../toolbox/maths.h"
#include "../models/models.h"

#include <iostream>
#include <unordered_map>
#include <list>

EntityRenderer::EntityRenderer(ShaderProgram* shader, Matrix4f* projectionMatrix)
{
	shader->start();
	shader->loadProjectionMatrix(projectionMatrix);
	shader->stop();
	this->shader = shader;
}

void EntityRenderer::renderNEW(std::unordered_map<TexturedModel*, std::list<Entity*>>* entitiesMap)
{
	for (auto entry : (*entitiesMap))
	{
		prepareTexturedModel(entry.first);
		std::list<Entity*>* entityList = &entry.second;

		for (Entity* entity : (*entityList))
		{
			prepareInstance(entity);
			glDrawElements(GL_TRIANGLES, (entry.first)->getRawModel()->getVertexCount(), GL_UNSIGNED_INT, 0);
		}
		unbindTexturedModel();
	}
}

void EntityRenderer::prepareTexturedModel(TexturedModel* model)
{
	RawModel* rawModel = (*model).getRawModel();
	glBindVertexArray((*rawModel).getVaoID());
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	ModelTexture* texture = model->getTexture();
	//if (texture->getHasTransparency() != 0)
	{
		//Master_disableCulling();
	}
	//else
	{
		//Master_enableCulling();
	}
	shader->loadFakeLighting(texture->getUsesFakeLighting());
	shader->loadShineVariables(texture->getShineDamper(), texture->getReflectivity());
	shader->loadTransparency(texture->getHasTransparency());
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, (*(*model).getTexture()).getID());
}

void EntityRenderer::unbindTexturedModel()
{
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glBindVertexArray(0);
}

void EntityRenderer::prepareInstance(Entity* entity)
{
	shader->loadTransformationMatrix(entity->getTransformationMatrix());
}

void EntityRenderer::render(Entity* entity, ShaderProgram* shader)
{
	if (entity->getVisible() == false)
	{
		return;
	}

	prepareInstance(entity);

	std::list<TexturedModel*>* models = entity->getModels();

	for (auto texturedModel : (*models))
	{
		RawModel* model = texturedModel->getRawModel();

		prepareTexturedModel(texturedModel);

		glDrawElements(GL_TRIANGLES, model->getVertexCount(), GL_UNSIGNED_INT, 0);

		unbindTexturedModel();
	}
}

void EntityRenderer::updateProjectionMatrix(Matrix4f* projectionMatrix)
{
	this->shader->start();
	this->shader->loadProjectionMatrix(projectionMatrix);
	this->shader->stop();
}