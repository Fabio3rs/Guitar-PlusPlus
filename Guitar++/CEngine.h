#pragma once
#ifndef __GUITARPP_CENGINE_H_
#define __GUITARPP_CENGINE_H_
// brmodstudio.forumeiros.com

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <string>
#include <ctime>
#include <memory>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <bitset>
#include <deque>
#include <map>
#include <algorithm>
#include <utility>
#include <functional>
#include <array>

#ifndef GLFW_KEY_MENU
/* The unknown key */
#define GLFW_KEY_UNKNOWN            -1

/* Printable keys */
#define GLFW_KEY_SPACE              32
#define GLFW_KEY_APOSTROPHE         39  /* ' */
#define GLFW_KEY_COMMA              44  /* , */
#define GLFW_KEY_MINUS              45  /* - */
#define GLFW_KEY_PERIOD             46  /* . */
#define GLFW_KEY_SLASH              47  /* / */
#define GLFW_KEY_0                  48
#define GLFW_KEY_1                  49
#define GLFW_KEY_2                  50
#define GLFW_KEY_3                  51
#define GLFW_KEY_4                  52
#define GLFW_KEY_5                  53
#define GLFW_KEY_6                  54
#define GLFW_KEY_7                  55
#define GLFW_KEY_8                  56
#define GLFW_KEY_9                  57
#define GLFW_KEY_SEMICOLON          59  /* ; */
#define GLFW_KEY_EQUAL              61  /* = */
#define GLFW_KEY_A                  65
#define GLFW_KEY_B                  66
#define GLFW_KEY_C                  67
#define GLFW_KEY_D                  68
#define GLFW_KEY_E                  69
#define GLFW_KEY_F                  70
#define GLFW_KEY_G                  71
#define GLFW_KEY_H                  72
#define GLFW_KEY_I                  73
#define GLFW_KEY_J                  74
#define GLFW_KEY_K                  75
#define GLFW_KEY_L                  76
#define GLFW_KEY_M                  77
#define GLFW_KEY_N                  78
#define GLFW_KEY_O                  79
#define GLFW_KEY_P                  80
#define GLFW_KEY_Q                  81
#define GLFW_KEY_R                  82
#define GLFW_KEY_S                  83
#define GLFW_KEY_T                  84
#define GLFW_KEY_U                  85
#define GLFW_KEY_V                  86
#define GLFW_KEY_W                  87
#define GLFW_KEY_X                  88
#define GLFW_KEY_Y                  89
#define GLFW_KEY_Z                  90
#define GLFW_KEY_LEFT_BRACKET       91  /* [ */
#define GLFW_KEY_BACKSLASH          92  /* \ */
#define GLFW_KEY_RIGHT_BRACKET      93  /* ] */
#define GLFW_KEY_GRAVE_ACCENT       96  /* ` */
#define GLFW_KEY_WORLD_1            161 /* non-US #1 */
#define GLFW_KEY_WORLD_2            162 /* non-US #2 */

/* Function keys */
#define GLFW_KEY_ESCAPE             256
#define GLFW_KEY_ENTER              257
#define GLFW_KEY_TAB                258
#define GLFW_KEY_BACKSPACE          259
#define GLFW_KEY_INSERT             260
#define GLFW_KEY_DELETE             261
#define GLFW_KEY_RIGHT              262
#define GLFW_KEY_LEFT               263
#define GLFW_KEY_DOWN               264
#define GLFW_KEY_UP                 265
#define GLFW_KEY_PAGE_UP            266
#define GLFW_KEY_PAGE_DOWN          267
#define GLFW_KEY_HOME               268
#define GLFW_KEY_END                269
#define GLFW_KEY_CAPS_LOCK          280
#define GLFW_KEY_SCROLL_LOCK        281
#define GLFW_KEY_NUM_LOCK           282
#define GLFW_KEY_PRINT_SCREEN       283
#define GLFW_KEY_PAUSE              284
#define GLFW_KEY_F1                 290
#define GLFW_KEY_F2                 291
#define GLFW_KEY_F3                 292
#define GLFW_KEY_F4                 293
#define GLFW_KEY_F5                 294
#define GLFW_KEY_F6                 295
#define GLFW_KEY_F7                 296
#define GLFW_KEY_F8                 297
#define GLFW_KEY_F9                 298
#define GLFW_KEY_F10                299
#define GLFW_KEY_F11                300
#define GLFW_KEY_F12                301
#define GLFW_KEY_F13                302
#define GLFW_KEY_F14                303
#define GLFW_KEY_F15                304
#define GLFW_KEY_F16                305
#define GLFW_KEY_F17                306
#define GLFW_KEY_F18                307
#define GLFW_KEY_F19                308
#define GLFW_KEY_F20                309
#define GLFW_KEY_F21                310
#define GLFW_KEY_F22                311
#define GLFW_KEY_F23                312
#define GLFW_KEY_F24                313
#define GLFW_KEY_F25                314
#define GLFW_KEY_KP_0               320
#define GLFW_KEY_KP_1               321
#define GLFW_KEY_KP_2               322
#define GLFW_KEY_KP_3               323
#define GLFW_KEY_KP_4               324
#define GLFW_KEY_KP_5               325
#define GLFW_KEY_KP_6               326
#define GLFW_KEY_KP_7               327
#define GLFW_KEY_KP_8               328
#define GLFW_KEY_KP_9               329
#define GLFW_KEY_KP_DECIMAL         330
#define GLFW_KEY_KP_DIVIDE          331
#define GLFW_KEY_KP_MULTIPLY        332
#define GLFW_KEY_KP_SUBTRACT        333
#define GLFW_KEY_KP_ADD             334
#define GLFW_KEY_KP_ENTER           335
#define GLFW_KEY_KP_EQUAL           336
#define GLFW_KEY_LEFT_SHIFT         340
#define GLFW_KEY_LEFT_CONTROL       341
#define GLFW_KEY_LEFT_ALT           342
#define GLFW_KEY_LEFT_SUPER         343
#define GLFW_KEY_RIGHT_SHIFT        344
#define GLFW_KEY_RIGHT_CONTROL      345
#define GLFW_KEY_RIGHT_ALT          346
#define GLFW_KEY_RIGHT_SUPER        347
#define GLFW_KEY_MENU               348
#define GLFW_KEY_LAST               GLFW_KEY_MENU
#endif

