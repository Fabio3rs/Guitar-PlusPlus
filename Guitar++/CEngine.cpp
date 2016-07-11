#ifndef _CRT_SECURE_NO_WARNINGS
#define  _CRT_SECURE_NO_WARNINGS
#endif
#include <bass.h>
#include <bass_fx.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "CEngine.h"
#include <exception>
#include <stdexcept>

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

#include <GL/glu.h>
#include "internal.h"
#include <cmath>
#include <GL/GL.h>

/*
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
*/

void CALLBACK GetBPM_ProgressCallback(DWORD chan, float percent, void *user)
{
	std::cout << percent << "  " << chan << std::endl;


}

double CEngine::getChannelLength(unsigned int ch)
{
	return BASS_ChannelBytes2Seconds(ch, BASS_ChannelGetLength(ch, BASS_POS_BYTE));
}

float CEngine::getSoundBPM(unsigned int sound, double at, double interval)
{
	float result = 0.0;
	//static float r = 0.0;
//	static bool b = false;

	//if (!b)
	{
		result = BASS_FX_BPM_DecodeGet(sound, at, at + interval, 0, BASS_FX_BPM_BKGRND | BASS_FX_BPM_MULT2 | BASS_FX_FREESOURCE, (BPMPROGRESSPROC*)0, 0);
		//b = true;
	}

	//std::cout << "res " << result << std::endl;
	//BASS_FX_BPM_DecodeGet();

	return result;
}

void CEngine::clearAccmumaltionBuffer()
{
	glClear(GL_ACCUM_BUFFER_BIT);
}

void CEngine::addToAccumulationBuffer(double d)
{
	glAccum(GL_ACCUM, d);
}

void CEngine::retAccumulationBuffer(double d)
{
	glAccum(GL_RETURN, d);
}

CEngine &CEngine::engine(std::function <void(int, const std::string &e)> errfun)
{
	static CEngine eng(errfun);
	return eng;
}

/*const uint32_t CEngine::bitValues[32] = {
	1, 2, 4, 8, 16, 32, 64, 128,
	256, 512, 1024, 2048, 4096, 8192, 16384, 32768,
	65536, 131072, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x800000,
	0x1000000, 0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000
};*/

bool CEngine::pauseSoundStream(int handle){
	return BASS_ChannelPause(handle) != 0;
}

bool CEngine::setSoundVolume(int handle, float volume)
{
	return BASS_ChannelSetAttribute(handle, BASS_ATTRIB_VOL, volume);
}

CEngine::chdata CEngine::getChannelData(int handle)
{
	chdata result;
	int BANDS = 120;
	int b0 = 0;
	float peak = -10;

	int r = BASS_ChannelGetData(handle, result.data, BASS_DATA_FFT_INDIVIDUAL);


	std::cout << r << "   " << BASS_ChannelGetLevel(handle) << std::endl;
	/*for (int i = 0; i < 4; i++)
	{
		std::cout << r << "   " << result.data[i] << std::endl;
	}*/

	//std::cout << r << "   " << peak << std::endl;
	return result;
}

CEngine::chdata CEngine::getChannelData(int handle, int b)
{
	chdata result;
	int BANDS = 120;
	int b0 = 0;
	float peak = -10;
	float fft[1024];
	BASS_CHANNELINFO ci;
	BASS_ChannelGetInfo(handle, &ci);

	int r = BASS_ChannelGetData(handle, fft, BASS_DATA_FFT1024);

	for (int i = b; i < b + 4; i++)
	{
		result.data[i] = fft[b] * ci.chans;
	}

	return result;
}

std::deque<CEngine::Resolution> CEngine::getPossibleVideoModes()
{
	int count = 0;
	const GLFWvidmode* modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);
	std::deque<CEngine::Resolution> result;

	for (int i = 0; i < count; i++){
		Resolution newRes;
		newRes.width = modes[i].width;
		newRes.height = modes[i].height;
		newRes.redBits = modes[i].redBits;
		newRes.greenBits = modes[i].greenBits;
		newRes.blueBits = modes[i].blueBits;
		newRes.refreshRate = modes[i].refreshRate;

		result.push_back(newRes);
	}

	return result;
}

void CEngine::Rotate(double a, double x, double y, double z){
	glRotated(a, x, y, z);
}

void sub_50E0C1F8(double *a1)
{
	*(double *)a1 = 1.0;
	*(double *)(a1 + 32) = 0.0;
	*(double *)(a1 + 64) = 0.0;
	*(double *)(a1 + 96) = 0.0;
	*(double *)(a1 + 8) = 0.0;
	*(double *)(a1 + 72) = 0.0;
	*(double *)(a1 + 104) = 0.0;
	*(double *)(a1 + 16) = 0.0;
	*(double *)(a1 + 48) = 0.0;
	*(double *)(a1 + 112) = 0.0;
	*(double *)(a1 + 24) = 0.0;
	*(double *)(a1 + 56) = 0.0;
	*(double *)(a1 + 88) = 0.0;
	*(double *)(a1 + 40) = 1.0;
	*(double *)(a1 + 80) = 1.0;
	*(double *)(a1 + 120) = 1.0;
}

/*void CEngine::gluPerspective(double fovy, double aspect, double zNear, double zFar)
{
	double v4;
	double v5;
	const GLdouble *v6;
	double v7;
	GLdouble v8;
	double v9;
	double v10;
	double v11;
	double v12;
	double v13;
	double v14;

	v8 = fovy * 0.5 * 3.141592653589793 / 180.0;
	v7 = zFar - zNear;
	v4 = sin(v8);
	if (v7 != 0.0 && 0.0 != v4 && aspect != 0.0)
	{
		v5 = cos(v8) / v4;
		sub_50E0C1F8(&v9); // matrix set?
		v9 = v5 / aspect;
		v10 = v5;
		v11 = -((zNear + zFar) / v7);
		v12 = -1.0;
		v13 = zFar * (zNear * -2.0) / v7;
		v14 = 0.0;
		glMultMatrixd(&v7);
	}
}*/

