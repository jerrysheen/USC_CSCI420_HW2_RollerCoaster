/*
/*
  CSCI 420 Computer Graphics, USC
  Assignment 1: Height Fields with Shaders.
  C++ starter code

  Student username: <type your USC username here>
*/

#include "basicPipelineProgram.h"
#include "openGLMatrix.h"
#include "imageIO.h"
#include "openGLHeader.h"
#include "glutHeader.h"

#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <math.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>



#if defined(WIN32) || defined(_WIN32)
#ifdef _DEBUG
#pragma comment(lib, "glew32d.lib")
#else
#pragma comment(lib, "glew32.lib")
#endif
#endif

#if defined(WIN32) || defined(_WIN32)
char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
char shaderBasePath[1024] = "../openGLHelper-starterCode";
#endif

using namespace std;

int mousePos[2]; // x,y coordinate of the mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

// state of the world
float landRotate[3] = { 0.0f, 0.0f, 0.0f };
float landTranslate[3] = { 0.0f, 0.0f, 0.0f };
float landScale[3] = { 1.0f, 1.0f, 1.0f };

int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 homework I";

ImageIO * heightmapImage;

GLuint triVertexBuffer, triColorVertexBuffer;
GLuint triVertexArray;
int sizeTri;

OpenGLMatrix matrix;
BasicPipelineProgram * pipelineProgram;

typedef enum { P, W, T, S } RENDER_MODE;
RENDER_MODE render_mode = P;

std::vector<glm::vec3> point_set;
std::vector<glm::vec4> point_color_set;



std::vector<glm::vec3> lines_set;
std::vector<glm::vec4> lines_color_set;


std::vector<GLfloat> ground_vertices;
std::vector<GLfloat> ground_uvs;



float max_height;
int iterNum = 0;
GLuint program;
GLuint vao;
GLuint points_vbo;
GLuint color_vbo;
// for mode == 2;

GLuint ground_texture_handle;
GLuint vao_ground_texture;
GLuint vbo_ground_vertices;
GLuint vbo_ground_uvs;



//hw2 code here
// represents one control point along the spline 
struct Point
{
	double x;
	double y;
	double z;
};

// spline struct 
// contains how many control points the spline has, and an array of control points 
struct Spline
{
	int numControlPoints;
	Point * points;
};

// the spline array 
Spline * splines;
// total number of splines 
int numSplines;

int loadSplines(char * argv)
{
	char * cName = (char *)malloc(128 * sizeof(char));
	FILE * fileList;
	FILE * fileSpline;
	int iType, i = 0, j, iLength;

	// load the track file 
	fileList = fopen(argv, "r");
	if (fileList == NULL)
	{
		printf("can't open file\n");
		exit(1);
	}

	// stores the number of splines in a global variable 
	fscanf(fileList, "%d", &numSplines);
	printf("current num == %d \n", numSplines);
	splines = (Spline*)malloc(numSplines * sizeof(Spline));

	// reads through the spline files 
	for (j = 0; j < numSplines; j++)
	{
		i = 0;
		fscanf(fileList, "%s", cName);
		fileSpline = fopen(cName, "r");

		if (fileSpline == NULL)
		{
			printf("can't open file\n");
			exit(1);
		}

		// gets length for spline file
		fscanf(fileSpline, "%d %d", &iLength, &iType);

		// allocate memory for all the points
		splines[j].points = (Point *)malloc(iLength * sizeof(Point));
		splines[j].numControlPoints = iLength;

		// saves the data to the struct
		while (fscanf(fileSpline, "%lf %lf %lf",
			&splines[j].points[i].x,
			&splines[j].points[i].y,
			&splines[j].points[i].z) != EOF)
		{
			i++;
		}
	}

	free(cName);

	return 0;
}


int initTexture(const char * imageFilename, GLuint textureHandle)
{
	// read the texture image
	ImageIO img;
	ImageIO::fileFormatType imgFormat;
	ImageIO::errorType err = img.load(imageFilename, &imgFormat);

	if (err != ImageIO::OK)
	{
		printf("Loading texture from %s failed.\n", imageFilename);
		return -1;
	}
	else {
		printf("Loaded texture from %s \n", imageFilename);
	}

	// check that the number of bytes is a multiple of 4
	if (img.getWidth() * img.getBytesPerPixel() % 4)
	{
		printf("Error (%s): The width*numChannels in the loaded image must be a multiple of 4.\n", imageFilename);
		return -1;
	}

	// allocate space for an array of pixels
	int width = img.getWidth();
	int height = img.getHeight();
	unsigned char * pixelsRGBA = new unsigned char[4 * width * height]; // we will use 4 bytes per pixel, i.e., RGBA

	// fill the pixelsRGBA array with the image pixels
	memset(pixelsRGBA, 0, 4 * width * height); // set all bytes to 0
	for (int h = 0; h < height; h++)
		for (int w = 0; w < width; w++)
		{
			// assign some default byte values (for the case where img.getBytesPerPixel() < 4)
			pixelsRGBA[4 * (h * width + w) + 0] = 0; // red
			pixelsRGBA[4 * (h * width + w) + 1] = 0; // green
			pixelsRGBA[4 * (h * width + w) + 2] = 0; // blue
			pixelsRGBA[4 * (h * width + w) + 3] = 255; // alpha channel; fully opaque

			// set the RGBA channels, based on the loaded image
			int numChannels = img.getBytesPerPixel();
			for (int c = 0; c < numChannels; c++) // only set as many channels as are available in the loaded image; the rest get the default value
				pixelsRGBA[4 * (h * width + w) + c] = img.getPixel(w, h, c);
		}

	// bind the texture
	glBindTexture(GL_TEXTURE_2D, textureHandle);

	// initialize the texture
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);

	// generate the mipmaps for this texture
	glGenerateMipmap(GL_TEXTURE_2D);

	// set the texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// query support for anisotropic texture filtering
	GLfloat fLargest;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
	printf("Max available anisotropic samples: %f\n", fLargest);
	// set anisotropic texture filtering
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.5f * fLargest);

	// query for any errors
	GLenum errCode = glGetError();
	if (errCode != 0)
	{
		printf("Texture initialization error. Error code: %d.\n", errCode);
		return -1;
	}

	// de-allocate the pixel array -- it is no longer needed
	delete[] pixelsRGBA;

	return 0;
}


void saveScreenshot(const char * filename)
{
	unsigned char * screenshotData = new unsigned char[windowWidth * windowHeight * 3];
	glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData);

	ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

	if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
		cout << "File " << filename << " saved successfully." << endl;
	else cout << "Failed to save file " << filename << '.' << endl;

	delete[] screenshotData;
}

std::vector<Point> control_points;
std::vector<glm::vec3> left_splinePoint_positions;
std::vector<glm::vec3> left_splinePoint_normal;

std::vector<glm::vec3> right_splinePoint_positions;
std::vector<glm::vec3> right_splinePoint_normal;

std::vector<glm::vec3> sleeper_positions;
std::vector<glm::vec3> sleeper_normal;
std::vector<GLfloat> sleeper_uvs;

std::vector<glm::vec3> support_positions;
std::vector<glm::vec3> support_normal;
std::vector<GLfloat> support_uvs;

std::vector<GLfloat> spline_point_uvs;
std::vector<glm::vec4> sleeper_color;