enum windowsNum{closed, opened, preUpdate, posUpdate};

struct lightData{
	float ambientLight[4], diffuseLight[4], specularLight[4], position[4], direction[4], angle;

};

struct gppVec3f
{
	float x, y, z;
};

struct gppVec2f
{
	float x, y;
};

typedef void(*customwcallback)(int w, int h, windowsNum state);

class staticCallFunc
{

public:
	template<class T, class... U>
	staticCallFunc(T fun, U&&... u)
	{
		fun(std::forward<U>(u)...);
	};
};

class CEngine{
	void *window;
	customwcallback wcallfunc;
	bool openWindowCalled;

public:
	void setWindowCallbackFunction(customwcallback f){
		wcallfunc = f;
	}

	customwcallback getWindowCallbackFunction(){
		return wcallfunc;
	}

	struct GLFWimageC {
		int Width, Height;
		int Format;
		int BytesPerPixel;
		unsigned char *Data;
		unsigned char *tmpData;
		bool bRevPixels;
		bool keepData;

		GLFWimageC()
		{
			keepData = false;
			Data = nullptr;
			tmpData = nullptr;
			bRevPixels = false;
		}
	};

	typedef GLFWimageC GLFWimage;

	typedef struct {
		FILE*   file;
		void*   data;
		long    position;
		long    size;
	} GLFWstream;


	int readImage(const char *name, GLFWimage *img, int flags);
	int openFileStream(GLFWstream *stream, const char *name, const char *mode);
	long readStream(GLFWstream *stream, void *data, long size);
	void closeStream(GLFWstream *stream);
	long tellStream(GLFWstream *stream);
	int seekStream(GLFWstream *stream, long offset, int whence);
	int loadTexture2D(const char *name, int flags, GLFWimage *eimg = nullptr);
	int loadTextureImage2D(GLFWimage *img, int flags);
	void freeImage(GLFWimage *img);


	int getKey(int key);
	void Rotate(double a, double x, double y, double z);
	void matrixReset();
	static void pushMatrix();
	static void popMatrix();
	static void multiplyMatrix(float *matrix);

	int AASamples;

	struct RenderStruct{
		float x1, x2, x3, x4, y1, y2, y3, y4, z1, z2, z3, z4, TextureX1, TextureY1, TextureX2, TextureY2, alphaTop, alphaBottom;
		unsigned int Text;
	};

	struct RenderDoubleStruct{
		double x1, x2, x3, x4, y1, y2, y3, y4, z1, z2, z3, z4, TextureX1, TextureY1, TextureX2, TextureY2, alphaTop, alphaBottom;
		unsigned int Text;
	};

	double windowHeight, windowWidth;
	int glMajor, glMinor;

	double mouseX, mouseY;

	std::function <void(int, const std::string &e)> errorCallbackFun;

private:
	//void gluPerspective(double fovy, double aspect, double zNear, double zFar);
	static void GLFWerrorfun(int error, const char *description);

	double lastFrameTime, lastFPSSwapTime, DeltaTime;
	int FPS, tmpFPS;
	unsigned int lastUsedTexture, lastUsedVBOBuffer;