void CEngine::matrixReset(){
	//glLoadIdentity();

	lastRenderAt[0] = 0.0;
	lastRenderAt[1] = 0.0;
	lastRenderAt[2] = 0.0;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	gluPerspective(45.0, (double)windowWidth / (double)windowHeight, 0.005, 1000.0);
	gluLookAt(eyex,
		eyey,
		eyez,
		centerx,
		centery,
		centerz,
		upx,
		upy,
		upz); 

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

double CEngine::updateRandomNoiseInterval(double interval){
	if (std::isnan(interval)){
		return updateNoiseInterval;
	}

	updateNoiseInterval = interval;
	return updateNoiseInterval;
}

int CEngine::getCRCKeyboardNoise(){
	noiseCRC32 = 0xFFFFFFFF;

	for (int i = 0, size = keyboardNoise.size(), temp; i < size; i++){
		temp = keyboardNoise[i];
		temp <<= i % 64;
		temp += 64 % (i + 1);
		noiseCRC32 ^= temp;
	}

	return noiseCRC32;
}

std::string CEngine::getKeyboardNoise(){
	keyboardNoise = "";

	for (int i = 1; i < GLFW_KEY_LAST; i++){
		keyboardNoise += (char)i;
		keyboardNoise += (char)getKey(i);
	}

	return keyboardNoise;
}

int CEngine::getKey(int key){
	return glfwGetKey((GLFWwindow*)window, key);
}

void CEngine::setColor(double r, double g, double b, double a)
{
	if (lrC != r || lgC != g || lbC != b || laC != a)
	{
		lrC = r;
		lgC = g;
		lbC = b;
		laC = a;
		glColor4d(r, g, b, a);
	}
}


double CEngine::getSoundTime(int handle)
{
	return BASS_ChannelBytes2Seconds(handle, BASS_ChannelGetPosition(handle, BASS_POS_BYTE));
}

void CEngine::setSoundTime(int handle, double time)
{
	BASS_ChannelSetPosition(handle, BASS_ChannelSeconds2Bytes(handle, time), BASS_POS_BYTE);
}

bool CEngine::loadMusicStream(const char *fileName, int &handle)
{
	handle = BASS_MusicLoad(false, fileName, (QWORD)MAKELONG(0, 0), 0, BASS_STREAM_PRESCAN | BASS_ASYNCFILE, 0);


	std::cout << BASS_ErrorGetCode() << std::endl;

	return BASS_ChannelSetPosition(handle, (QWORD)MAKELONG(0, 0), BASS_POS_BYTE) && handle != 0;
}

bool CEngine::loadSoundStream(const char *fileName, int &handle, bool decode)
{
	int flags = BASS_STREAM_PRESCAN | BASS_ASYNCFILE;

	if (decode)
	{
		flags |= BASS_STREAM_DECODE;
	}

	handle = BASS_StreamCreateFile(false, fileName, 0, 0, flags);
	return BASS_ChannelSetPosition(handle, (QWORD)MAKELONG(0, 0), BASS_POS_BYTE) && (handle != 0);
}

bool CEngine::unloadSoundStream(int &handle)
{
	auto r = BASS_StreamFree(handle);
	handle = -1;
	return r;
}


bool CEngine::playSoundStream(int handle){
	return BASS_ChannelPlay(handle, false) != 0;
}

double CEngine::getTime(){
	return glfwGetTime();
}

void CEngine::setCamera(const cameraSET &cam){
	setCamera(cam.eyex,
		cam.eyey,
		cam.eyez,
		cam.centerx,
		cam.centery,
		cam.centerz,
		cam.upx,
		cam.upy,
		cam.upz);
}

void CEngine::setCamera(double eyex,
	double eyey,
	double eyez,
	double centerx,
	double centery,
	double centerz,
	double upx,
	double upy,
	double upz)
{
	this->eyex = eyex;
	this->eyey = eyey;
	this->eyez = eyez;
	this->centerx = centerx;
	this->centery = centery;
	this->centerz = centerz;
	this->upx = upx;
	this->upy = upy;
	this->upz = upz;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	gluPerspective(45.0, (double)windowWidth / (double)windowHeight, 0.005, 1000.0);
	gluLookAt(engine().eyex,
		engine().eyey,
		engine().eyez,
		engine().centerx,
		engine().centery,
		engine().centerz,
		engine().upx,
		engine().upy,
		engine().upz); /* positive Y up vector */

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

static void windowCallBack(GLFWwindow *window, int w, int h){
	if (h == 0) h = 1;
	if (w == 0) w = 1;

	auto &engine = CEngine::engine();

	if (engine.getWindowCallbackFunction())
		engine.getWindowCallbackFunction()(w, h, preUpdate);

	glViewport(0, 0, w, h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	engine.windowWidth = w;
	engine.windowHeight = h;

	gluPerspective(45.0, (double)w / (double)h, 0.005, 1000.0);

	gluLookAt(engine.eyex,
		engine.eyey,
		engine.eyez,
		engine.centerx,
		engine.centery,
		engine.centerz,
		engine.upx,
		engine.upy,
		engine.upz); /* positive Y up vector */

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if (engine.getWindowCallbackFunction())
		engine.getWindowCallbackFunction()(w, h, posUpdate);
}

/*void CEngine::setBitState(unsigned int *array, unsigned int bitSet, bool state){
	if (array){
		int index = bitSet >> 5, bit = bitValues[bitSet % 32];

		if ((array[index] & bit) && !state){
			::glDisable(bitSet);
			array[index] ^= bit;
		}
		else if (!(array[index] & bit) && state){
			::glEnable(bitSet);
			array[index] ^= bit;
		}
	}
	else{
		if (state){
			::glEnable(bitSet);
		}
		else{
			::glDisable(bitSet);
		}
	}
}*/

void CEngine::glEnable(int num){
	//if (num >= glStates.size())
	//	MessageBoxA(0, std::to_string(num).c_str(), "Erro", 0);

	//if (!glStates[num]){
		::glEnable(num);
		//glStates[num] = true;
	//}
}

void CEngine::glDisable(int num){
	//if (num >= glStates.size())
	//	MessageBoxA(0, std::to_string(num).c_str(), "Erro", 0);

	//if (glStates[num]){
		::glDisable(num);
	//	glStates[num] = false;
	//}
}

int CEngine::openFileStream(GLFWstream *stream, const char *name, const char *mode){
	memset(stream, 0, sizeof(GLFWstream));

	stream->file = fopen(name, mode);

	return stream->file != NULL;
}

void CEngine::closeStream(GLFWstream *stream){
	if (stream->file != NULL){
		fclose(stream->file);
	}

	memset(stream, 0, sizeof(GLFWstream));
}

int CEngine::seekStream(GLFWstream *stream, long offset, int whence){
	long position;

	if (stream->file != NULL)
	{
		if (fseek(stream->file, offset, whence) != 0)
		{
			return GL_FALSE;
		}

		return GL_TRUE;
	}

	if (stream->data != NULL)
	{
		position = offset;

		// Handle whence parameter
		if (whence == SEEK_CUR)
		{
			position += stream->position;
		}
		else if (whence == SEEK_END)
		{
			position += stream->size;
		}
		else if (whence != SEEK_SET)
		{
			return GL_FALSE;
		}

		// Clamp offset to buffer bounds and apply it
		if (position > stream->size)
		{
			stream->position = stream->size;
		}
		else if (position < 0)
		{
			stream->position = 0;
		}
		else
		{
			stream->position = position;
		}

		return GL_TRUE;
	}

	return GL_FALSE;
}

long CEngine::readStream(GLFWstream *stream, void *data, long size){
	if (stream->file != NULL)
	{
		return (long)fread(data, 1, size, stream->file);
	}

	if (stream->data != NULL)
	{
		// Check for EOF
		if (stream->position == stream->size)
		{
			return 0;
		}

		// Clamp read size to available data
		if (stream->position + size > stream->size)
		{
			size = stream->size - stream->position;
		}

		// Perform data read
		memcpy(data, (unsigned char*)stream->data + stream->position, size);
		stream->position += size;
		return size;
	}

	return 0;
}

long CEngine::tellStream(GLFWstream *stream){
	if (stream->file != NULL)
	{
		return ftell(stream->file);
	}

	if (stream->data != NULL)
	{
		return stream->position;
	}

	return 0;
}

int CEngine::readImage(const char *name, GLFWimage *img, int flags){
	GLFWstream stream;

	// Start with an empty image descriptor
	img->Width = 0;
	img->Height = 0;
	img->BytesPerPixel = 0;
	img->Data = NULL;

	// Open file
	if (!openFileStream(&stream, name, "rb"))
	{
		return GL_FALSE;
	}

	// We only support TGA files at the moment
	if (!_glfwReadTGA(&stream, img, flags))
	{
		closeStream(&stream);
		return GL_FALSE;
	}

	// Close stream
	closeStream(&stream);

	// Should we rescale the image to closest 2^N x 2^M resolution?
	/*if (!(flags & GLFW_NO_RESCALE_BIT))
	{
	if (!RescaleImage(img))
	{
	return GL_FALSE;
	}
	}*/

	// Interpret BytesPerPixel as an OpenGL format
	switch (img->BytesPerPixel)
	{
	default:
	case 1:
		if (flags & GLFW_ALPHA_MAP_BIT)
		{
			img->Format = GL_ALPHA;
		}
		else
		{
			img->Format = GL_LUMINANCE;
		}
		break;
	case 3:
		img->Format = GL_RGB;
		break;
	case 4:
		img->Format = GL_RGBA;
		break;
	}

	return GL_TRUE;
}

static int HalveImage(GLubyte *src, int *width, int *height,
	int components)
{
	int     halfwidth, halfheight, m, n, k, idx1, idx2;
	GLubyte *dst;

	// Last level?
	if (*width <= 1 && *height <= 1)
	{
		return GL_FALSE;
	}

	// Calculate new width and height (handle 1D case)
	halfwidth = *width > 1 ? *width / 2 : 1;
	halfheight = *height > 1 ? *height / 2 : 1;

	// Downsample image with a simple box-filter
	dst = src;
	if (*width == 1 || *height == 1)
	{
		// 1D case
		for (m = 0; m < halfwidth + halfheight - 1; m++)
		{
			for (k = 0; k < components; k++)
			{
				*dst++ = (GLubyte)(((int)*src +
					(int)src[components] + 1) >> 1);
				src++;
			}
			src += components;
		}
	}
	else
	{
		// 2D case
		idx1 = *width*components;
		idx2 = (*width + 1)*components;
		for (m = 0; m < halfheight; m++)
		{
			for (n = 0; n < halfwidth; n++)
			{
				for (k = 0; k < components; k++)
				{
					*dst++ = (GLubyte)(((int)*src +
						(int)src[components] +
						(int)src[idx1] +
						(int)src[idx2] + 2) >> 2);
					src++;
				}
				src += components;
			}
			src += components * (*width);
		}
	}

	// Return new width and height
	*width = halfwidth;
	*height = halfheight;

	return GL_TRUE;
}

int CEngine::loadTextureImage2D(GLFWimage *img, int flags){
	GLint   UnpackAlignment/*, GenMipMap*/;
	int     level, format, AutoGen, newsize, n;
	unsigned char *data, *dataptr;

	// TODO: Use GL_MAX_TEXTURE_SIZE or GL_PROXY_TEXTURE_2D to determine
	//       whether the image size is valid.
	// NOTE: May require box filter downsampling routine.

	// Do we need to convert the alpha map to RGBA format (OpenGL 1.0)?
	if ((glMajor == 1) && (glMinor == 0) &&
		(img->Format == GL_ALPHA))
	{
		// We go to RGBA representation instead
		img->BytesPerPixel = 4;

		// Allocate memory for new RGBA image data
		newsize = img->Width * img->Height * img->BytesPerPixel;
		data = (unsigned char *)malloc(newsize);
		if (data == NULL)
		{
			free(img->Data);
			return GL_FALSE;
		}

		// Convert Alpha map to RGBA
		dataptr = data;
		for (n = 0; n < (img->Width*img->Height); ++n)
		{
			*dataptr++ = 255;
			*dataptr++ = 255;
			*dataptr++ = 255;
			*dataptr++ = img->Data[n];
		}

		// Free memory for old image data (not needed anymore)
		free(img->Data);

		// Set pointer to new image data
		img->Data = data;
	}

	// Set unpack alignment to one byte
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &UnpackAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Should we use automatic mipmap generation?
	AutoGen = (flags & GLFW_BUILD_MIPMAPS_BIT)/* &&
											  _glfwWin.has_GL_SGIS_generate_mipmap*/;

	// Enable automatic mipmap generation
	/*if (AutoGen)
	{
	glGetTexParameteriv(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS,
	&GenMipMap);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS,
	GL_TRUE);
	}*/

	// Format specification is different for OpenGL 1.0
	if (glMajor == 1 && glMinor == 0)
	{
		format = img->BytesPerPixel;
	}
	else
	{
		format = img->Format;
	}

	// Upload to texture memeory
	level = 0;
	do
	{
		// Upload this mipmap level
		glTexImage2D(GL_TEXTURE_2D, level, format,
			img->Width, img->Height, 0, format,
			GL_UNSIGNED_BYTE, (void*)img->Data);

		// Build next mipmap level manually, if required
		if ((flags & GLFW_BUILD_MIPMAPS_BIT) && !AutoGen)
		{
			level = HalveImage(img->Data, &img->Width,
				&img->Height, img->BytesPerPixel) ?
				level + 1 : 0;
		}
	} while (level != 0);

	// Restore old automatic mipmap generation state
	/*if (AutoGen)
	{
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS,
	GenMipMap);
	}*/

	// Restore old unpack alignment
	glPixelStorei(GL_UNPACK_ALIGNMENT, UnpackAlignment);

	return GL_TRUE;
}

void CEngine::freeImage(GLFWimage *img){
	// Free memory
	if (img->Data != NULL)
	{
		free(img->Data);
		img->Data = NULL;
	}

	// Clear all fields
	img->Width = 0;
	img->Height = 0;
	img->Format = 0;
	img->BytesPerPixel = 0;
}

void CEngine::setScale(double x, double y, double z){
	glScaled(x, y, z);
}

void CEngine::RenderMulti3DQuad(const std::deque<RenderDoubleStruct> &quad3DData, unsigned int &bufferID){
	if (bufferID < 0)
		glGenBuffers(1, &bufferID);

	//glBindBuffer(GL_ARRAY_BUFFER_ARB, bufferID);

	int i = quad3DData.size();

	if (i <= 0)
		return;

	std::unique_ptr<GLdouble[]> vertexArray(new GLdouble[18 * i]);
	std::unique_ptr<GLdouble[]> textureArray(new GLdouble[12 * i]);

	for (int j = 0; j < i; j++){
		/*GLdouble vA[] = { quad3DData[i].x1, quad3DData[i].y1, quad3DData[i].z1,
			quad3DData[i].x2, quad3DData[i].y2, quad3DData[i].z2,
			quad3DData[i].x3, quad3DData[i].y3, quad3DData[i].z3,
			quad3DData[i].x3, quad3DData[i].y3, quad3DData[i].z3,
			quad3DData[i].x4, quad3DData[i].y4, quad3DData[i].z4,
			quad3DData[i].x1, quad3DData[i].y1, quad3DData[i].z1 };*/
		
		vertexArray[j * 18 + 0] = quad3DData[i].x1;
		vertexArray[j * 18 + 1] = quad3DData[i].y1;
		vertexArray[j * 18 + 2] = quad3DData[i].z1;
		vertexArray[j * 18 + 3] = quad3DData[i].x2;
		vertexArray[j * 18 + 4] = quad3DData[i].y2;
		vertexArray[j * 18 + 5] = quad3DData[i].z2;
		vertexArray[j * 18 + 6] = quad3DData[i].x3;
		vertexArray[j * 18 + 7] = quad3DData[i].y3;
		vertexArray[j * 18 + 8] = quad3DData[i].z3;
		vertexArray[j * 18 + 9] = quad3DData[i].x3;
		vertexArray[j * 18 + 10] = quad3DData[i].y3;
		vertexArray[j * 18 + 11] = quad3DData[i].z3;
		vertexArray[j * 18 + 12] = quad3DData[i].x4;
		vertexArray[j * 18 + 13] = quad3DData[i].y4;
		vertexArray[j * 18 + 14] = quad3DData[i].z4;
		vertexArray[j * 18 + 15] = quad3DData[i].x1;
		vertexArray[j * 18 + 16] = quad3DData[i].y1;
		vertexArray[j * 18 + 17] = quad3DData[i].z1;
	}

	for (int j = 0; j < i; j++){
		/*GLdouble tA[] = {
			quad3DData[i].TextureX1, quad3DData[i].TextureY1,
			quad3DData[i].TextureX2, quad3DData[i].TextureY1,
			quad3DData[i].TextureX2, quad3DData[i].TextureY2,
			quad3DData[i].TextureX2, quad3DData[i].TextureY2,
			quad3DData[i].TextureX1, quad3DData[i].TextureY2,
			quad3DData[i].TextureX1, quad3DData[i].TextureY1
		};*/

		textureArray[j * 12 + 0] = quad3DData[i].TextureX1;
		textureArray[j * 12 + 1] = quad3DData[i].TextureY1;
		textureArray[j * 12 + 2] = quad3DData[i].TextureX2;
		textureArray[j * 12 + 3] = quad3DData[i].TextureY1;
		textureArray[j * 12 + 4] = quad3DData[i].TextureX2;
		textureArray[j * 12 + 5] = quad3DData[i].TextureY2;
		textureArray[j * 12 + 6] = quad3DData[i].TextureX2;
		textureArray[j * 12 + 7] = quad3DData[i].TextureY2;
		textureArray[j * 12 + 8] = quad3DData[i].TextureX1;
		textureArray[j * 12 + 9] = quad3DData[i].TextureY2;
		textureArray[j * 12 + 10] = quad3DData[i].TextureX1;
		textureArray[j * 12 + 11] = quad3DData[i].TextureY1;
	}

	bindTexture(quad3DData[0].Text);
	/*glBufferDataARB(GL_ARRAY_BUFFER_ARB, i * 18 * sizeof(GLdouble), vertexArray.get(), GL_DYNAMIC_DRAW_ARB);

	glTexCoordPointer(2, GL_DOUBLE, 0, textureArray.get());
	glVertexPointer(3, GL_DOUBLE, 0, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6 * i);

	glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);*/
}

int CEngine::loadTexture2D(const char *name, int flags, GLFWimage *eimg){
	GLFWimage img;

	// Force rescaling if necessary
	/*if (!_glfwWin.has_GL_ARB_texture_non_power_of_two)
	{
	flags &= (~GLFW_NO_RESCALE_BIT);
	}*/

	// Read image from file
	if (!readImage(name, &img, flags))
	{
		return GL_FALSE;
	}

	if (!loadTextureImage2D(&img, flags))
	{
		return GL_FALSE;
	}

	if (eimg)
	{
		std::copy(&img, &img + 1, eimg);

		eimg->Data = nullptr;
	}

	// Data buffer is not needed anymore
	freeImage(&img);

	return GL_TRUE;
}

unsigned int CEngine::loadTexture(const char *textureFileName, GLFWimage *eimg){
	GLuint Text;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glGenTextures(1, &Text);
	glBindTexture(GL_TEXTURE_2D, Text);

	if (loadTexture2D(textureFileName, 0, eimg) && glIsTexture(Text)){
		return Text;
	}

	return 0;
}

double calcDist(double x1, double y1, double x2, double y2){
	double xr = x2 - x1, yr = y2 - y1;
	return sqrt(xr * xr + yr * yr);
}

void CEngine::renderFrame()
{
	if (renderFrameCallback)
		renderFrameCallback();

	RenderDoubleStruct cursorPointer;

	cursorPointer.x1 = mouseX;
	cursorPointer.x2 = mouseX + 0.1;
	cursorPointer.x3 = mouseX + 0.1;
	cursorPointer.x4 = mouseX;

	cursorPointer.y1 = mouseY + 0.1;
	cursorPointer.y2 = mouseY + 0.1;
	cursorPointer.y3 = mouseY;
	cursorPointer.y4 = mouseY;

	cursorPointer.TextureX1 = 0.0;
	cursorPointer.TextureX2 = 1.0;

	cursorPointer.TextureY1 = 1.0;
	cursorPointer.TextureY2 = 0.0;

	cursorPointer.Text = cursorText;

	Render2DQuad(cursorPointer);

	glfwSwapBuffers((GLFWwindow*)window);
	glfwPollEvents();

	DeltaTime = glfwGetTime() - lastFrameTime;
	lastFrameTime = glfwGetTime();

	tmpFPS++;

	if (lastFrameTime - lastFPSSwapTime >= 1.0){
		lastFPSSwapTime = glfwGetTime();
		FPS = tmpFPS;
		tmpFPS = 0;
	}

	if (glfwGetTime() > 0.1){
		double mx, my;

		glfwGetCursorPos((GLFWwindow*)window, &mx, &my);

		double proportion = (windowWidth / windowHeight);

		mouseX = (mx / windowWidth * 2.0 - 1.0) * proportion;
		mouseY = -((my / windowHeight * 2.0) - 1.0);
		/*
		if (mouseX > proportion)
			mouseX = proportion;

		if (mouseX < -proportion)
			mouseX = -proportion;

		if (mouseY > 1.0)
			mouseY = 1.0;

		if (mouseY < -1.0)
			mouseY = -1.0;*/

		//double x = windowWidth / 2.0, y = windowHeight / 2.0;
		//glfwSetCursorPos((GLFWwindow*)window, x, y);
	}

	/*
	if (getTime() - lastUpdatedNoise >= updateNoiseInterval){
		getKeyboardNoise();
		getCRCKeyboardNoise();

		lastUpdatedNoise = getTime();
	}*/
}

void CEngine::GLFWerrorfun(int error, const char *description)
{
	if (engine().errorCallbackFun)
		engine().errorCallbackFun(error, description);
	else
		MessageBoxA(0, description, std::to_string(error).c_str(), 0);
}

bool CEngine::windowOpened(){
	return !glfwWindowShouldClose((GLFWwindow*)window);
}

unsigned int CEngine::vboSET(size_t size, void *buffer)
{
	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, size, buffer, GL_STATIC_DRAW);

	return vertexbuffer;
}

void CEngine::attribVBOBuff(int id, int size, unsigned int buffer)
{
	// 1rst attribute buffer : vertices
	glEnableVertexAttribArray(id);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glVertexAttribPointer(
		id,                  // attribute
		size,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
		);
}

void CEngine::disableBuf(int id)
{
	glDisableVertexAttribArray(id);
}

void CEngine::bindTextOnSlot(int text, int slot)
{
	glActiveTexture(GL_TEXTURE0 + slot);
	bindTexture(text);
	// Set our "myTextureSampler" sampler to user Texture Unit 0
	glUniform1i(text, 0);
}

void CEngine::drawBufArrays(int size)
{
	glDrawArrays(GL_TRIANGLES, 0, size);
}

void CEngine::activateNormals(bool a)
{
	if (a)
		glEnableClientState(GL_NORMAL_ARRAY);
	else
		glDisableClientState(GL_NORMAL_ARRAY);
}

void CEngine::openWindow(const char *name, int w, int h, int fullScreen){
	GLFWmonitor *monitor = nullptr;
	if (fullScreen){
		int monitorCount;
		GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

		if (monitorCount > 0){
			monitor = *monitors;
		}
	}

	if (w < 1 || h < 1){
		const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

		w = mode->width;
		h = mode->height;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);

	//glfwWindowHint(GLFW_DECORATED, GL_FALSE);

	if (AASamples)
		glfwWindowHint(GLFW_SAMPLES, AASamples);

	window = glfwCreateWindow(w, h, name, monitor, NULL);

	if (!window)
	{
		GLFWerrorfun(-1, "Can't open GLFW window - verify your video's driver");
		throw std::logic_error("Can't open GLFW window - verify your video's driver");
	}

	glfwMakeContextCurrent((GLFWwindow*)window);

	glfwSetWindowSizeCallback((GLFWwindow*)window, windowCallBack);
	windowCallBack((GLFWwindow*)window, w, h);

	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		GLFWerrorfun(err, (char*)glewGetErrorString(err));
		MessageBoxA(0, (char*)glewGetErrorString(err), "Error: %s\n", 0);
	}

	if (glUseProgram)
	{
		glUseProgram(0);
	}
	else
	{
		GLFWerrorfun(err, "Something wrong happened");
		GLFWerrorfun(0, "glUseProgram GLEW_GET_FUN(__glewUseProgram) is 0");
	}


	glfwMakeContextCurrent((GLFWwindow*)window);


	glClearColor(0.0, 0.0, 0.0, 1.0);

	/*for (int i = 0, size = glStates.size(); i < size; i++){
		glStates[i] = glIsEnabled(i);
	}*/
	lightData l;

	for (auto &t : l.ambientLight)
	{
		t = 0.3;
	}

	for (auto &t : l.direction)
	{
		t = 2.5;
	}

	for (auto &t : l.position)
	{
		t = 0.0;
	}

	for (auto &t : l.specularLight)
	{
		t = 1.0;
	}

	for (auto &t : l.diffuseLight)
	{
		t = 1.0;
	}

	l.angle = 100.0;
	l.direction[0] = 0.0;
	l.direction[1] = -0.5;
	l.direction[2] = -5.0;

	l.position[3] = 1.0;
	l.position[1] = 0.0;
	l.position[2] = 2.5;
	//l.position[1] = -0.2;


	glEnable(GL_LIGHTING);

	glEnable(GL_LIGHT0);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, l.ambientLight);

	glLightfv(GL_LIGHT0, GL_AMBIENT, l.ambientLight);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, l.diffuseLight);
	glLightfv(GL_LIGHT0, GL_SPECULAR, l.specularLight);
	glLightfv(GL_LIGHT0, GL_EMISSION, l.specularLight);
	glLightfv(GL_LIGHT0, GL_POSITION, l.position);

	glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, l.angle);

	glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, l.direction);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	//glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);

	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);

	glDisable(GL_LIGHTING);

	//glCullFace(GL_BACK);

	cursorText = loadTexture("data/sprites/cursor.tga");
	glfwSetInputMode((GLFWwindow*)window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	//setVSyncMode(0);

	CEngine::engine().activateLighting(false);
}