std::vector<glm::vec3> rail_tangent_set;
std::vector<glm::vec3> rail_normal_set;
std::vector<glm::vec3> rail_binormal_set;
void generateSpline() {

	for (int i = 0; i < numSplines; i++) {
		for (int j = 0; j < splines[i].numControlPoints; j++) {
			control_points.push_back(splines[i].points[j]);
			printf("size control point : %f,   %f.   %f\n", splines[i].points[j].x, splines[i].points[j].y, splines[i].points[j].z);
		}
	}
	printf("size control point : %d\n", control_points.size());
	for (int i = 0; i < control_points.size() - 3; i++) {
		//printf("testtesttesttesttesttesttesttesttesttest");
		float s = 0.5;
		float m[16] = { -1.0f*s, 2.0f - s, s - 2.0f,  s,
							2.0f*s, s - 3.0f, 3.0f - 2 * s,-1 * s,
						   -1.0f*s, 0.0f, 	s, 		 0.0f,
							0.0f, 	1.0f, 	0.0f, 	 0.0f };
		Point dot1 = control_points[i];
		Point dot2 = control_points[i + 1];
		Point dot3 = control_points[i + 2];
		Point dot4 = control_points[i + 3];
		float control[12] = { dot1.x, dot1.y, dot1.z,
								dot2.x, dot2.y, dot2.z,
								dot3.x, dot3.y, dot3.z,
								dot4.x, dot4.y, dot4.z };

		for (float u = 0; u < 1.0; u += 0.005f) {
			float u_value[4] = { pow(u,3), pow(u,2), u, 1 };
			glm::vec4 u_mat = glm::make_vec4(u_value);
			glm::mat4x4 m_mat = glm::make_mat4x4(m);
			glm::mat4x3 control_mat = glm::make_mat4x3(control);

			glm::vec3 point = control_mat * m_mat * u_mat;
			//printf("%f", point.x);
			point_set.push_back(point);
			point_color_set.push_back({ 1,1,0,1 });

		}
	}
	

	for (int i = 0; i < point_set.size() - 3; i++) {
		// we want calculate the tangent of the point, just do derivative.
		// (Au3 + Bu2 + cu + d) ' =  2Au2 + 2Bu + 1
		// so that we can get the tangent on the u point...
		float s = 0.5;
		float m[16] = { -1.0f*s, 2.0f - s, s - 2.0f,  s,
							2.0f*s, s - 3.0f, 3.0f - 2 * s,-1 * s,
						   -1.0f*s, 0.0f, 	s, 		 0.0f,
							0.0f, 	1.0f, 	0.0f, 	 0.0f };
		glm::vec3 dot1 = point_set[i];
		glm::vec3 dot2 = point_set[i + 1];
		glm::vec3 dot3 = point_set[i + 2];
		glm::vec3 dot4 = point_set[i + 3];
		float control[12] = { dot1.x, dot1.y, dot1.z,
								dot2.x, dot2.y, dot2.z,
								dot3.x, dot3.y, dot3.z,
								dot4.x, dot4.y, dot4.z };
		float u = 0.005f;
		float u_prime_value[4] = { 3 * pow(u,2), 2 * pow(u,2), 1, 0 };
		glm::vec4 u_mat = glm::make_vec4(u_prime_value);
		glm::mat4x4 m_mat = glm::make_mat4x4(m);
		glm::mat4x3 control_mat = glm::make_mat4x3(control);

		// calculate tangent. and normalize it.
		glm::vec3 tangent_u = control_mat * m_mat * u_mat;
		glm::vec3 unit_tangent_u = glm::normalize(tangent_u);

		rail_tangent_set.push_back(unit_tangent_u);

		// apply sloan's method to decide camera movement
		if (i == 0) {
			glm::vec3 T0 = unit_tangent_u;
			glm::vec3 V = { 0.0f, 1.0f, 0.0f };
			glm::vec3 N0 = glm::normalize(glm::cross(T0, V));
			glm::vec3 B0 = glm::normalize(glm::cross(T0, N0));
			rail_normal_set.push_back(N0);
			rail_binormal_set.push_back(B0);
		}
		else {
			glm::vec3 T1 = unit_tangent_u;
			glm::vec3 N1 = glm::normalize(glm::cross(rail_binormal_set[i - 1], T1));
			glm::vec3 B1 = glm::normalize(glm::cross(T1, N1));
			rail_normal_set.push_back(N1);
			rail_binormal_set.push_back(B1);
		}
	}
	for (int k = 0; k < point_set.size() - 10; k++) {
		//printf("size point_set : %d\n", point_set.size());
			// right
			glm::vec3 left_p0;
			left_p0.x = point_set[k].x + 0.25 * rail_normal_set[k].x;
			left_p0.y = point_set[k].y + 0.25 * rail_normal_set[k].y;
			left_p0.z = point_set[k].z + 0.25 * rail_normal_set[k].z;
			glm::vec3 left_p1;
			left_p1.x = point_set[k + 1].x + 0.25 * rail_normal_set[k + 1].x;
			left_p1.y = point_set[k + 1].y + 0.25 * rail_normal_set[k + 1].y;
			left_p1.z = point_set[k + 1].z + 0.25 * rail_normal_set[k + 1].z;

			float alpha = 0.05f;
			// v0, v1, v2, v3 follows -> 
			// "http://barbic.usc.edu/cs420-s21/assign2/csci420_assign2_crossSection.pdf"
			
			//v0
			float left_v0[3] = { left_p0.x + alpha * (-rail_normal_set[k][0] + rail_binormal_set[k][0]),
				left_p0.y + alpha * (-rail_normal_set[k][1] + rail_binormal_set[k][1]),
				left_p0.z + alpha * (-rail_normal_set[k][2] + rail_binormal_set[k][2])
			};
			//v1
			float left_v1[3] = { left_p0.x + alpha * (+rail_normal_set[k][0] + rail_binormal_set[k][0]),
				left_p0.y + alpha * (+rail_normal_set[k][1] + rail_binormal_set[k][1]),
				left_p0.z + alpha * (+rail_normal_set[k][2] + rail_binormal_set[k][2])
			};
			//v2
			float left_v2[3] = { left_p0.x + alpha * (rail_normal_set[k][0] - rail_binormal_set[k][0]),
				left_p0.y + alpha * (rail_normal_set[k][1] - rail_binormal_set[k][1]),
				left_p0.z + alpha * (rail_normal_set[k][2] - rail_binormal_set[k][2])
			};

			//v3 
			float left_v3[3] = { left_p0.x + alpha * (-rail_normal_set[k][0] - rail_binormal_set[k][0]),
				left_p0.y + alpha * (-rail_normal_set[k][1] - rail_binormal_set[k][1]),
				left_p0.z + alpha * (-rail_normal_set[k][2] - rail_binormal_set[k][2])
			};
			
			
			glm::vec3 left_v0_splinePoint = glm::make_vec3(left_v0);
			glm::vec3 left_v1_splinePoint = glm::make_vec3(left_v1);
			glm::vec3 left_v2_splinePoint = glm::make_vec3(left_v2);
			glm::vec3 left_v3_splinePoint = glm::make_vec3(left_v3);

			// define the default color here.
			// without phong mode, it will show gray color.
			glm::vec4 default_color = glm::vec4(0.5, 0.5, 0.5, 0);

			//v4
			float left_v4[3] = { left_p1[0] + alpha * (-rail_normal_set[k + 1][0] + rail_binormal_set[k + 1][0]),
				left_p1[1] + alpha * (-rail_normal_set[k + 1][1] + rail_binormal_set[k + 1][1]),
				left_p1[2] + alpha * (-rail_normal_set[k + 1][2] + rail_binormal_set[k + 1][2])
			};
			//v5
			float left_v5[3] = { left_p1[0] + alpha * (rail_normal_set[k + 1][0] + rail_binormal_set[k + 1][0]),
				left_p1[1] + alpha * (rail_normal_set[k + 1][1] + rail_binormal_set[k + 1][1]),
				left_p1[2] + alpha * (rail_normal_set[k + 1][2] + rail_binormal_set[k + 1][2])
			};
			//v6
			float left_v6[3] = { left_p1[0] + alpha * (rail_normal_set[k + 1][0] - rail_binormal_set[k + 1][0]),
				left_p1[1] + alpha * (rail_normal_set[k + 1][1] - rail_binormal_set[k + 1][1]),
				left_p1[2] + alpha * (rail_normal_set[k + 1][2] - rail_binormal_set[k + 1][2])
			};
			//v7
			float left_v7[3] = { left_p1[0] + alpha * (-rail_normal_set[k + 1][0] - rail_binormal_set[k + 1][0]),
				left_p1[1] + alpha * (-rail_normal_set[k + 1][1] - rail_binormal_set[k + 1][1]),
				left_p1[2] + alpha * (-rail_normal_set[k + 1][2] - rail_binormal_set[k + 1][2])
			};
			
			
			glm::vec3 left_v4_splinePoint = glm::make_vec3(left_v4);
			glm::vec3 left_v5_splinePoint = glm::make_vec3(left_v5);
			glm::vec3 left_v6_splinePoint = glm::make_vec3(left_v6);
			glm::vec3 left_v7_splinePoint = glm::make_vec3(left_v7);

			//right face triangle one
			right_splinePoint_positions.push_back(left_v1_splinePoint);
			right_splinePoint_positions.push_back(left_v0_splinePoint);
			right_splinePoint_positions.push_back(left_v4_splinePoint);
			
			right_splinePoint_normal.push_back(rail_binormal_set[k]);
			right_splinePoint_normal.push_back(rail_binormal_set[k]);
			right_splinePoint_normal.push_back(rail_binormal_set[k + 1]);

			//right face triangle two
			right_splinePoint_positions.push_back(left_v4_splinePoint);
			right_splinePoint_positions.push_back(left_v5_splinePoint);
			right_splinePoint_positions.push_back(left_v1_splinePoint);
			
			right_splinePoint_normal.push_back(rail_binormal_set[k + 1]);
			right_splinePoint_normal.push_back(rail_binormal_set[k + 1]);
			right_splinePoint_normal.push_back(rail_binormal_set[k]);
			

			
			
			//left face triangle one v3, v7, v2
			right_splinePoint_positions.push_back(left_v3_splinePoint);
			right_splinePoint_positions.push_back(left_v7_splinePoint);
			right_splinePoint_positions.push_back(left_v2_splinePoint);
			
			right_splinePoint_normal.push_back(-rail_binormal_set[k]);
			right_splinePoint_normal.push_back(-rail_binormal_set[k + 1]);
			right_splinePoint_normal.push_back(-rail_binormal_set[k]);
			
		
			
			//left face triangle two v2, v7,v6
			right_splinePoint_positions.push_back(left_v2_splinePoint);
			right_splinePoint_positions.push_back(left_v7_splinePoint);
			right_splinePoint_positions.push_back(left_v6_splinePoint);
			
			right_splinePoint_normal.push_back(-rail_binormal_set[k]);
			right_splinePoint_normal.push_back(-rail_binormal_set[k + 1]);
			right_splinePoint_normal.push_back(-rail_binormal_set[k +1]);

	

			//top face triangle one
			// v2, v6. v5
			right_splinePoint_positions.push_back(left_v2_splinePoint);
			right_splinePoint_positions.push_back(left_v5_splinePoint);
			right_splinePoint_positions.push_back(left_v6_splinePoint);
			
			right_splinePoint_normal.push_back(rail_normal_set[k]);
			right_splinePoint_normal.push_back(rail_normal_set[k + 1]);
			right_splinePoint_normal.push_back(rail_normal_set[k + 1]);

		

			//top face triangle two
			// v2 v1 v5
			right_splinePoint_positions.push_back(left_v2_splinePoint);
			right_splinePoint_positions.push_back(left_v1_splinePoint);
			right_splinePoint_positions.push_back(left_v5_splinePoint);
			
			right_splinePoint_normal.push_back(rail_normal_set[k]);
			right_splinePoint_normal.push_back(rail_normal_set[k]);
			right_splinePoint_normal.push_back(rail_normal_set[k + 1]);

	

			//bottom face triangle one
			right_splinePoint_positions.push_back(left_v3_splinePoint);
			right_splinePoint_positions.push_back(left_v0_splinePoint);
			right_splinePoint_positions.push_back(left_v4_splinePoint);
			
			right_splinePoint_normal.push_back(-rail_normal_set[k]);
			right_splinePoint_normal.push_back(-rail_normal_set[k]);
			right_splinePoint_normal.push_back(-rail_normal_set[k + 1]);

	

			//bottom face triangle two
			right_splinePoint_positions.push_back(left_v3_splinePoint);
			right_splinePoint_positions.push_back(left_v4_splinePoint);
			right_splinePoint_positions.push_back(left_v7_splinePoint);
			
			right_splinePoint_normal.push_back(-rail_normal_set[k]);
			right_splinePoint_normal.push_back(-rail_normal_set[k + 1]);
			right_splinePoint_normal.push_back(-rail_normal_set[k + 1]);

			glm::vec3 right_p0;
			right_p0.x = point_set[k].x - 0.25 * rail_normal_set[k].x;
			right_p0.y = point_set[k].y - 0.25 * rail_normal_set[k].y;
			right_p0.z = point_set[k].z - 0.25 * rail_normal_set[k].z;
			glm::vec3 right_p1;
			right_p1.x = point_set[k + 1].x - 0.25 * rail_normal_set[k + 1].x;
			right_p1.y = point_set[k + 1].y - 0.25 * rail_normal_set[k + 1].y;
			right_p1.z = point_set[k + 1].z - 0.25 * rail_normal_set[k + 1].z;
			alpha = 0.05f;
			// v0, v1, v2, v3 follows -> 
			// "http://barbic.usc.edu/cs420-s21/assign2/csci420_assign2_crossSection.pdf"

			//v0
			float right_v0[3] = { right_p0.x + alpha * (-rail_normal_set[k][0] + rail_binormal_set[k][0]),
				right_p0.y + alpha * (-rail_normal_set[k][1] + rail_binormal_set[k][1]),
				right_p0.z + alpha * (-rail_normal_set[k][2] + rail_binormal_set[k][2])
			};
			//v1
			float right_v1[3] = { right_p0.x + alpha * (+rail_normal_set[k][0] + rail_binormal_set[k][0]),
				right_p0.y + alpha * (+rail_normal_set[k][1] + rail_binormal_set[k][1]),
				right_p0.z + alpha * (+rail_normal_set[k][2] + rail_binormal_set[k][2])
			};
			//v2
			float right_v2[3] = { right_p0.x + alpha * (rail_normal_set[k][0] - rail_binormal_set[k][0]),
				right_p0.y + alpha * (rail_normal_set[k][1] - rail_binormal_set[k][1]),
				right_p0.z + alpha * (rail_normal_set[k][2] - rail_binormal_set[k][2])
			};

			//v3 
			float right_v3[3] = { right_p0.x + alpha * (-rail_normal_set[k][0] - rail_binormal_set[k][0]),
				right_p0.y + alpha * (-rail_normal_set[k][1] - rail_binormal_set[k][1]),
				right_p0.z + alpha * (-rail_normal_set[k][2] - rail_binormal_set[k][2])
			};


			glm::vec3 right_v0_splinePoint = glm::make_vec3(right_v0);
			glm::vec3 right_v1_splinePoint = glm::make_vec3(right_v1);
			glm::vec3 right_v2_splinePoint = glm::make_vec3(right_v2);
			glm::vec3 right_v3_splinePoint = glm::make_vec3(right_v3);

			// define the default color here.
			// without phong mode, it will show gray color.



			//point 2 triangle coordinates
			//v4
			float right_v4[3] = { right_p1[0] + alpha * (-rail_normal_set[k + 1][0] + rail_binormal_set[k + 1][0]),
				right_p1[1] + alpha * (-rail_normal_set[k + 1][1] + rail_binormal_set[k + 1][1]),
				right_p1[2] + alpha * (-rail_normal_set[k + 1][2] + rail_binormal_set[k + 1][2])
			};
			//v5
			float right_v5[3] = { right_p1[0] + alpha * (rail_normal_set[k + 1][0] + rail_binormal_set[k + 1][0]),
				right_p1[1] + alpha * (rail_normal_set[k + 1][1] + rail_binormal_set[k + 1][1]),
				right_p1[2] + alpha * (rail_normal_set[k + 1][2] + rail_binormal_set[k + 1][2])
			};
			//v6
			float right_v6[3] = { right_p1[0] + alpha * (rail_normal_set[k + 1][0] - rail_binormal_set[k + 1][0]),
				right_p1[1] + alpha * (rail_normal_set[k + 1][1] - rail_binormal_set[k + 1][1]),
				right_p1[2] + alpha * (rail_normal_set[k + 1][2] - rail_binormal_set[k + 1][2])
			};
			//v7
			float right_v7[3] = { right_p1[0] + alpha * (-rail_normal_set[k + 1][0] - rail_binormal_set[k + 1][0]),
				right_p1[1] + alpha * (-rail_normal_set[k + 1][1] - rail_binormal_set[k + 1][1]),
				right_p1[2] + alpha * (-rail_normal_set[k + 1][2] - rail_binormal_set[k + 1][2])
			};


			glm::vec3 right_v4_splinePoint = glm::make_vec3(right_v4);
			glm::vec3 right_v5_splinePoint = glm::make_vec3(right_v5);
			glm::vec3 right_v6_splinePoint = glm::make_vec3(right_v6);
			glm::vec3 right_v7_splinePoint = glm::make_vec3(right_v7);

			//right face triangle one
			left_splinePoint_positions.push_back(right_v1_splinePoint);
			left_splinePoint_positions.push_back(right_v0_splinePoint);
			left_splinePoint_positions.push_back(right_v4_splinePoint);

			left_splinePoint_normal.push_back(rail_binormal_set[k]);
			left_splinePoint_normal.push_back(rail_binormal_set[k]);
			left_splinePoint_normal.push_back(rail_binormal_set[k + 1]);


			//right face triangle two
			left_splinePoint_positions.push_back(right_v4_splinePoint);
			left_splinePoint_positions.push_back(right_v5_splinePoint);
			left_splinePoint_positions.push_back(right_v1_splinePoint);

			left_splinePoint_normal.push_back(rail_binormal_set[k + 1]);
			left_splinePoint_normal.push_back(rail_binormal_set[k + 1]);
			left_splinePoint_normal.push_back(rail_binormal_set[k]);


			//left face triangle one v3, v7, v2
			left_splinePoint_positions.push_back(right_v3_splinePoint);
			left_splinePoint_positions.push_back(right_v7_splinePoint);
			left_splinePoint_positions.push_back(right_v2_splinePoint);

			left_splinePoint_normal.push_back(-rail_binormal_set[k]);
			left_splinePoint_normal.push_back(-rail_binormal_set[k + 1]);
			left_splinePoint_normal.push_back(-rail_binormal_set[k]);



			//left face triangle two v2, v7,v6
			left_splinePoint_positions.push_back(right_v2_splinePoint);
			left_splinePoint_positions.push_back(right_v7_splinePoint);
			left_splinePoint_positions.push_back(right_v6_splinePoint);

			left_splinePoint_normal.push_back(-rail_binormal_set[k]);
			left_splinePoint_normal.push_back(-rail_binormal_set[k + 1]);
			left_splinePoint_normal.push_back(-rail_binormal_set[k + 1]);

			//top face triangle one
			// v2, v6. v5
			left_splinePoint_positions.push_back(right_v2_splinePoint);
			left_splinePoint_positions.push_back(right_v5_splinePoint);
			left_splinePoint_positions.push_back(right_v6_splinePoint);

			left_splinePoint_normal.push_back(rail_normal_set[k]);
			left_splinePoint_normal.push_back(rail_normal_set[k + 1]);
			left_splinePoint_normal.push_back(rail_normal_set[k + 1]);


			//top face triangle two
			// v2 v1 v5
			left_splinePoint_positions.push_back(right_v2_splinePoint);
			left_splinePoint_positions.push_back(right_v1_splinePoint);
			left_splinePoint_positions.push_back(right_v5_splinePoint);

			left_splinePoint_normal.push_back(rail_normal_set[k]);
			left_splinePoint_normal.push_back(rail_normal_set[k]);
			left_splinePoint_normal.push_back(rail_normal_set[k + 1]);


			//bottom face triangle one
			left_splinePoint_positions.push_back(right_v3_splinePoint);
			left_splinePoint_positions.push_back(right_v0_splinePoint);
			left_splinePoint_positions.push_back(right_v4_splinePoint);

			left_splinePoint_normal.push_back(-rail_normal_set[k]);
			left_splinePoint_normal.push_back(-rail_normal_set[k]);
			left_splinePoint_normal.push_back(-rail_normal_set[k + 1]);


			//bottom face triangle two
			left_splinePoint_positions.push_back(right_v3_splinePoint);
			left_splinePoint_positions.push_back(right_v4_splinePoint);
			left_splinePoint_positions.push_back(right_v7_splinePoint);

			left_splinePoint_normal.push_back(-rail_normal_set[k]);
			left_splinePoint_normal.push_back(-rail_normal_set[k + 1]);
			left_splinePoint_normal.push_back(-rail_normal_set[k + 1]);


			GLfloat v3[2] = { 0.0f, 0.0f };
			GLfloat v2[2] = { 0.0f, 1.0f }; 
			GLfloat v1[2] = { 1.0f, 1.0f };
			GLfloat v0[2] = { 1.0f, 0.0f }; 


			//sleeper....
			
			if (k % 30 == 0 && k < point_set.size() - 30) {
				glm::vec3 tempv2, tempv1;
				for (int i = 0; i < 3; i++) {
					tempv2[i] = left_v2_splinePoint[i] + 0.1 *rail_tangent_set[k][i];
					tempv1[i] = right_v1_splinePoint[i] + 0.1 * rail_tangent_set[k][i];
				}
				sleeper_positions.push_back(tempv2);
				sleeper_positions.push_back(left_v3_splinePoint);
				sleeper_positions.push_back(tempv1);
				sleeper_positions.push_back(left_v3_splinePoint);
				sleeper_positions.push_back(right_v0_splinePoint);
				sleeper_positions.push_back(tempv1);

				sleeper_uvs.push_back(v2[0]);
				sleeper_uvs.push_back(v2[1]);
				sleeper_uvs.push_back(v3[0]);
				sleeper_uvs.push_back(v3[1]);
				sleeper_uvs.push_back(v1[0]);
				sleeper_uvs.push_back(v1[1]);
				sleeper_uvs.push_back(v3[0]);
				sleeper_uvs.push_back(v3[1]);
				sleeper_uvs.push_back(v0[0]);
				sleeper_uvs.push_back(v0[1]);
				sleeper_uvs.push_back(v1[0]);
				sleeper_uvs.push_back(v1[1]);


			}

			if (k % 100 == 0 && k < point_set.size() - 30) {
				/*glm::vec3 tempv2, tempv1;
				for (int i = 0; i < 3; i++) {
					tempv2[i] = left_v2_splinePoint[i] + 0.1 *rail_tangent_set[k][i];
					tempv1[i] = right_v1_splinePoint[i] + 0.1 * rail_tangent_set[k][i];
				}*/
				glm::vec3 v3_bottom_splinePoint = left_v3_splinePoint - glm::vec3(0, left_v3_splinePoint.y + 1, 0);
				glm::vec3 v2_bottom_splinePoint = left_v2_splinePoint - glm::vec3(0, left_v2_splinePoint.y + 1, 0);
				glm::vec3 v1_bottom_splinePoint = right_v1_splinePoint - glm::vec3(0, right_v1_splinePoint.y + 1, 0);
				glm::vec3 v0_bottom_splinePoint = right_v0_splinePoint - glm::vec3(0, right_v0_splinePoint.y + 1, 0);
				support_positions.push_back(left_v2_splinePoint);
				support_positions.push_back(left_v3_splinePoint);
				support_positions.push_back(v3_bottom_splinePoint);

				support_positions.push_back(v3_bottom_splinePoint);
				support_positions.push_back(v2_bottom_splinePoint);
				support_positions.push_back(left_v2_splinePoint);

				support_positions.push_back(right_v1_splinePoint);
				support_positions.push_back(right_v0_splinePoint);
				support_positions.push_back(v0_bottom_splinePoint);

				support_positions.push_back(v0_bottom_splinePoint);
				support_positions.push_back(v1_bottom_splinePoint);
				support_positions.push_back(right_v1_splinePoint);

				support_uvs.push_back(v2[0]);
				support_uvs.push_back(v2[1]);
				support_uvs.push_back(v3[0]);
				support_uvs.push_back(v3[1]);
				support_uvs.push_back(v1[0]);
				support_uvs.push_back(v1[1]);
				support_uvs.push_back(v3[0]);
				support_uvs.push_back(v3[1]);
				support_uvs.push_back(v0[0]);
				support_uvs.push_back(v0[1]);
				support_uvs.push_back(v1[0]);
				support_uvs.push_back(v1[1]);


			}


		}

		for (int k = 0; k < point_set.size() - 10; k++) {
			
		}

}


