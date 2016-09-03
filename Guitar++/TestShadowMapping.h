#pragma once
#ifndef _TEST_SHADOW_MAP_H__
#define _TEST_SHADOW_MAP_H__
#include <GL/glew.h>
#include <GLFW/glfw3.h>

static class ShadowMapping {
	// Considera apenas uma fonte de luz.
	GLuint shadowMapTexture;

	// Este tamanho não pode ser maior que a resolução da window.
	// Para isto, deve-se usar FBO com Off-screen rendering.
	int shadowMapSize;

	GLfloat textureTrasnformS[4];
	GLfloat textureTrasnformT[4];
	GLfloat textureTrasnformR[4];
	GLfloat textureTrasnformQ[4];
private:
	void createDepthTexture() {
		//Create the shadow map texture
		glGenTextures(1, &shadowMapTexture);
		glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowMapSize, shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

		//Enable shadow comparison
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);

		//Shadow comparison should be true (ie not in shadow) if r<=texture
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

		//Shadow comparison should generate an INTENSITY result
		glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
	}

	void loadTextureTransform() {
		GLfloat lightProjectionMatrix[16];
		GLfloat lightViewMatrix[16];

		// Busca as matrizes de view e projection da luz
		glGetFloatv(GL_PROJECTION_MATRIX, lightProjectionMatrix);
		glGetFloatv(GL_MODELVIEW_MATRIX, lightViewMatrix);

		// Salva o estado da matrix mode.
		glPushAttrib(GL_TRANSFORM_BIT);
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();

		//Calculate texture matrix for projection
		//This matrix takes us from eye space to the light's clip space
		//It is postmultiplied by the inverse of the current view matrix when specifying texgen
		GLfloat biasMatrix[16] = { 0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.5f, 0.5f, 0.5f, 1.0f };	//bias from [-1, 1] to [0, 1]

		GLfloat textureMatrix[16];

		// Aplica as 3 matrizes em uma só, levando um fragmento em 3D para o espaço
		// canônico da câmera.
		glLoadMatrixf(biasMatrix);
		glMultMatrixf(lightProjectionMatrix);
		glMultMatrixf(lightViewMatrix);
		glGetFloatv(GL_TEXTURE_MATRIX, textureMatrix);

		// Separa as colunas em arrays diferentes por causa da opengl
		for (int i = 0; i<4; i++) {
			textureTrasnformS[i] = textureMatrix[i * 4];
			textureTrasnformT[i] = textureMatrix[i * 4 + 1];
			textureTrasnformR[i] = textureMatrix[i * 4 + 2];
			textureTrasnformQ[i] = textureMatrix[i * 4 + 3];
		}

		glPopMatrix();
		glPopAttrib();
	}

public:
	void enableDepthCapture() {
		// Protege o código anterior a esta função
		glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_LIGHTING_BIT | GL_VIEWPORT_BIT | GL_COLOR_BUFFER_BIT);

		// Se a textura ainda não tiver sido criada, crie
		if (!shadowMapTexture) createDepthTexture();

		// Seta a viewport com o mesmo tamanho da textura.
		// O tamanho da viewport não pode ser maior que o tamanho da tela.
		// SE for, deve-se usar offline rendering e FBOs.
		glViewport(0, 0, shadowMapSize, shadowMapSize);

		// Calcula a transformação do espaço de câmera para o espaço da luz
		// e armazena a transformação para ser utilizada no teste de sombra do rendering
		loadTextureTransform();

		// Habilita Offset para evitar flickering.
		// Desloca o mapa de altura 1.9 vezes + 4.00 para trás.
		glPolygonOffset(1.9, 4.00);
		glEnable(GL_POLYGON_OFFSET_FILL);

		// Flat shading for speed
		glShadeModel(GL_FLAT);
		// Disable Lighting for performance.
		glDisable(GL_LIGHTING);
		// Não escreve no buffer de cor, apenas no depth
		glColorMask(0, 0, 0, 0);
	}

	void disableDepthCapture() {
		// Copia o Depth buffer para a textura.
		glBindTexture(GL_TEXTURE_2D, shadowMapTexture);

		// SubTexture não realoca a textura toda, como faz o texture
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, shadowMapSize, shadowMapSize);

		// Limpa o Buffer de profundidade
		glClear(GL_DEPTH_BUFFER_BIT);

		// Retorna as configurações anteriores ao depthCapture
		glPopAttrib();
	}

	void enableShadowTest() {
		// Protege o código anterior a esta função
		glPushAttrib(GL_TEXTURE_BIT | GL_ENABLE_BIT);

		// Habilita a geração automática de coordenadas de textura do ponto de vista da câmera
		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
		glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
		glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);

		// Aplica a transformação destas coordenadas para o espaço da luz
		glTexGenfv(GL_S, GL_EYE_PLANE, textureTrasnformS);
		glTexGenfv(GL_T, GL_EYE_PLANE, textureTrasnformT);
		glTexGenfv(GL_R, GL_EYE_PLANE, textureTrasnformR);
		glTexGenfv(GL_Q, GL_EYE_PLANE, textureTrasnformQ);

		// Ativa
		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);
		glEnable(GL_TEXTURE_GEN_R);
		glEnable(GL_TEXTURE_GEN_Q);

		//Bind & enable shadow map texture
		//glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
	}

	void disableShadowTest() {
		// Retorna as configurações anteriores do programa
		glPopAttrib();
	}

	ShadowMapping(int resolution = 512) {
		shadowMapTexture = NULL;
		shadowMapSize = resolution;
	}
	virtual ~ShadowMapping() {
		shadowMapTexture = NULL;
		glDeleteTextures(1, &shadowMapTexture);
	}
};

#endif