void CEngine::setLight(const lightData &l, int id, bool setAmbient)
{
	if (setAmbient)
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, l.ambientLight);

	glLightfv(GL_LIGHT0 + id, GL_AMBIENT, l.ambientLight);
	glLightfv(GL_LIGHT0 + id, GL_DIFFUSE, l.diffuseLight);
	glLightfv(GL_LIGHT0 + id, GL_SPECULAR, l.specularLight);
	glLightfv(GL_LIGHT0 + id, GL_EMISSION, l.specularLight);
	glLightfv(GL_LIGHT0 + id, GL_POSITION, l.position);

	glLightf(GL_LIGHT0 + id, GL_SPOT_CUTOFF, l.angle);

	glLightfv(GL_LIGHT0 + id, GL_SPOT_DIRECTION, l.direction);
}

void CEngine::activateLight(int id, bool a)
{
	if (a)
		glEnable(GL_LIGHT0 + id);
	else
		glDisable(GL_LIGHT0 + id);
}

void CEngine::activateLighting(bool a)
{
	if (1)
	{
		if (a)
			glEnable(GL_LIGHTING);
		else
			glDisable(GL_LIGHTING);
	}
}

void CEngine::activate3DRender(bool a)
{
	if (a)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
}

void CEngine::useShader(unsigned int programID)
{
	glUseProgram(programID);
}