std::vector<GLfloat> right_skybox_vertices;
std::vector<GLfloat> right_sky_uvs;
std::vector<GLfloat> forward_skybox_vertices;
std::vector<GLfloat> forward_sky_uvs;
std::vector<GLfloat> left_skybox_vertices;
std::vector<GLfloat> left_sky_uvs;
std::vector<GLfloat> backward_skybox_vertices;
std::vector<GLfloat> backward_sky_uvs;

std::vector<GLfloat> top_skybox_vertices;
std::vector<GLfloat> top_sky_uvs;
void generateSkyBox()
{

	//    __
	//   /__/|
	//   |__|/
	GLfloat v0[3] = { -128, -1, -128 };      
	GLfloat v1[3] = { -128, -1, 128 };
	GLfloat v2[3] = { 128, -1, 128 };
	GLfloat v3[3] = { 128, -1, -128 };
	GLfloat v4[3] = { -128, 256, -128 };
	GLfloat v5[3] = { -128, 256, 128 };
	GLfloat v6[3] = { 128, 256, 128 };
	GLfloat v7[3] = { 128, 256, -128 };



	// right triangle1
	right_skybox_vertices.push_back(v0[0]);
	right_skybox_vertices.push_back(v0[1]);
	right_skybox_vertices.push_back(v0[2]);
	right_skybox_vertices.push_back(v1[0]);
	right_skybox_vertices.push_back(v1[1]);
	right_skybox_vertices.push_back(v1[2]);
	right_skybox_vertices.push_back(v4[0]);
	right_skybox_vertices.push_back(v4[1]);
	right_skybox_vertices.push_back(v4[2]);
	// right triangle2
	right_skybox_vertices.push_back(v1[0]);
	right_skybox_vertices.push_back(v1[1]);
	right_skybox_vertices.push_back(v1[2]);
	right_skybox_vertices.push_back(v5[0]);
	right_skybox_vertices.push_back(v5[1]);
	right_skybox_vertices.push_back(v5[2]);
	right_skybox_vertices.push_back(v4[0]);
	right_skybox_vertices.push_back(v4[1]);
	right_skybox_vertices.push_back(v4[2]);


	GLfloat uv1[2] = { 0.0f, 0.0f };    
	GLfloat uv0[2] = { 1.0f, 0.0f };   
	GLfloat uv4[2] = { 1.0f, 1.0f };   
	GLfloat uv5[2] = { 0.0f, 1.0f };    
	// right triangle 1 UV
	right_sky_uvs.push_back(uv0[0]);
	right_sky_uvs.push_back(uv0[1]);
	right_sky_uvs.push_back(uv1[0]);
	right_sky_uvs.push_back(uv1[1]);
	right_sky_uvs.push_back(uv4[0]);
	right_sky_uvs.push_back(uv4[1]);

	// right triangle2 UV
	right_sky_uvs.push_back(uv1[0]);
	right_sky_uvs.push_back(uv1[1]);
	right_sky_uvs.push_back(uv5[0]);
	right_sky_uvs.push_back(uv5[1]);
	right_sky_uvs.push_back(uv4[0]);
	right_sky_uvs.push_back(uv4[1]);


	// forward triangle1........................
	forward_skybox_vertices.push_back(v2[0]);
	forward_skybox_vertices.push_back(v2[1]);
	forward_skybox_vertices.push_back(v2[2]);
	forward_skybox_vertices.push_back(v1[0]);
	forward_skybox_vertices.push_back(v1[1]);
	forward_skybox_vertices.push_back(v1[2]);
	forward_skybox_vertices.push_back(v5[0]);
	forward_skybox_vertices.push_back(v5[1]);
	forward_skybox_vertices.push_back(v5[2]);
	// forwardrward triangle2
	forward_skybox_vertices.push_back(v5[0]);
	forward_skybox_vertices.push_back(v5[1]);
	forward_skybox_vertices.push_back(v5[2]);
	forward_skybox_vertices.push_back(v6[0]);
	forward_skybox_vertices.push_back(v6[1]);
	forward_skybox_vertices.push_back(v6[2]);
	forward_skybox_vertices.push_back(v2[0]);
	forward_skybox_vertices.push_back(v2[1]);
	forward_skybox_vertices.push_back(v2[2]);

	GLfloat uv2[2] = { 0.0f, 0.0f };    
	uv1[0] = 1.0f, uv1[1] = 0.0f;   
	 uv5[0] = 1.0f, uv5[1] = 1.0f;    
	GLfloat uv6[2] = { 0.0f, 1.0f };   
	// forward triangle 1 UV
	forward_sky_uvs.push_back(uv2[0]);
	forward_sky_uvs.push_back(uv2[1]);
	forward_sky_uvs.push_back(uv1[0]);
	forward_sky_uvs.push_back(uv1[1]);
	forward_sky_uvs.push_back(uv5[0]);
	forward_sky_uvs.push_back(uv5[1]);
	//forwa_sky_uvsrd triangle 2uv
	forward_sky_uvs.push_back(uv5[0]);
	forward_sky_uvs.push_back(uv5[1]);
	forward_sky_uvs.push_back(uv6[0]);
	forward_sky_uvs.push_back(uv6[1]);
	forward_sky_uvs.push_back(uv2[0]);
	forward_sky_uvs.push_back(uv2[1]);



	//left triangle1..................................
	left_skybox_vertices.push_back(v7[0]);
	left_skybox_vertices.push_back(v7[1]);
	left_skybox_vertices.push_back(v7[2]);
	left_skybox_vertices.push_back(v3[0]);
	left_skybox_vertices.push_back(v3[1]);
	left_skybox_vertices.push_back(v3[2]);
	left_skybox_vertices.push_back(v2[0]);
	left_skybox_vertices.push_back(v2[1]);
	left_skybox_vertices.push_back(v2[2]);
	// left triangle2
	left_skybox_vertices.push_back(v2[0]);
	left_skybox_vertices.push_back(v2[1]);
	left_skybox_vertices.push_back(v2[2]);
	left_skybox_vertices.push_back(v6[0]);
	left_skybox_vertices.push_back(v6[1]);
	left_skybox_vertices.push_back(v6[2]);
	left_skybox_vertices.push_back(v7[0]);
	left_skybox_vertices.push_back(v7[1]);
	left_skybox_vertices.push_back(v7[2]);

	GLfloat uv3[2] = { 0.0f, 0.0f };   
	uv2[0] = 1.0f, uv2[1] = 0.0f;  
	uv6[0] = 1.0f, uv5[1] = 1.0f;    
	GLfloat uv7[2] = { 0.0f, 1.0f };   
	// /left_sky triangle 1 UV
	left_sky_uvs.push_back(uv7[0]);
	left_sky_uvs.push_back(uv7[1]);
	left_sky_uvs.push_back(uv3[0]);
	left_sky_uvs.push_back(uv3[1]);
	left_sky_uvs.push_back(uv2[0]);
	left_sky_uvs.push_back(uv2[1]);
	//left_sky_uvsrd triangle 2uv
	left_sky_uvs.push_back(uv2[0]);
	left_sky_uvs.push_back(uv2[1]);
	left_sky_uvs.push_back(uv6[0]);
	left_sky_uvs.push_back(uv6[1]);
	left_sky_uvs.push_back(uv7[0]);
	left_sky_uvs.push_back(uv7[1]);


	//backward triangle1..................................
	backward_skybox_vertices.push_back(v3[0]);
	backward_skybox_vertices.push_back(v3[1]);
	backward_skybox_vertices.push_back(v3[2]);
	backward_skybox_vertices.push_back(v0[0]);
	backward_skybox_vertices.push_back(v0[1]);
	backward_skybox_vertices.push_back(v0[2]);
	backward_skybox_vertices.push_back(v7[0]);
	backward_skybox_vertices.push_back(v7[1]);
	backward_skybox_vertices.push_back(v7[2]);
	//backwardeft triangle2
	backward_skybox_vertices.push_back(v0[0]);
	backward_skybox_vertices.push_back(v0[1]);
	backward_skybox_vertices.push_back(v0[2]);
	backward_skybox_vertices.push_back(v4[0]);
	backward_skybox_vertices.push_back(v4[1]);
	backward_skybox_vertices.push_back(v4[2]);
	backward_skybox_vertices.push_back(v7[0]);
	backward_skybox_vertices.push_back(v7[1]);
	backward_skybox_vertices.push_back(v7[2]);

	uv0[0] = 0.0f, uv0[1] = 0.0f;    
	uv3[0] = 1.0f, uv3[1] = 0.0f;   
	uv4[0] = 0.0f, uv4[1] = 1.0f;   
	uv7[0] = 1.0f, uv7[1] = 1.0f;   
	// backward triangle 1 UV
	backward_sky_uvs.push_back(uv3[0]);
	backward_sky_uvs.push_back(uv3[1]);
	backward_sky_uvs.push_back(uv0[0]);
	backward_sky_uvs.push_back(uv0[1]);
	backward_sky_uvs.push_back(uv7[0]);
	backward_sky_uvs.push_back(uv7[1]);
	//backward
	backward_sky_uvs.push_back(uv0[0]);
	backward_sky_uvs.push_back(uv0[1]);
	backward_sky_uvs.push_back(uv4[0]);
	backward_sky_uvs.push_back(uv4[1]);
	backward_sky_uvs.push_back(uv7[0]);
	backward_sky_uvs.push_back(uv7[1]);
	

	//top triangle1..................................
	top_skybox_vertices.push_back(v4[0]);
	top_skybox_vertices.push_back(v4[1]);
	top_skybox_vertices.push_back(v4[2]);
	top_skybox_vertices.push_back(v5[0]);
	top_skybox_vertices.push_back(v5[1]);
	top_skybox_vertices.push_back(v5[2]);
	top_skybox_vertices.push_back(v6[0]);
	top_skybox_vertices.push_back(v6[1]);
	top_skybox_vertices.push_back(v6[2]);
	//topiangle2
	top_skybox_vertices.push_back(v6[0]);
	top_skybox_vertices.push_back(v6[1]);
	top_skybox_vertices.push_back(v6[2]);
	top_skybox_vertices.push_back(v7[0]);
	top_skybox_vertices.push_back(v7[1]);
	top_skybox_vertices.push_back(v7[2]);
	top_skybox_vertices.push_back(v4[0]);
	top_skybox_vertices.push_back(v4[1]);
	top_skybox_vertices.push_back(v4[2]);

	uv5[0] = 1.0f, uv5[1] = 0.0f;    
	uv6[0] = 0.0f, uv6[1] = 0.0f;  
	uv4[0] = 1.0f, uv4[1] = 1.0f;    
	uv7[0] = 0.0f, uv7[1] = 1.0f;    
	// backward triangle 1 UV
	top_sky_uvs.push_back(uv4[0]);
	top_sky_uvs.push_back(uv4[1]);
	top_sky_uvs.push_back(uv5[0]);
	top_sky_uvs.push_back(uv5[1]);
	top_sky_uvs.push_back(uv6[0]);
	top_sky_uvs.push_back(uv6[1]);
	//toprd
	top_sky_uvs.push_back(uv6[0]);
	top_sky_uvs.push_back(uv6[1]);
	top_sky_uvs.push_back(uv7[0]);
	top_sky_uvs.push_back(uv7[1]);
	top_sky_uvs.push_back(uv4[0]);
	top_sky_uvs.push_back(uv4[1]);



}