	unsigned int cursorText;

	std::string keyboardNoise;
	//const static uint32_t bitValues[32];

	std::bitset <0x18000> glStates;

	//void setBitState(unsigned int *array, unsigned int bitSet, bool state);

	struct audioInfo{
		const char *fileName;
		int state;
		void *handle;

		audioInfo(){
			state = 0;
			handle = nullptr;
		}
	};

	double lastUpdatedNoise, updateNoiseInterval;


	// TODO: make once allocation
	class CCircleStream{
		unsigned int GlBuffer;
		std::unique_ptr < double[] > verticesBuffer;
		double x, y, percent, radius, lineWeight;
		int polysNum;

	public:
		friend CEngine;

		bool isChanged(double x, double y, double percent, double radius, double lineWeight, int polysNum){
			if (percent != this->percent || x != this->x || y != this->y || radius != this->radius || lineWeight != this->lineWeight || polysNum != this->polysNum){
				this->x = x;
				this->y = y;
				this->percent = percent;
				this->radius = radius;
				this->lineWeight = lineWeight;

				if (polysNum > this->polysNum){
					verticesBuffer = std::unique_ptr< double[] >(new double[polysNum * 12]);
					this->polysNum = polysNum;
				}
				return true;
			}
			return false;
		}

		bool isChangedMX(double x, double y, double radius, double lineWeight, int polysNum){
			if (x != this->x || y != this->y || radius != this->radius || lineWeight != this->lineWeight || polysNum != this->polysNum){
				this->x = x;
				this->y = y;
				this->radius = radius;
				this->lineWeight = lineWeight;

				if (polysNum > this->polysNum){
					verticesBuffer = std::unique_ptr< double[] >(new double[polysNum * 12]);
					this->polysNum = polysNum;
				}
				return true;
			}
			return false;
		}

		void allocAll(int polysNum)
		{
			verticesBuffer = std::unique_ptr< double[] >(new double[polysNum * 12]);
			this->polysNum = polysNum;
		}

		inline CCircleStream(){
			GlBuffer = -1;
			x = y = percent = radius = lineWeight = 0;
			polysNum = 0;
		}
	};

	std::map<int, CCircleStream> circlesBuffer;
	//*****************************************


	double lrC, lgC, lbC, laC;

	double lastRenderAt[3];

public:
	double volumeMaster;

	double eyex,
		eyey,
		eyez,
		centerx,
		centery,
		centerz,
		upx,
		upy,
		upz;

	struct cameraSET{
		double eyex,
			eyey,
			eyez,
			centerx,
			centery,
			centerz,
			upx,
			upy,
			upz;

		inline cameraSET(double eyex,
			double eyey,
			double eyez,
			double centerx,
			double centery,
			double centerz,
			double upx,
			double upy,
			double upz){

			this->eyex = eyex;
			this->eyey = eyey;
			this->eyez = eyez;
			this->centerx = centerx;
			this->centery = centery;
			this->centerz = centerz;
			this->upx = upx;
			this->upy = upy;
			this->upz = upz;
		}

		inline cameraSET(){
			eyex = eyey = eyez = centerx = centery = centerz = upx = upy = upz = 0.0;
		}
	};

	struct staticDrawBuffer
	{
		void *pointer;
		unsigned int vertexL;
		unsigned int uvL;
		unsigned int normalsL;
		unsigned int texture;
		unsigned int bufferID;
		unsigned int sizebytes;
		int count;

		inline staticDrawBuffer()
		{
			pointer = nullptr;
			count = 0;
			vertexL = uvL = normalsL = texture = 0;
			bufferID = ~0;
			sizebytes = 0;
		}

		void destroy(bool deletePtr = false);
		~staticDrawBuffer();
	};

	struct dTriangleWithAlpha
	{
		bool useColors;
		bool autoEnDisaColors;
		unsigned int texture;
		template<class T>
		struct vType
		{
			T t;

			vType(T &tdata)
			{
				memcpy(t, tdata, sizeof(tdata));
			}
		};

		std::vector<vType<double[18]>> vArray;
		std::vector<vType<double[12]>> tArray;
		std::vector<vType<double[24]>> aArray;

		inline void clear()
		{
			vArray.clear();
			tArray.clear();
			aArray.clear();
		}

		inline dTriangleWithAlpha()
		{
			autoEnDisaColors = true;
		}
	};

	std::function < void(void) > renderFrameCallback;

	static void pushQuad(dTriangleWithAlpha &arr, const RenderDoubleStruct &quad3DData);
	void drawTrianglesWithAlpha(dTriangleWithAlpha &tris);
	static void enableColorsPointer(bool state);

	const inline void *getWindow() const{ return window; }