unsigned int CEngine::getUniformLocation(unsigned int programID, const char *str)
{
	return glGetUniformLocation(programID, str);;
}

void CEngine::setVSyncMode(int mode){
	glfwSwapInterval(mode);
}

int CEngine::getMouseButton(int btn){
	return glfwGetMouseButton((GLFWwindow*)window, btn);
}

void CEngine::bindTexture(unsigned int text){
	if (lastUsedTexture == text) return;

	lastUsedTexture = text;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, text);
}

void CEngine::clearScreen(){
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
}

void CEngine::draw2DLine(double *linedata, int points)
{
	bindTexture(0);
	glLineWidth(4);

	glBegin(GL_LINES);

	for (int i = 0; i < points; i++)
	{
		glVertex2d(linedata[i * 2 + 0], linedata[i * 2 + 1]);
		if (i > 0 && i < (points - 1))
		{
			glVertex2d(linedata[i * 2 + 0], linedata[i * 2 + 1]);
		}
	}

	glEnd();

}

void CEngine::Render2DQuad(const RenderDoubleStruct &quad2DData){
	GLdouble vertexArray[] = {
		quad2DData.x1, quad2DData.y1,
		quad2DData.x2, quad2DData.y2,
		quad2DData.x3, quad2DData.y3,
		quad2DData.x3, quad2DData.y3,
		quad2DData.x4, quad2DData.y4,
		quad2DData.x1, quad2DData.y1 };

	GLdouble textArray[] = {
		quad2DData.TextureX1, quad2DData.TextureY1,
		quad2DData.TextureX2, quad2DData.TextureY1,
		quad2DData.TextureX2, quad2DData.TextureY2,
		quad2DData.TextureX2, quad2DData.TextureY2,
		quad2DData.TextureX1, quad2DData.TextureY2,
		quad2DData.TextureX1, quad2DData.TextureY1 };

	//glTranslated(eyex, centery, 0.0);

	bindTexture(quad2DData.Text);

	glTexCoordPointer(2, GL_DOUBLE, 0, textArray);
	glVertexPointer(2, GL_DOUBLE, 0, vertexArray);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void CEngine::RenderCustomVerticesFloat(void *vertexPtr, void *uvPtr, void *normals, int count, unsigned int texture, unsigned int &vbuffer, unsigned int &uvbuffer, unsigned int &nvbuffer)
{
	if (vbuffer == (~0)){
		glGenBuffers(1, &vbuffer);
		glBindBuffer(GL_ARRAY_BUFFER, vbuffer);
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(float) * 3, vertexPtr, GL_STATIC_DRAW);

		glGenBuffers(1, &nvbuffer);
		glBindBuffer(GL_ARRAY_BUFFER, nvbuffer);
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(float) * 3, normals, GL_STATIC_DRAW);

		glGenBuffers(1, &uvbuffer);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(float) * 2, uvPtr, GL_STATIC_DRAW);
	}

	if (texture) bindTexture(texture);


}