void generateGround()
{
	GLfloat v0[3] = { -128, -1, -128 };
	GLfloat v1[3] = { -128, -1, 128 };
	GLfloat v2[3] = { 128, -1, 128 };
	GLfloat v3[3] = { 128, -1, -128 };
	

	ground_vertices.push_back(v2[0]);
	ground_vertices.push_back(v2[1]);
	ground_vertices.push_back(v2[2]);
	ground_vertices.push_back(v3[0]);
	ground_vertices.push_back(v3[1]);
	ground_vertices.push_back(v3[2]);
	ground_vertices.push_back(v0[0]);
	ground_vertices.push_back(v0[1]);
	ground_vertices.push_back(v0[2]);

	ground_vertices.push_back(v0[0]);
	ground_vertices.push_back(v0[1]);
	ground_vertices.push_back(v0[2]);
	ground_vertices.push_back(v1[0]);
	ground_vertices.push_back(v1[1]);
	ground_vertices.push_back(v1[2]);
	ground_vertices.push_back(v2[0]);
	ground_vertices.push_back(v2[1]);
	ground_vertices.push_back(v2[2]);

	GLfloat v0_uv[2] = { 1.0f, 0.0f };    //bottom left uv
	GLfloat v1_uv[2] = { 1.0f, 1.0f };    //bottom right uv
	GLfloat v2_uv[2] = { 0.0f, 1.0f };    //top left uv
	GLfloat v3_uv[2] = { 0.0f, 0.0f };    //top right uv

	ground_uvs.push_back(v2_uv[0]);
	ground_uvs.push_back(v2_uv[1]);
	ground_uvs.push_back(v3_uv[0]);
	ground_uvs.push_back(v3_uv[1]);
	ground_uvs.push_back(v0_uv[0]);
	ground_uvs.push_back(v0_uv[1]);

	ground_uvs.push_back(v0_uv[0]);
	ground_uvs.push_back(v0_uv[1]);
	ground_uvs.push_back(v1_uv[0]);
	ground_uvs.push_back(v1_uv[1]);
	ground_uvs.push_back(v2_uv[0]);
	ground_uvs.push_back(v2_uv[1]);

}