	struct Resolution{
		int 	width;
		int 	height;
		int 	redBits;
		int 	greenBits;
		int 	blueBits;
		int 	refreshRate;
	};

	std::deque<Resolution> getPossibleVideoModes();

	int noiseCRC32;

	std::string getKeyboardNoise();
	int getCRCKeyboardNoise();
	double updateRandomNoiseInterval(double interval);

	void setCamera(double eyex,
		double eyey,
		double eyez,
		double centerx,
		double centery,
		double centerz,
		double upx,
		double upy,
		double upz);

	void setCamera(const cameraSET &cam);
	void openWindow(const char *name, int w, int h, int fullScreen);
	bool windowOpened();
	void renderFrame();
	void clearScreen();

	/**/
	unsigned int loadTexture(const char *texturePath, GLFWimage *eimg = nullptr);
	void bindTexture(unsigned int text);

	void glEnable(int num);
	void glDisable(int num);

	void renderAt(double x, double y, double z);
	void loadModel();

	/* Rendering functions */
	void Render2DQuad(const RenderDoubleStruct &quad2DData);
	void Render3DQuad(const RenderDoubleStruct &quad3DData);
	void RenderCustomVerticesFloat(void *vertexPtr, void *uvPtr, void *normals, int count, unsigned int texture);
	void RenderCustomVerticesFloat(void *vertexPtr, void *uvPtr, void *normals, int count, unsigned int texture, unsigned int &vbuffer, unsigned int &uvbuffer, unsigned int &nvbuffer);
	void RenderCustomVerticesFloat(staticDrawBuffer &buffer, bool autoBindZero = true);
	void RenderMulti3DQuad(const std::deque<RenderDoubleStruct> &quad3DData, unsigned int &bufferID);
	void Render3DQuadWithAlpha(const RenderDoubleStruct &quad3DData);
	void Render2DCircle(double x, double y, double percent, double radius, double lineWeight, int polysNum, int maxPolys, unsigned int &bufferID);
	void Render2DCircleBufferMax(double x, double y, double perone, double radius, double lineWeight, int maxPolys, unsigned int &bufferID);
	void draw2DLine(double *linedata, int points);
	void setColor(double r, double g, double b, double a);

	void addToAccumulationBuffer(double d);
	void retAccumulationBuffer(double d);
	void clearAccmumaltionBuffer();

	float getSoundBPM(unsigned int sound, double at, double interval);
	double getChannelLength(unsigned int ch);

	struct chdata
	{
		float data[4];
	};

	/**/
	bool loadSoundStream(const char *fileName, int &handle, bool decode = false);
	bool loadMusicStream(const char *fileName, int &handle);
	bool playSoundStream(int handle);
	bool pauseSoundStream(int handle);
	bool unloadSoundStream(int &handle);
	void setSoundTime(int handle, double time);
	double getSoundTime(int handle);
	double getSoundVolume(int handle);
	bool setSoundVolume(int handle, float volume);
	int setSoundFlags(int handle, int flags, int mask);
	int setSoundAttribute(int handle, int attribute, float value);
	chdata getChannelData(int handle);
	chdata getChannelData(int handle, int b);

	static int getBassError();

	void setScale(double x, double y, double z);

	double getTime();
	inline double getDeltaTime() const{return DeltaTime;}
	int getMouseButton(int btn);
	inline const int &getFPS() const{ return FPS; }

	unsigned int vboSET(size_t size, void *buffer);
	void attribVBOBuff(int id, int size, unsigned int buffer);
	void disableBuf(int id);
	void drawBufArrays(int size);
	void bindTextOnSlot(int text, int slot);
	unsigned int getUniformLocation(unsigned int programID, const char *str);
	void useShader(unsigned int programID);
	void bindVBOBuffer(unsigned int buffer);

	void activateNormals(bool a);
	void activate3DRender(bool a);
	void activateStencilTest(bool a);
	void activateLighting(bool a);
	void activateLight(int id, bool a);
	void setLight(const lightData &l, int id, bool setAmbient = true);

	void clear3DBuffer();

	void startShadowCapture();
	void endShadowCapture();
	static void shadowMatrix(float shadowMat[4][4], float groundplane[4], float lightpos[4]);
	static void findPlane(float plane[4], float v0[3], float v1[3], float v2[3]);

	static void enablePolygonOffset();
	static void disablePolygonOffset();

	static void colorRGBToArray(int32_t rgb, double *arr);
	static void colorRGBToArrayf(int32_t rgb, float *arr);

	void setVSyncMode(int mode);

	~CEngine();

	static CEngine &engine(std::function <void(int, const std::string &e)> errfun = nullptr);

	CEngine(const CEngine&) = delete;

private:
	CEngine();
	CEngine(std::function <void(int, const std::string &e)> errfun);
};

#endif