void CEngine::Render2DCircleBufferMax(double x, double y, double perone, double radius, double lineWeight, int maxPolys, unsigned int &bufferID)
{
	if (perone > 1.0)
	{
		perone = 1.0;
	}

	double peroneA = perone;

	if (bufferID == (~0)){
		bufferID = circlesBuffer.size();

		auto &stream = circlesBuffer[bufferID];
		glGenBuffers(1, &stream.GlBuffer);
		stream.allocAll(maxPolys);

		perone = 1.0;
	}

	int polysNum = maxPolys * perone;

	auto &stream = circlesBuffer[bufferID];
	bool res = stream.isChangedMX(x, y, radius, lineWeight, maxPolys);

	auto &result = stream.verticesBuffer;

	glBindBuffer(GL_ARRAY_BUFFER_ARB, stream.GlBuffer);

	if (res){
		const long double PiValue = 3.1415926535897932384626433832795;

		RenderDoubleStruct quad2DData;

		double degrees = 360.0 * (perone);
		degrees = degrees / (360.0 / (PiValue * 2.0));
		double degreesStep = degrees / (double)polysNum;

		double planificatedSize = radius * 2.0 * PiValue;
		double planificatedSizeToRender = planificatedSize * (perone / 57.50);
		double planificatedSizeToRenderStep = planificatedSizeToRender / (double)polysNum;

		for (int i = 0; i < polysNum; i++){
			double degreesNow = degreesStep * (double)i;
			double planificatedSizeToRenderNow = planificatedSize;
			double sinValue = sin(degreesNow);
			double cosValue = cos(degreesNow);
			double lineWC = cosValue * radius * lineWeight;
			double lineWS = sinValue * radius * lineWeight;

			double degreesNowTwo = degreesStep * (i + 1.0);
			double sinValueTwo = sin(degreesNowTwo);
			double cosValueTwo = cos(degreesNowTwo);
			double planificatedSizeToRenderNowTwo = planificatedSize;
			double lineWCT = cosValueTwo * lineWeight;
			double lineWST = sinValueTwo * lineWeight;

			quad2DData.x1 = sinValue * (radius + lineWeight); // x1
			quad2DData.y1 = cosValue * (radius + lineWeight); // y1
			quad2DData.x2 = sinValueTwo * (radius + lineWeight); // x2
			quad2DData.y2 = cosValueTwo * (radius + lineWeight); // y2

			quad2DData.x3 = sinValueTwo * (radius); // x3
			quad2DData.y3 = cosValueTwo * (radius); // y3
			quad2DData.x4 = sinValue * (radius); // x4
			quad2DData.y4 = cosValue * (radius); // y4

			// Triangle 0
			result[i * 12] = quad2DData.x1 + x;
			result[i * 12 + 1] = quad2DData.y1 + y;
			result[i * 12 + 2] = quad2DData.x2 + x;
			result[i * 12 + 3] = quad2DData.y2 + y;
			result[i * 12 + 4] = quad2DData.x3 + x;
			result[i * 12 + 5] = quad2DData.y3 + y;

			// Triangle 1
			result[i * 12 + 6] = quad2DData.x3 + x;
			result[i * 12 + 7] = quad2DData.y3 + y;
			result[i * 12 + 8] = quad2DData.x4 + x;
			result[i * 12 + 9] = quad2DData.y4 + y;
			result[i * 12 + 10] = quad2DData.x1 + x;
			result[i * 12 + 11] = quad2DData.y1 + y;

			/*
			quad2DData.x1, quad2DData.y1,
			quad2DData.x2, quad2DData.y2,
			quad2DData.x3, quad2DData.y3,
			quad2DData.x3, quad2DData.y3,
			quad2DData.x4, quad2DData.y4,
			quad2DData.x1, quad2DData.y1*/
		}
	}

	polysNum = maxPolys * peroneA;

	bindTexture(0);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, polysNum * 12 * sizeof(GLdouble), result.get(), GL_STATIC_DRAW_ARB);

	//glBindBufferARB(GL_ARRAY_BUFFER_ARB, bufferID);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	//glTexCoordPointer(2, GL_DOUBLE, 0, result.get());
	glVertexPointer(2, GL_DOUBLE, 0, 0);

	glDrawArrays(GL_TRIANGLES, 0, polysNum * 6);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
}