GLuint vbo_left_splinePoint_positions;
GLuint red_texture_handle;
GLuint vao_spline_points;
GLuint vbo_spline_point_uvs;
GLuint vbo_spline_color;
GLuint vbo_left_spline_normal;

GLuint vbo_right_splinePoint_positions;
GLuint vbo_right_spline_normal;

GLuint vbo_sleeper_positions;
GLuint vbo_sleeper_normal;
GLuint vbo_sleeper_color;
GLuint vbo_sleeper_uvs;
GLuint sleeper_texture_handle;

GLuint vbo_support_positions;
GLuint vbo_support_normal;
GLuint vbo_support_uvs;
GLuint support_texture_handle;



GLuint skybox_forward_texture_handle;
GLuint skybox_right_texture_handle;


GLuint vao_sky_texture;
GLuint vbo_sky_vertices;
GLuint vbo_sky_uvs;

GLuint vbo_forward_sky_vertices;
GLuint vbo_forward_sky_uvs;

GLuint skybox_left_texture_handle;
GLuint vbo_left_sky_vertices;
GLuint vbo_left_sky_uvs;

GLuint skybox_backward_texture_handle;
GLuint vbo_backward_sky_vertices;
GLuint vbo_backward_sky_uvs;

GLuint skybox_top_texture_handle;
GLuint vbo_top_sky_vertices;
GLuint vbo_top_sky_uvs;
void initVao_Vbo() {

	
	//spline points VAO and VBOs
	glGenVertexArrays(1, &vao_spline_points);
	glBindVertexArray(vao_spline_points); //bind the points VAO
	// upload points position data
	glGenBuffers(1, &vbo_left_splinePoint_positions);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_left_splinePoint_positions);
	glBufferData(GL_ARRAY_BUFFER, left_splinePoint_positions.size() * sizeof(glm::vec3), &left_splinePoint_positions[0], GL_STATIC_DRAW);

	
	glGenBuffers(1, &vbo_left_spline_normal);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_left_spline_normal);
	glBufferData(GL_ARRAY_BUFFER, left_splinePoint_normal.size() * sizeof(glm::vec3), &left_splinePoint_normal[0], GL_STATIC_DRAW);

	glGenBuffers(1, &vbo_right_splinePoint_positions);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_right_splinePoint_positions);
	glBufferData(GL_ARRAY_BUFFER, right_splinePoint_positions.size() * sizeof(glm::vec3), &right_splinePoint_positions[0], GL_STATIC_DRAW);


	glGenBuffers(1, &vbo_right_spline_normal);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_right_spline_normal);
	glBufferData(GL_ARRAY_BUFFER, right_splinePoint_normal.size() * sizeof(glm::vec3), &right_splinePoint_normal[0], GL_STATIC_DRAW);
	


	// set sleeper 
	glGenTextures(1, &sleeper_texture_handle);
	initTexture("textures/wood.jpg", sleeper_texture_handle);
	
	glGenBuffers(1, &vbo_sleeper_positions); // get handle on VBO buffer
	glBindBuffer(GL_ARRAY_BUFFER, vbo_sleeper_positions);  // bind the VBO buffer
	// glBufferData() can allocate memory but glBufferSubData() cannot
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * sleeper_positions.size(), sleeper_positions.data(), GL_STATIC_DRAW); // to allocate memory

	glGenBuffers(1, &vbo_sleeper_uvs);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_sleeper_uvs);
	glBufferData(GL_ARRAY_BUFFER, sleeper_uvs.size() * sizeof(GLfloat), &sleeper_uvs[0], GL_STATIC_DRAW);
	
	// set support
	glGenTextures(1, &support_texture_handle);
	initTexture("textures/gold.jpg", support_texture_handle);

	glGenBuffers(1, &vbo_support_positions); // get handle on VBO buffer
	glBindBuffer(GL_ARRAY_BUFFER, vbo_support_positions);  // bind the VBO buffer
	// glBufferData() can allocate memory but glBufferSubData() cannot
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * support_positions.size(), support_positions.data(), GL_STATIC_DRAW); // to allocate memory

	glGenBuffers(1, &vbo_support_uvs);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_support_uvs);
	glBufferData(GL_ARRAY_BUFFER, support_uvs.size() * sizeof(GLfloat), &support_uvs[0], GL_STATIC_DRAW);

	glDisableVertexAttribArray(0);

	// set ground 
	glGenTextures(1, &ground_texture_handle);
	initTexture("skybox/negy.jpg", ground_texture_handle);

	//bind ground texture VAO
	glGenVertexArrays(1, &vao_ground_texture);
	glBindVertexArray(vao_ground_texture);

	//upload vertices data
	glGenBuffers(1, &vbo_ground_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_ground_vertices);
	glBufferData(GL_ARRAY_BUFFER, ground_vertices.size() * sizeof(GLfloat), &ground_vertices[0], GL_STATIC_DRAW);

	//upload uv data
	glGenBuffers(1, &vbo_ground_uvs);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_ground_uvs);
	glBufferData(GL_ARRAY_BUFFER, ground_uvs.size() * sizeof(GLfloat), &ground_uvs[0], GL_STATIC_DRAW);

	// render the sky.....................
	// set right sky 
	glGenTextures(1, &skybox_right_texture_handle);
	initTexture("skybox/posx.jpg", skybox_right_texture_handle);

	//bind ground texture VAO
	glGenVertexArrays(1, &vao_sky_texture);
	glBindVertexArray(vao_sky_texture);

	//upload vertices data
	glGenBuffers(1, &vbo_sky_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_sky_vertices);
	glBufferData(GL_ARRAY_BUFFER, right_skybox_vertices.size() * sizeof(GLfloat), &right_skybox_vertices[0], GL_STATIC_DRAW);

	//upload uv data
	glGenBuffers(1, &vbo_sky_uvs);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_sky_uvs);
	glBufferData(GL_ARRAY_BUFFER, right_sky_uvs.size() * sizeof(GLfloat), &right_sky_uvs[0], GL_STATIC_DRAW);

	glDisableVertexAttribArray(0);

	// set foward sky 
	glGenTextures(1, &skybox_forward_texture_handle);
	initTexture("skybox/posz.jpg", skybox_forward_texture_handle);
	//bind ground texture VAO
	glGenVertexArrays(1, &vao_sky_texture);
	glBindVertexArray(vao_sky_texture);
	//upload vertices data
	glGenBuffers(1, &vbo_forward_sky_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_forward_sky_vertices);
	glBufferData(GL_ARRAY_BUFFER, forward_skybox_vertices.size() * sizeof(GLfloat), &forward_skybox_vertices[0], GL_STATIC_DRAW);
	//upload uv data
	glGenBuffers(1, &vbo_forward_sky_uvs);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_forward_sky_uvs);
	glBufferData(GL_ARRAY_BUFFER, forward_sky_uvs.size() * sizeof(GLfloat), &forward_sky_uvs[0], GL_STATIC_DRAW);
	glDisableVertexAttribArray(0);

	// set left sky 
	glGenTextures(1, &skybox_left_texture_handle);
	initTexture("skybox/negx.jpg", skybox_left_texture_handle);
	//bind ground texture VAO
	glGenVertexArrays(1, &vao_sky_texture);
	glBindVertexArray(vao_sky_texture);
	//upload vertices data
	glGenBuffers(1, &vbo_left_sky_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_left_sky_vertices);
	glBufferData(GL_ARRAY_BUFFER, left_skybox_vertices.size() * sizeof(GLfloat), &left_skybox_vertices[0], GL_STATIC_DRAW);
	//upload uv data
	glGenBuffers(1, &vbo_left_sky_uvs);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_left_sky_uvs);
	glBufferData(GL_ARRAY_BUFFER, left_sky_uvs.size() * sizeof(GLfloat), &left_sky_uvs[0], GL_STATIC_DRAW);
	glDisableVertexAttribArray(0);


	// set backward sky 
	glGenTextures(1, &skybox_backward_texture_handle);
	initTexture("skybox/negz.jpg", skybox_backward_texture_handle);
	//bind ground texture VAO
	glGenVertexArrays(1, &vao_sky_texture);
	glBindVertexArray(vao_sky_texture);
	//upload vertices data
	glGenBuffers(1, &vbo_backward_sky_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_backward_sky_vertices);
	glBufferData(GL_ARRAY_BUFFER, backward_skybox_vertices.size() * sizeof(GLfloat), &backward_skybox_vertices[0], GL_STATIC_DRAW);
	//upload uv data
	glGenBuffers(1, &vbo_backward_sky_uvs);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_backward_sky_uvs);
	glBufferData(GL_ARRAY_BUFFER, backward_sky_uvs.size() * sizeof(GLfloat), &backward_sky_uvs[0], GL_STATIC_DRAW);
	glDisableVertexAttribArray(0);

	// set top sky 
	glGenTextures(1, &skybox_top_texture_handle);
	initTexture("skybox/posy.jpg", skybox_top_texture_handle);
	//bind ground texture VAO
	glGenVertexArrays(1, &vao_sky_texture);
	glBindVertexArray(vao_sky_texture);
	//upload vertices data
	glGenBuffers(1, &vbo_top_sky_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_top_sky_vertices);
	glBufferData(GL_ARRAY_BUFFER, top_skybox_vertices.size() * sizeof(GLfloat), &top_skybox_vertices[0], GL_STATIC_DRAW);
	//upload uv data
	glGenBuffers(1, &vbo_top_sky_uvs);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_top_sky_uvs);
	glBufferData(GL_ARRAY_BUFFER, top_sky_uvs.size() * sizeof(GLfloat), &top_sky_uvs[0], GL_STATIC_DRAW);
	glDisableVertexAttribArray(0);

}

void phongLightSetting() {
	// get a handle to the program
	GLuint program = pipelineProgram->GetProgramHandle();
	// get a handle to the viewLightDirection shader variable
	GLint h_viewLightDirection =
		glGetUniformLocation(program, "viewLightDirection");
	float lightDirection[3] = { 0, 0, 1 };
	float viewLightDirection[3];
	float view[16]; // column-major
	matrix.SetMatrixMode(OpenGLMatrix::ModelView);
	//matrix.LoadIdentity();
	matrix.GetMatrix(view);
	//for (int i = 0; i < 16; i++) {
	//	printf("%f ", view[i]);
	//}
	viewLightDirection[0] = (view[0] * lightDirection[0]) + (view[4] * lightDirection[1]) + (view[8] * lightDirection[2]);
	viewLightDirection[1] = (view[1] * lightDirection[0]) + (view[5] * lightDirection[1]) + (view[9] * lightDirection[2]);
	viewLightDirection[2] = (view[2] * lightDirection[0]) + (view[6] * lightDirection[1]) + (view[10] * lightDirection[2]);
	//printf("%f %f %f \n", viewLightDirection[0], viewLightDirection[1], viewLightDirection[2]);
	glUniform3fv(h_viewLightDirection, 1, viewLightDirection);

}
void renderSplines() {
	//position data
	//glBindTexture(GL_TEXTURE_2D, red_texture_handle);
	 //bind vbo_left_splinePoint_positions 
	
	GLint loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	glUniform1i(loc, 0);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_left_splinePoint_positions);
	GLuint loc1 = glGetAttribLocation(program, "position"); //get the location of the "position" shader variable
	glEnableVertexAttribArray(loc1); //enable the "position" attribute
	const void * offset = (const void*)0;
	GLsizei stride = 0;
	GLboolean normalized = GL_FALSE;
	//set the layout of the “position” attribute data
	glVertexAttribPointer(loc1, 3, GL_FLOAT, normalized, stride, offset);


	glBindBuffer(GL_ARRAY_BUFFER, vbo_left_spline_normal);
	GLuint loc3 = glGetAttribLocation(program, "normal"); //get the location of the "position" shader variable
	glEnableVertexAttribArray(loc3); //enable the "position" attribute
	offset = (const void*)0;
	stride = 0;
	normalized = GL_FALSE;
	//set the layout of the “position” attribute data
	glVertexAttribPointer(loc3, 3, GL_FLOAT, normalized, stride, offset);
	glEnable(GL_DEPTH_TEST);
	
	phongLightSetting();
	glDrawArrays(GL_TRIANGLES, 0, left_splinePoint_positions.size());

	glDisableVertexAttribArray(loc1);
	glDisableVertexAttribArray(loc3);
	glDisableVertexAttribArray(loc);

	// sleeper.....
	glBindTexture(GL_TEXTURE_2D, sleeper_texture_handle);
	loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	glUniform1i(loc, 1);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_sleeper_positions);
	// get location index of the "position" shader variable
	loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position");
	glEnableVertexAttribArray(loc); // enable the "position" attribute
	offset = (const void*)0;
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, offset); // set the layout of the "position" attribute data


	glBindBuffer(GL_ARRAY_BUFFER, vbo_sleeper_uvs); //bind the vbo_ground_uvs
	GLuint loc2 = glGetAttribLocation(program, "atexCoord"); //get the location of the "texCoord" shader variable
	glEnableVertexAttribArray(loc2); //enable the "texCoord" attribute
	//set the layout of the "texCoord" attribute data
	glVertexAttribPointer(loc2, 2, GL_FLOAT, normalized, stride, offset);
	glDrawArrays(GL_TRIANGLES, 0, sleeper_positions.size());
	////

	// supporter...
	glBindTexture(GL_TEXTURE_2D, support_texture_handle);
	loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	glUniform1i(loc, 1);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_support_positions);
	// get location index of the "position" shader variable
	loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position");
	glEnableVertexAttribArray(loc); // enable the "position" attribute
	offset = (const void*)0;
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, offset); // set the layout of the "position" attribute data


	glBindBuffer(GL_ARRAY_BUFFER, vbo_support_uvs); //bind the vbo_ground_uvs
	loc2 = glGetAttribLocation(program, "atexCoord"); //get the location of the "texCoord" shader variable
	glEnableVertexAttribArray(loc2); //enable the "texCoord" attribute
	//set the layout of the "texCoord" attribute data
	glVertexAttribPointer(loc2, 2, GL_FLOAT, normalized, stride, offset);
	glDrawArrays(GL_TRIANGLES, 0, support_positions.size());
	

	// render right splines

	loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	glUniform1i(loc, 0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_right_splinePoint_positions);
	loc1 = glGetAttribLocation(program, "position"); //get the location of the "position" shader variable
	glEnableVertexAttribArray(loc1); //enable the "position" attribute
	offset = (const void*)0;
	stride = 0;
	normalized = GL_FALSE;
	//set the layout of the “position” attribute data
	glVertexAttribPointer(loc1, 3, GL_FLOAT, normalized, stride, offset);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_right_spline_normal);
	loc3 = glGetAttribLocation(program, "normal"); //get the location of the "position" shader variable
	glEnableVertexAttribArray(loc3); //enable the "position" attribute
	offset = (const void*)0;
	stride = 0;
	normalized = GL_FALSE;
	//set the layout of the “position” attribute data
	glVertexAttribPointer(loc3, 3, GL_FLOAT, normalized, stride, offset);
	glEnable(GL_DEPTH_TEST);

	phongLightSetting();
	glDrawArrays(GL_TRIANGLES, 0, left_splinePoint_positions.size());

	glDisableVertexAttribArray(loc1);
	glDisableVertexAttribArray(loc3);
	glDisableVertexAttribArray(loc);

}