void CEngine::Render2DCircle(double x, double y, double percent, double radius, double lineWeight, int polysNum, int maxPolys, unsigned int &bufferID){
	if (bufferID == (~0)){
		bufferID = circlesBuffer.size();

		auto &stream = circlesBuffer[bufferID];
		glGenBuffers(1, &stream.GlBuffer);
		stream.allocAll(maxPolys);
	}

	auto &stream = circlesBuffer[bufferID];
	bool res = stream.isChanged(x, y, percent, radius, lineWeight, polysNum);

	auto &result = stream.verticesBuffer;

	glBindBuffer(GL_ARRAY_BUFFER_ARB, stream.GlBuffer);

	if (res){
		const long double PiValue = 3.1415926535897932384626433832795;

		RenderDoubleStruct quad2DData;

		double degrees = 360.0 * (percent / 100.0);
		degrees = degrees / (360.0 / (PiValue * 2.0));
		double degreesStep = degrees / (double)polysNum;

		double planificatedSize = radius * 2 * PiValue;
		double planificatedSizeToRender = planificatedSize * (percent / 5750.0);
		double planificatedSizeToRenderStep = planificatedSizeToRender / (double)polysNum;

		for (int i = 0; i < polysNum; i++){
			double degreesNow = degreesStep * (double)i;
			double planificatedSizeToRenderNow = planificatedSize;
			double sinValue = sin(degreesNow);
			double cosValue = cos(degreesNow);
			double lineWC = cosValue * radius * lineWeight;
			double lineWS = sinValue * radius * lineWeight;

			double degreesNowTwo = degreesStep * (i + 1.0);
			double sinValueTwo = sin(degreesNowTwo);
			double cosValueTwo = cos(degreesNowTwo);
			double planificatedSizeToRenderNowTwo = planificatedSize;
			double lineWCT = cosValueTwo * lineWeight;
			double lineWST = sinValueTwo * lineWeight;

			quad2DData.x1 = sinValue * (radius + lineWeight); // x1
			quad2DData.y1 = cosValue * (radius + lineWeight); // y1
			quad2DData.x2 = sinValueTwo * (radius + lineWeight); // x2
			quad2DData.y2 = cosValueTwo * (radius + lineWeight); // y2

			quad2DData.x3 = sinValueTwo * (radius); // x3
			quad2DData.y3 = cosValueTwo * (radius); // y3
			quad2DData.x4 = sinValue * (radius); // x4
			quad2DData.y4 = cosValue * (radius); // y4

			// Triangle 0
			result[i * 12] = quad2DData.x1 + x;
			result[i * 12 + 1] = quad2DData.y1 + y;
			result[i * 12 + 2] = quad2DData.x2 + x;
			result[i * 12 + 3] = quad2DData.y2 + y;
			result[i * 12 + 4] = quad2DData.x3 + x;
			result[i * 12 + 5] = quad2DData.y3 + y;

			// Triangle 1
			result[i * 12 + 6] = quad2DData.x3 + x;
			result[i * 12 + 7] = quad2DData.y3 + y;
			result[i * 12 + 8] = quad2DData.x4 + x;
			result[i * 12 + 9] = quad2DData.y4 + y;
			result[i * 12 + 10] = quad2DData.x1 + x;
			result[i * 12 + 11] = quad2DData.y1 + y;

			/*
			quad2DData.x1, quad2DData.y1,
			quad2DData.x2, quad2DData.y2,
			quad2DData.x3, quad2DData.y3,
			quad2DData.x3, quad2DData.y3,
			quad2DData.x4, quad2DData.y4,
			quad2DData.x1, quad2DData.y1*/
		}
	}

	bindTexture(0);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, polysNum * 12 * sizeof(GLdouble), result.get(), GL_DYNAMIC_DRAW_ARB);

	//glBindBufferARB(GL_ARRAY_BUFFER_ARB, bufferID);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	//glTexCoordPointer(2, GL_DOUBLE, 0, result.get());
	glVertexPointer(2, GL_DOUBLE, 0, 0);

	glDrawArrays(GL_TRIANGLES, 0, polysNum * 6);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindBuffer(GL_ARRAY_BUFFER_ARB, 0);
	//glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
}