void renderGround()
{
	//position data
	glBindTexture(GL_TEXTURE_2D, ground_texture_handle);

	GLint loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	glUniform1i(loc, 1);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_ground_vertices); //bind vbo_ground_vertices
	GLuint loc1 = glGetAttribLocation(program, "position"); //get the location of the "position" shader variables
	glEnableVertexAttribArray(loc1); //enalbe the "position" attribute
	const void * offset = (const void*)0;
	GLsizei stride = 0;
	GLboolean normalized = GL_FALSE;
	//set the layout of the "position" attribute data
	glVertexAttribPointer(loc1, 3, GL_FLOAT, normalized, stride, offset);

	//texture data
	glBindBuffer(GL_ARRAY_BUFFER, vbo_ground_uvs); //bind the vbo_ground_uvs
	GLuint loc2 = glGetAttribLocation(program, "atexCoord"); //get the location of the "texCoord" shader variable
	glEnableVertexAttribArray(loc2); //enable the "texCoord" attribute
	//set the layout of the "texCoord" attribute data
	glVertexAttribPointer(loc2, 2, GL_FLOAT, normalized, stride, offset);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDrawArrays(GL_TRIANGLES, 0, ground_vertices.size() / 3);

	glDisableVertexAttribArray(loc1);
	glDisableVertexAttribArray(loc2);
}

void renderPosxSky() {
	//render posx skybox.
	glBindTexture(GL_TEXTURE_2D, skybox_right_texture_handle);

	GLint loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	glUniform1i(loc, 1);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_sky_vertices); //bind vbo_ground_vertices
	GLuint loc1 = glGetAttribLocation(program, "position"); //get the location of the "position" shader variables
	glEnableVertexAttribArray(loc1); //enalbe the "position" attribute
	const void * offset = (const void*)0;
	GLsizei stride = 0;
	GLboolean normalized = GL_FALSE;
	//set the layout of the "position" attribute data
	glVertexAttribPointer(loc1, 3, GL_FLOAT, normalized, stride, offset);

	//texture data
	glBindBuffer(GL_ARRAY_BUFFER, vbo_sky_uvs); //bind the vbo_ground_uvs
	GLuint loc2 = glGetAttribLocation(program, "atexCoord"); //get the location of the "texCoord" shader variable
	glEnableVertexAttribArray(loc2); //enable the "texCoord" attribute
	//set the layout of the "texCoord" attribute data
	glVertexAttribPointer(loc2, 2, GL_FLOAT, normalized, stride, offset);


	glDrawArrays(GL_TRIANGLES, 0, right_skybox_vertices.size() / 3);

	glDisableVertexAttribArray(loc1);
	glDisableVertexAttribArray(loc2);

	
}

void renderNegxSky() {
	//render posx skybox.
	glBindTexture(GL_TEXTURE_2D, skybox_left_texture_handle);

	GLint loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	glUniform1i(loc, 1);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_left_sky_vertices); //bind vbo_ground_vertices
	GLuint loc1 = glGetAttribLocation(program, "position"); //get the location of the "position" shader variables
	glEnableVertexAttribArray(loc1); //enalbe the "position" attribute
	const void * offset = (const void*)0;
	GLsizei stride = 0;
	GLboolean normalized = GL_FALSE;
	//set the layout of the "position" attribute data
	glVertexAttribPointer(loc1, 3, GL_FLOAT, normalized, stride, offset);

	//texture data
	glBindBuffer(GL_ARRAY_BUFFER, vbo_left_sky_uvs); //bind the vbo_ground_uvs
	GLuint loc2 = glGetAttribLocation(program, "atexCoord"); //get the location of the "texCoord" shader variable
	glEnableVertexAttribArray(loc2); //enable the "texCoord" attribute
	//set the layout of the "texCoord" attribute data
	glVertexAttribPointer(loc2, 2, GL_FLOAT, normalized, stride, offset);


	glDrawArrays(GL_TRIANGLES, 0, left_skybox_vertices.size() / 3);

	glDisableVertexAttribArray(loc1);
	glDisableVertexAttribArray(loc2);


}

void renderPoszsky() {
	//render posz skybox.
	glBindTexture(GL_TEXTURE_2D, skybox_forward_texture_handle);

	GLint loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	glUniform1i(loc, 1);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_forward_sky_vertices); //bind vbo_ground_vertices
	GLint loc1 = glGetAttribLocation(program, "position"); //get the location of the "position" shader variables
	glEnableVertexAttribArray(loc1); //enalbe the "position" attribute
	const void * offset = (const void*)0;
	GLsizei stride = 0;
	GLboolean normalized = GL_FALSE;
	//set the layout of the "position" attribute data
	glVertexAttribPointer(loc1, 3, GL_FLOAT, normalized, stride, offset);

	//texture data
	glBindBuffer(GL_ARRAY_BUFFER, vbo_forward_sky_uvs); //bind the vbo_ground_uvs
	GLint loc2 = glGetAttribLocation(program, "atexCoord"); //get the location of the "texCoord" shader variable
	glEnableVertexAttribArray(loc2); //enable the "texCoord" attribute
	//set the layout of the "texCoord" attribute data
	glVertexAttribPointer(loc2, 2, GL_FLOAT, normalized, stride, offset);


	glDrawArrays(GL_TRIANGLES, 0, forward_skybox_vertices.size() / 3);

	glDisableVertexAttribArray(loc1);
	glDisableVertexAttribArray(loc2);
}

void renderNegzsky() {
	//render posz skybox.
	glBindTexture(GL_TEXTURE_2D, skybox_backward_texture_handle);

	GLint loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	glUniform1i(loc, 1);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_backward_sky_vertices); //bind vbo_ground_vertices
	GLint loc1 = glGetAttribLocation(program, "position"); //get the location of the "position" shader variables
	glEnableVertexAttribArray(loc1); //enalbe the "position" attribute
	const void * offset = (const void*)0;
	GLsizei stride = 0;
	GLboolean normalized = GL_FALSE;
	//set the layout of the "position" attribute data
	glVertexAttribPointer(loc1, 3, GL_FLOAT, normalized, stride, offset);

	//texture data
	glBindBuffer(GL_ARRAY_BUFFER, vbo_backward_sky_uvs); //bind the vbo_ground_uvs
	GLint loc2 = glGetAttribLocation(program, "atexCoord"); //get the location of the "texCoord" shader variable
	glEnableVertexAttribArray(loc2); //enable the "texCoord" attribute
	//set the layout of the "texCoord" attribute data
	glVertexAttribPointer(loc2, 2, GL_FLOAT, normalized, stride, offset);


	glDrawArrays(GL_TRIANGLES, 0, backward_skybox_vertices.size() / 3);

	glDisableVertexAttribArray(loc1);
	glDisableVertexAttribArray(loc2);
}

void renderPosysky() {
	//render posz skybox.
	glBindTexture(GL_TEXTURE_2D, skybox_top_texture_handle);

	GLint loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	glUniform1i(loc, 1);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_top_sky_vertices); //bind vbo_ground_vertices
	GLint loc1 = glGetAttribLocation(program, "position"); //get the location of the "position" shader variables
	glEnableVertexAttribArray(loc1); //enalbe the "position" attribute
	const void * offset = (const void*)0;
	GLsizei stride = 0;
	GLboolean normalized = GL_FALSE;
	//set the layout of the "position" attribute data
	glVertexAttribPointer(loc1, 3, GL_FLOAT, normalized, stride, offset);

	//texture data
	glBindBuffer(GL_ARRAY_BUFFER, vbo_top_sky_uvs); //bind the vbo_ground_uvs
	GLint loc2 = glGetAttribLocation(program, "atexCoord"); //get the location of the "texCoord" shader variable
	glEnableVertexAttribArray(loc2); //enable the "texCoord" attribute
	//set the layout of the "texCoord" attribute data
	glVertexAttribPointer(loc2, 2, GL_FLOAT, normalized, stride, offset);


	glDrawArrays(GL_TRIANGLES, 0, top_skybox_vertices.size() / 3);

	glDisableVertexAttribArray(loc1);
	glDisableVertexAttribArray(loc2);
}