void CEngine::bindVBOBuffer(unsigned int buffer)
{
	if (lastUsedVBOBuffer == buffer) return;

	lastUsedVBOBuffer = buffer;
	glBindBuffer(GL_ARRAY_BUFFER_ARB, buffer);
}

void CEngine::RenderCustomVerticesFloat(staticDrawBuffer &buffer, bool autoBindZero)
{
	if (buffer.bufferID == (~0))
	{
		glGenBuffers(1, &buffer.bufferID);

		glBindBuffer(GL_ARRAY_BUFFER_ARB, buffer.bufferID);

		glBufferDataARB(GL_ARRAY_BUFFER_ARB, buffer.sizebytes, buffer.pointer, GL_STATIC_DRAW_ARB);
	}

	bindVBOBuffer(buffer.bufferID);
	if (buffer.texture) bindTexture(buffer.texture);

	glNormalPointer(GL_FLOAT, 0, (void*)buffer.normalsL);

	glTexCoordPointer(2, GL_FLOAT, 0, (void*)buffer.uvL);
	glVertexPointer(3, GL_FLOAT, 0, (void*)buffer.vertexL);

	glDrawArrays(GL_TRIANGLES, 0, buffer.count);
	if (autoBindZero) bindVBOBuffer(0);
}

void CEngine::RenderCustomVerticesFloat(void *vertexPtr, void *uvPtr, void *normals, int count, unsigned int texture)
{
	if (texture) bindTexture(texture);

	//glTranslated(0.0, 0.0, 0.0);

	if (normals)
		glNormalPointer(GL_FLOAT, 0, normals);

	glTexCoordPointer(2, GL_FLOAT, 0, uvPtr);
	glVertexPointer(3, GL_FLOAT, 0, vertexPtr);

	glDrawArrays(GL_TRIANGLES, 0, count);
}

void CEngine::renderAt(double x, double y, double z)
{
	glTranslated(x - lastRenderAt[0], y - lastRenderAt[1], z - lastRenderAt[2]);

	lastRenderAt[0] = x;
	lastRenderAt[1] = y;
	lastRenderAt[2] = z;
}