void renderSkyBox()
{
	renderPosxSky();
	renderPoszsky();
	renderNegxSky();
	renderNegzsky();
	renderPosysky();
}


int curr = 0;
void displayFunc()
{
	// render some stuff...
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	pipelineProgram->Bind();

	
	if (curr <point_set.size() - 5  ) {
		// move the rail a little bit upper the rail,
		glm::vec3 pos = point_set[curr] - 0.5f * rail_binormal_set[curr];
		glm::vec3 lookat = point_set[curr] + rail_tangent_set[curr];
		glm::vec3 up = - rail_binormal_set[curr];
		matrix.SetMatrixMode(OpenGLMatrix::ModelView);
		matrix.LoadIdentity();
		matrix.LookAt(pos.x, pos.y, pos.z, lookat.x, lookat.y, lookat.z, up.x, up.y, up.z);
		curr++;
		//take a screenshot
	}
	else {
		/*glm::vec3 pos = point_set[curr] + 0.5f *  rail_normal_set[curr];
		glm::vec3 lookat = point_set[curr] + rail_tangent_set[curr];
		glm::vec3 up = rail_normal_set[curr];*/
		matrix.SetMatrixMode(OpenGLMatrix::ModelView);
		matrix.LoadIdentity();
		matrix.LookAt(50,  50, 50, 0, 0, 0, 0, 1, 0);
	}
	

	////transformation
	matrix.Translate(landTranslate[0], landTranslate[1], landTranslate[2]);
	matrix.Rotate(landRotate[0], 1.0f, 0.0f, 0.0f);
	matrix.Rotate(landRotate[1], 0.0f, 1.0f, 0.0f);
	matrix.Rotate(landRotate[2], 0.0f, 0.0f, 1.0f);
	matrix.Scale(landScale[0], landScale[1], landScale[2]);

	//upload model-view matrix to GPU
	float m[16]; // column-major
	matrix.GetMatrix(m);
	pipelineProgram->SetModelViewMatrix(m);

	matrix.SetMatrixMode(OpenGLMatrix::Projection);
	matrix.LoadIdentity();
	matrix.Perspective(60.0f, 1.5* 255 / 255, 0.01f, 1000.0f);

	//upload projection matrix to GPU
	float p[16]; // column-major
	matrix.GetMatrix(p);
	pipelineProgram->SetProjectionMatrix(p);
	renderGround();
	renderSplines();
	renderSkyBox();
	//renderGround();
	glutSwapBuffers();
}

int curr_point = 0;
int num_screenshots = 0;
void idleFunc()
{
	//// do some stuff... 
	if (curr_point < point_set.size() - 4 && num_screenshots <= 500) {
		curr_point++;
		if (curr_point % 50 == 0) {
			char anim_num[5];
			sprintf(anim_num, "%03d", num_screenshots);
			saveScreenshot(("animation\\" + std::string(anim_num) + ".jpg").c_str());
			num_screenshots++;
		}
	}
	
	glutPostRedisplay();
}

void reshapeFunc(int w, int h)
{
	glViewport(0, 0, w, h);

	matrix.SetMatrixMode(OpenGLMatrix::Projection);
	matrix.LoadIdentity();
	matrix.Perspective(54.0f, (float)w / (float)h, 0.01f, 100.0f);
	matrix.SetMatrixMode(OpenGLMatrix::ModelView);
}

void mouseMotionDragFunc(int x, int y)
{
	// mouse has moved and one of the mouse buttons is pressed (dragging)

	// the change in mouse position since the last invocation of this function
	int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

	switch (controlState)
	{
		// translate the landscape
	case TRANSLATE:
		if (leftMouseButton)
		{
			// control x,y translation via the left mouse button
			landTranslate[0] += mousePosDelta[0] * 0.01f;
			landTranslate[1] -= mousePosDelta[1] * 0.01f;
		}
		if (middleMouseButton)
		{
			// control z translation via the middle mouse button
			landTranslate[2] += mousePosDelta[1] * 0.01f;
		}
		break;

		// rotate the landscape
	case ROTATE:
		if (leftMouseButton)
		{
			// control x,y rotation via the left mouse button
			landRotate[0] += mousePosDelta[1];
			landRotate[1] += mousePosDelta[0];
		}
		if (middleMouseButton)
		{
			// control z rotation via the middle mouse button
			landRotate[2] += mousePosDelta[1];
		}
		break;

		// scale the landscape
	case SCALE:
		if (leftMouseButton)
		{
			// control x,y scaling via the left mouse button
			landScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
			landScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
		}
		if (middleMouseButton)
		{
			// control z scaling via the middle mouse button
			landScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
		}
		break;
	}

	// store the new mouse position
	mousePos[0] = x;
	mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
	// mouse has moved
	// store the new mouse position
	mousePos[0] = x;
	mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
	// a mouse button has has been pressed or depressed

	// keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables
	switch (button)
	{
	case GLUT_LEFT_BUTTON:
		leftMouseButton = (state == GLUT_DOWN);
		break;

	case GLUT_MIDDLE_BUTTON:
		middleMouseButton = (state == GLUT_DOWN);
		break;

	case GLUT_RIGHT_BUTTON:
		rightMouseButton = (state == GLUT_DOWN);
		break;
	}

	// keep track of whether CTRL and SHIFT keys are pressed
	switch (glutGetModifiers())
	{
	case GLUT_ACTIVE_CTRL:
		controlState = TRANSLATE;
		break;

	case GLUT_ACTIVE_SHIFT:
		controlState = SCALE;
		break;

		// if CTRL and SHIFT are not pressed, we are in rotate mode
	default:
		controlState = ROTATE;
		break;
	}

	// store the new mouse position
	mousePos[0] = x;
	mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y)
{
	GLint loc = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
	switch (key)
	{
	case 27: // ESC key
		exit(0); // exit the program
		break;

	//case ' ':
	//	cout << "You pressed the spacebar." << endl;
	//	break;

	case 'x':
		// take a screenshot
		saveScreenshot("animation\\screenshot.jpg");
		break;
	case '1':
		// point display
		render_mode = P;
		glUniform1i(loc, 0);
		break;
	case '2':
		// render
		render_mode = W;
		glUniform1i(loc, 0);
		break;
	//case '3':
	//	// triangle
	//	render_mode = T;
	//	glUniform1i(loc, 0);
	//	break;
	//case '4':
	//	// smooth
	//	render_mode = S;
	//	glUniform1i(loc, 1);
	//	break;
	}
}


void initScene(int argc, char *argv[])
{
	// load the image from a jpeg disk file to main memory
	//heightmapImage = new ImageIO();
	//if (heightmapImage->loadJPEG(argv[1]) != ImageIO::OK)
	//{
	//	cout << "Error reading image " << argv[1] << "." << endl;
	//	exit(EXIT_FAILURE);
	//}

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	//matrix =  new OpenGLMatrix();

	//renderTriangle();
	pipelineProgram = new BasicPipelineProgram();
	pipelineProgram->Init("../openGLHelper-starterCode");
	program = pipelineProgram->GetProgramHandle();
	
	generateSpline();
	generateSkyBox();
	generateGround();
	initVao_Vbo();
	
	std::cout << "GL error: " << glGetError() << std::endl;
}

int main(int argc, char *argv[])
{
	//printf("%d\n", argc);
	//printf("%s\n", argv[0]);
	//printf("%s\n", argv[1]);


	// load the splines from the provided filename
	loadSplines(argv[1]);

	printf("Loaded %d spline(s).\n", numSplines);
	for (int i = 0; i < numSplines; i++)
		//printf("Num control points in spline %d: %d.\n", i, splines[i].numControlPoints);
	//printf("%f\n", splines[0].points[1].x);
	//printf("%f\n", splines[0].points[1].y);
	//printf("%f\n", splines[0].points[1].z);

	cout << "Initializing GLUT..." << endl;
	glutInit(&argc, argv);

	cout << "Initializing OpenGL..." << endl;

#ifdef __APPLE__
	glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
#else
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
#endif

	glutInitWindowSize(windowWidth, windowHeight);
	glutInitWindowPosition(0, 0);
	glutCreateWindow(windowTitle);

	cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
	cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
	cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

#ifdef __APPLE__
	// This is needed on recent Mac OS X versions to correctly display the window.
	glutReshapeWindow(windowWidth - 1, windowHeight - 1);
#endif

	// tells glut to use a particular display function to redraw 
	glutDisplayFunc(displayFunc);
	// perform animation inside idleFunc
	glutIdleFunc(idleFunc);
	// callback for mouse drags
	glutMotionFunc(mouseMotionDragFunc);
	// callback for idle mouse movement
	glutPassiveMotionFunc(mouseMotionFunc);
	// callback for mouse button changes
	glutMouseFunc(mouseButtonFunc);
	// callback for resizing the window
	glutReshapeFunc(reshapeFunc);
	// callback for pressing the keys on the keyboard
	glutKeyboardFunc(keyboardFunc);

	// init glew
#ifdef __APPLE__
  // nothing is needed on Apple
#else
  // Windows, Linux
	GLint result = glewInit();
	if (result != GLEW_OK)
	{
		cout << "test error: " << glewGetErrorString(result) << endl;
		exit(EXIT_FAILURE);
	}
#endif

	// do initialization

	initScene(argc, argv);

	// sink forever into the glut loop
	glutMainLoop();
}