void CEngine::Render3DQuad(const RenderDoubleStruct &quad3DData){
	GLdouble vertexArray[] = {
		quad3DData.x1, quad3DData.y1, quad3DData.z1,
		quad3DData.x2, quad3DData.y2, quad3DData.z2,
		quad3DData.x3, quad3DData.y3, quad3DData.z3,
		quad3DData.x3, quad3DData.y3, quad3DData.z3,
		quad3DData.x4, quad3DData.y4, quad3DData.z4,
		quad3DData.x1, quad3DData.y1, quad3DData.z1 };

	GLdouble textArray[] = {
		quad3DData.TextureX1, quad3DData.TextureY1,
		quad3DData.TextureX2, quad3DData.TextureY1,
		quad3DData.TextureX2, quad3DData.TextureY2,
		quad3DData.TextureX2, quad3DData.TextureY2,
		quad3DData.TextureX1, quad3DData.TextureY2,
		quad3DData.TextureX1, quad3DData.TextureY1 };

	bindTexture(quad3DData.Text);

	//glTranslated(0.0, 0.0, 0.0);

	glTexCoordPointer(2, GL_DOUBLE, 0, textArray);
	glVertexPointer(3, GL_DOUBLE, 0, vertexArray);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void CEngine::pushQuad(dTriangleWithAlpha &arr, const RenderDoubleStruct &quad3DData)
{
	dTriangleWithAlpha nt;

	GLdouble vertexArray[] = {
		quad3DData.x1, quad3DData.y1, quad3DData.z1,
		quad3DData.x2, quad3DData.y2, quad3DData.z2,
		quad3DData.x3, quad3DData.y3, quad3DData.z3,
		quad3DData.x3, quad3DData.y3, quad3DData.z3,
		quad3DData.x4, quad3DData.y4, quad3DData.z4,
		quad3DData.x1, quad3DData.y1, quad3DData.z1 };

	GLdouble textArray[] = {
		quad3DData.TextureX1, quad3DData.TextureY1,
		quad3DData.TextureX2, quad3DData.TextureY1,
		quad3DData.TextureX2, quad3DData.TextureY2,
		quad3DData.TextureX2, quad3DData.TextureY2,
		quad3DData.TextureX1, quad3DData.TextureY2,
		quad3DData.TextureX1, quad3DData.TextureY1 };

	arr.vArray.push_back(vertexArray);
	arr.tArray.push_back(textArray);

	if (arr.useColors)
	{
		GLdouble alphaArray[] = {
			1.0, 1.0, 1.0, quad3DData.alphaTop,
			1.0, 1.0, 1.0, quad3DData.alphaTop,
			1.0, 1.0, 1.0, quad3DData.alphaBottom,
			1.0, 1.0, 1.0, quad3DData.alphaBottom,
			1.0, 1.0, 1.0, quad3DData.alphaBottom,
			1.0, 1.0, 1.0, quad3DData.alphaTop };

		arr.aArray.push_back(alphaArray);
	}
}

void CEngine::enableColorsPointer(bool state)
{
	if (state)
	{
		glEnableClientState(GL_COLOR_ARRAY);
	}
	else
	{
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

void CEngine::drawTrianglesWithAlpha(dTriangleWithAlpha &tris)
{
	bindTexture(tris.texture);

	if (tris.useColors && tris.autoEnDisaColors) glEnableClientState(GL_COLOR_ARRAY);
	
	//std::cout << "   " << tris.vArray.size() << std::endl;

	if (tris.useColors) glColorPointer(4, GL_DOUBLE, 0, &(tris.aArray[0]));
	glTexCoordPointer(2, GL_DOUBLE, 0, &(tris.tArray[0]));
	glVertexPointer(3, GL_DOUBLE, 0, &(tris.vArray[0]));

	glDrawArrays(GL_TRIANGLES, 0, 6 * tris.vArray.size());
	if (tris.useColors && tris.autoEnDisaColors) glDisableClientState(GL_COLOR_ARRAY);
}

void CEngine::Render3DQuadWithAlpha(const RenderDoubleStruct &quad3DData){
	GLdouble vertexArray[] = {
		quad3DData.x1, quad3DData.y1, quad3DData.z1,
		quad3DData.x2, quad3DData.y2, quad3DData.z2,
		quad3DData.x3, quad3DData.y3, quad3DData.z3,
		quad3DData.x3, quad3DData.y3, quad3DData.z3,
		quad3DData.x4, quad3DData.y4, quad3DData.z4,
		quad3DData.x1, quad3DData.y1, quad3DData.z1 };

	GLdouble textArray[] = {
		quad3DData.TextureX1, quad3DData.TextureY1,
		quad3DData.TextureX2, quad3DData.TextureY1,
		quad3DData.TextureX2, quad3DData.TextureY2,
		quad3DData.TextureX2, quad3DData.TextureY2,
		quad3DData.TextureX1, quad3DData.TextureY2,
		quad3DData.TextureX1, quad3DData.TextureY1 };

	GLdouble alphaArray[] = {
		1.0, 1.0, 1.0, quad3DData.alphaTop,
		1.0, 1.0, 1.0, quad3DData.alphaTop,
		1.0, 1.0, 1.0, quad3DData.alphaBottom,
		1.0, 1.0, 1.0, quad3DData.alphaBottom,
		1.0, 1.0, 1.0, quad3DData.alphaBottom,
		1.0, 1.0, 1.0, quad3DData.alphaTop };

	bindTexture(quad3DData.Text);

	glEnableClientState(GL_COLOR_ARRAY);

	glColorPointer(4, GL_DOUBLE, 0, alphaArray);
	glTexCoordPointer(2, GL_DOUBLE, 0, textArray);
	glVertexPointer(3, GL_DOUBLE, 0, vertexArray);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableClientState(GL_COLOR_ARRAY);
}

CEngine::CEngine()
{
	AASamples = 0;
	lastRenderAt[0] = 0.0;
	lastRenderAt[1] = 0.0;
	lastRenderAt[2] = 0.0;

	window = nullptr;

	lrC = lgC = lbC = laC = 1.0;

	mouseX = mouseY = 0.0;
	glMajor = 1;
	glMinor = 1;
	lastUsedTexture = 0;

	glfwSetErrorCallback(GLFWerrorfun);

	if (!glfwInit()){
		throw std::logic_error("Fail to initialize GLFW");
	}

	lastFrameTime = glfwGetTime();
	lastFPSSwapTime = glfwGetTime();
	DeltaTime = 0.0;
	FPS = 0;
	tmpFPS = 0;

	BASS_Init(-1, 44100, 0, 0, NULL);
	lastUpdatedNoise = 0.0;
	updateNoiseInterval = 0.2;

	setCamera(0.0, 0.0, 2.3, /* look from camera XYZ */
		0, 0, 0, /* look at the origin */
		0, 1, 0);

	wcallfunc = nullptr;
	/*
	if(!bitArray){
	bitArray = new unsigned int[10000];
	memset(bitArray, 0, sizeof(unsigned int) * 10000);
	}*/
}

CEngine::CEngine(std::function <void(int, const std::string &e)> errfun)
{
	lastRenderAt[0] = 0.0;
	lastRenderAt[1] = 0.0;
	lastRenderAt[2] = 0.0;
	AASamples = 0;

	window = nullptr;

	mouseX = mouseY = 0.0;
	glMajor = 1;
	glMinor = 1;
	lastUsedTexture = 0;

	errorCallbackFun = errfun;

	glfwSetErrorCallback(GLFWerrorfun);

	if (!glfwInit()){
		throw std::logic_error("Fail to initialize GLFW");
	}

	lastFrameTime = glfwGetTime();
	lastFPSSwapTime = glfwGetTime();
	DeltaTime = 0.0;
	FPS = 0;
	tmpFPS = 0;

	BASS_Init(-1, 44100, 0, 0, NULL);
	lastUpdatedNoise = 0.0;
	updateNoiseInterval = 0.2;

	setCamera(0.0, 0.0, 2.3, /* look from camera XYZ */
		0, 0, 0, /* look at the origin */
		0, 1, 0);

	wcallfunc = nullptr;
	/*
	if(!bitArray){
	bitArray = new unsigned int[10000];
	memset(bitArray, 0, sizeof(unsigned int) * 10000);
	}*/
}

CEngine::~CEngine(){
	glfwTerminate();
	//BASS_Stop();
	//BASS_Free();

	/*
	if(bitArray){
	delete[] bitArray;
	bitArray = nullptr;
	}*/

}
