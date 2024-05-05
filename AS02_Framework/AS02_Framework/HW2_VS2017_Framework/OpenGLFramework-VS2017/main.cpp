#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include<math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "textfile.h"

#include "Vectors.h"
#include "Matrices.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

using namespace std;

// Default window size
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;
// current window size
int screenWidth = WINDOW_WIDTH, screenHeight = WINDOW_HEIGHT;

bool mouse_pressed = false;
int starting_press_x = -1;
int starting_press_y = -1;

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	ViewCenter = 3,
	ViewEye = 4,
	ViewUp = 5,
	Light = 6,
	Shininess = 7,
};

GLint iLocMVP;
GLint iLocViewPos;
GLint iLocM;
GLint iLocVerOrPix; // per vertex or per pixel location
GLint iLocChangeLight;

struct lightPointer 
{
	// direct light
	GLint iLocDir_dir;
	GLint iLocDir_pos;

	// point light
	GLint iLocPoi_pos;
	GLint iLocPoi_const;
	GLint iLocPoi_linear;
	GLint iLocPoi_quad;

	//spot light
	GLint iLocSpot_pos;
	GLint iLocSpot_dir;
	GLint iLocSpot_exp;
	GLint iLocSpot_cutoff;
	GLint iLocSpot_const;
	GLint iLocSpot_linear;
	GLint iLocSpot_quad;

	// light attribute
	GLint iLocIa;
	GLint iLocIp;
	GLint iLocKa;
	GLint iLocKd;
	GLint iLocKs;
	GLint iLocShininess;
};
lightPointer lightPointers;

vector<string> filenames; // .obj filename list

struct PhongMaterial // 光
{
	Vector3 Ka;
	Vector3 Kd;
	Vector3 Ks;

};

typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	PhongMaterial material;
	int indexCount;
	GLuint m_texture;
} Shape;

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form

	vector<Shape> shapes;
};
vector<model> models;

struct camera
{
	Vector3 position;
	Vector3 center;
	Vector3 up_vector;
};
camera main_camera;

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};
project_setting proj;

/*----Light Sourse----*/
struct direction_light
{
	GLfloat position[3]; //(1, 1, 1)
	GLfloat direction[3]; //(0, 0, 0)
};
direction_light directionL;

struct point_light
{
	GLfloat position[3]; //(0, 2, 1)
	// attenuation
	GLfloat constant;
	GLfloat linear;
	GLfloat quad;
};
point_light pointL;

struct spot_light
{
	GLfloat position[3]; //(0, 2, 1)
	GLfloat direction[3]; //(0, 0, 0)
	GLfloat exponent; //50
	GLfloat cutoff; //30 degree
	// attenuation
	GLfloat constant;
	GLfloat linear;
	GLfloat quad;
};
spot_light spotL;

/*----Light Arribute----*/
struct lightAttribute
{
	GLfloat Ia[3]; //(0.15, 0.15, 0.15)
	GLfloat Ip[3]; //(1, 1, 1)
	GLfloat Ka[3]; //Base on cur_idx
	GLfloat Kd[3]; //Base on cur_idx
	GLfloat Ks[3]; //Base on cur_idx
	GLfloat shininess; //64  
};
lightAttribute light;

enum ProjMode
{
	Orthogonal = 0,
	Perspective = 1,
};
ProjMode cur_proj_mode = Orthogonal;
TransMode cur_trans_mode = GeoTranslation;

Matrix4 view_matrix;
Matrix4 project_matrix;

Shape quad; // 用在哪?
Shape m_shpae; // 用在哪?
int cur_idx = 0; // represent which model should be rendered now
int change_light = 0; // directional: 0 / point: 1 / spot: 2

static GLvoid Normalize(GLfloat v[3])
{
	GLfloat l;

	l = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

static GLvoid Cross(GLfloat u[3], GLfloat v[3], GLfloat n[3])
{

	n[0] = u[1] * v[2] - u[2] * v[1];
	n[1] = u[2] * v[0] - u[0] * v[2];
	n[2] = u[0] * v[1] - u[1] * v[0];
}


// [TODO] given a translation vector then output a Matrix4 (Translation Matrix)
Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		1, 0, 0, vec[0],
		0, 1, 0, vec[1],
		0, 0, 1, vec[2],
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix)
Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat;

	mat = Matrix4(
		vec[0], 0, 0, 0,
		0, vec[1], 0, 0,
		0, 0, vec[2], 0,
		0, 0, 0, 1
	);

	return mat;
}


// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X)
Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		1, 0, 0, 0,
		0, cos(val), -sin(val), 0,
		0, sin(val), cos(val), 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val), 0, sin(val), 0,
		0, 1, 0, 0,
		-sin(val), 0, cos(val), 0,
		0, 0, 0, 1
	);

	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;

	mat = Matrix4(
		cos(val), -sin(val), 0, 0,
		sin(val), cos(val), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x)*rotateY(vec.y)*rotateZ(vec.z);
}

// [TODO] compute viewing matrix accroding to the setting of main_camera
void setViewingMatrix()
{
	Vector3 rx, ry, rz;
	Vector3 p1p2, p1p3, cross;
	double dist;

	// Calculate Rz
	dist = sqrt(pow(main_camera.center[0] - main_camera.position[0], 2) + pow(main_camera.center[1] - main_camera.position[1], 2) + pow(main_camera.center[2] - main_camera.position[2], 2));

	rz = -(main_camera.center - main_camera.position) / dist;


	// Calculate Rx
	p1p2 = main_camera.center - main_camera.position;
	p1p3 = main_camera.up_vector - main_camera.position;

	cross = Vector3(p1p2[1] * p1p3[2] - p1p2[2] * p1p3[1], p1p2[2] * p1p3[0] - p1p2[0] * p1p3[2], p1p2[0] * p1p3[1] - p1p2[1] * p1p3[0]);
	dist = sqrt(pow(cross[0], 2) + pow(cross[1], 2) + pow(cross[2], 2));

	rx = cross / dist;

	// Calculate Rx
	ry = Vector3(rz[1] * rx[2] - rz[2] * rx[1], rz[2] * rx[0] - rz[0] * rx[2], rz[0] * rx[1] - rz[1] * rx[0]);

	// View Mateix
	view_matrix = Matrix4(rx[0], rx[1], rx[2], 0, ry[0], ry[1], ry[2], 0, rz[0], rz[1], rz[2], 0, 0, 0, 0, 1) * Matrix4(1, 0, 0, (-1)*main_camera.position[0], 0, 1, 0, (-1)*main_camera.position[1], 0, 0, 1, (-1)*main_camera.position[2], 0, 0, 0, 1);

}

// [TODO] compute orthogonal projection matrix
void setOrthogonal()
{
	cur_proj_mode = Orthogonal;
	GLfloat f, radian;
	radian = (proj.fovy * 3.1415926 / 180.0f) / 2;
	f = 1 / tan(radian);

	// handle side by side view
	float right = proj.right / 2;
	float left = proj.left / 2;

	project_matrix[0] = 2 / (right - left);
	project_matrix[3] = -(right + left) / (right - left); //
	project_matrix[5] = 2 / (proj.top - proj.bottom);
	project_matrix[7] = -(proj.top + proj.bottom) / (proj.top - proj.bottom); //
	project_matrix[10] = -2 / (proj.farClip - proj.nearClip);
	project_matrix[11] = -(proj.farClip + proj.nearClip) / (proj.farClip - proj.nearClip);
	project_matrix[14] = 0;
	project_matrix[15] = 1;
}

// [TODO] compute persepective projection matrix
void setPerspective()
{
	cur_proj_mode = Perspective;
	GLfloat f, radian;
	radian = (proj.fovy * 3.1415926 / 180.0f) / 2;
	f = 1 / tan(radian);

	project_matrix[0] = f / proj.aspect;
	project_matrix[5] = f;
	project_matrix[10] = -(proj.farClip + proj.nearClip) / (proj.farClip - proj.nearClip);
	project_matrix[11] = -2 * proj.farClip * proj.nearClip / (proj.farClip - proj.nearClip);
	project_matrix[14] = -1;
	project_matrix[15] = 0;
}


// Vertex buffers
GLuint VAO, VBO;

// Call back function for window reshape
void ChangeSize(GLFWwindow* window, int width, int height)
{
	// glViewport(0, 0, width, height);
	// [TODO] change your aspect ratio
	proj.aspect = (float)(width / 2) / (float)height; // handle side by side view
	proj.left = (float)-width / (float)height; 
	proj.right = (float)width / (float)height; 
	if (cur_proj_mode == Perspective) {
		setPerspective();
	}
	else if (cur_proj_mode == Orthogonal) {
		setOrthogonal();
	}

	screenWidth = width;
	screenHeight = height;
}

// Render function for display rendering
void RenderScene(int per_vertex_or_per_pixel) {
	// Vector3 modelPos = models[cu_idx].position; [HW3]

	Matrix4 T, R, S;

	// geometrical transformation
	T = translate(models[cur_idx].position);
	R = rotate(models[cur_idx].rotation);
	S = scaling(models[cur_idx].scale);

	Matrix4 MVP;
	GLfloat mvp[16];

	MVP = Matrix4(project_matrix * view_matrix * R * S * T);

	mvp[0] = MVP[0];  mvp[4] = MVP[1];   mvp[8] = MVP[2];    mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];   mvp[9] = MVP[6];    mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];   mvp[10] = MVP[10];   mvp[14] = MVP[11];
	mvp[3] = MVP[12];  mvp[7] = MVP[13];  mvp[11] = MVP[14];   mvp[15] = MVP[15];

	//  geometrical transformation
	Matrix4 M;
	GLfloat m[16];

	M = Matrix4(R * S * T);

	m[0] = M[0];  m[4] = M[1];   m[8] = M[2];    m[12] = M[3];
	m[1] = M[4];  m[5] = M[5];   m[9] = M[6];    m[13] = M[7];
	m[2] = M[8];  m[6] = M[9];   m[10] = M[10];   m[14] = M[11];
	m[3] = M[12];  m[7] = M[13];  m[11] = M[14];   m[15] = M[15];


	// set Ka, Kd, Ks (這是這樣傳過去嗎??)
	light.Ka[0] = models[cur_idx].shapes[0].material.Ka[0];
	light.Ka[1] = models[cur_idx].shapes[0].material.Ka[1];
	light.Ka[2] = models[cur_idx].shapes[0].material.Ka[2];
	light.Kd[0] = models[cur_idx].shapes[0].material.Kd[0];
	light.Kd[1] = models[cur_idx].shapes[0].material.Kd[1];
	light.Kd[2] = models[cur_idx].shapes[0].material.Kd[2];
	light.Ks[0] = models[cur_idx].shapes[0].material.Ks[0];
	light.Ks[1] = models[cur_idx].shapes[0].material.Ks[1];
	light.Ks[2] = models[cur_idx].shapes[0].material.Ks[2];

	// change vec3 to GLfloat for main_camera.position
	GLfloat viewPos[3];
	viewPos[0] = main_camera.position[0];
	viewPos[1] = main_camera.position[1];
	viewPos[2] = main_camera.position[2];

	// use uniform to send per vertex or per pixel to vertex & fragment shader
	glUniform1i(iLocVerOrPix, per_vertex_or_per_pixel);

	// use uniform to send mvp to vertex shader
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);

	// use uniform to send m to vertex shader
	glUniformMatrix4fv(iLocM, 1, GL_FALSE, m);

	// use uniform to send view position to fragent shader
	glUniform3fv(iLocViewPos, 1, viewPos);

	// use uniform to send change_light to vertex & fragent shader
	glUniform1i(iLocChangeLight, change_light);

	// use uniform to send light attribute to fragent shader
	glUniform3fv(lightPointers.iLocDir_dir, 1, directionL.direction);
	glUniform3fv(lightPointers.iLocDir_pos, 1, directionL.position);

	glUniform3fv(lightPointers.iLocPoi_pos, 1, pointL.position);
	glUniform1f(lightPointers.iLocPoi_const, pointL.constant);
	glUniform1f(lightPointers.iLocPoi_linear, pointL.linear);
	glUniform1f(lightPointers.iLocPoi_quad, pointL.quad);

	glUniform3fv(lightPointers.iLocSpot_dir, 1, spotL.direction);
	glUniform3fv(lightPointers.iLocSpot_pos, 1, spotL.position);
	glUniform1f(lightPointers.iLocSpot_exp, spotL.exponent);
	glUniform1f(lightPointers.iLocSpot_cutoff, spotL.cutoff);
	glUniform1f(lightPointers.iLocSpot_const, spotL.constant);
	glUniform1f(lightPointers.iLocSpot_linear, spotL.linear);
	glUniform1f(lightPointers.iLocSpot_quad, spotL.quad);

	glUniform3fv(lightPointers.iLocIa, 1, light.Ia);
	glUniform3fv(lightPointers.iLocIp, 1, light.Ip);
	glUniform3fv(lightPointers.iLocKa, 1, light.Ka);
	glUniform3fv(lightPointers.iLocKd, 1, light.Kd);
	glUniform3fv(lightPointers.iLocKs, 1, light.Ks);
	glUniform1f(lightPointers.iLocShininess, light.shininess);


	for (int i = 0; i < models[cur_idx].shapes.size(); i++) 
	{
		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);
	}

}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS) {
		switch (key)
		{
		case GLFW_KEY_ESCAPE:
			exit(0);
			break;
		case GLFW_KEY_Z:
			cur_idx = (cur_idx + 1) % 5; // five models
			break;
		case GLFW_KEY_X:
			cur_idx = (cur_idx + 4) % 5; // five models
			break;
		case GLFW_KEY_O:
			if (cur_proj_mode == Perspective)
			{
				proj.farClip -= 3.0f; // 為甚麼要這個
				setViewingMatrix(); // 為甚麼要這個
				setOrthogonal();
			}
			break;
		case GLFW_KEY_P:
			if (cur_proj_mode == Orthogonal)
			{
				proj.farClip += 3.0f; // 為甚麼要這個
				setViewingMatrix(); // 為甚麼要這個
				setPerspective();
			}
			break;
		case GLFW_KEY_T:
			cur_trans_mode = GeoTranslation;
			break;
		case GLFW_KEY_S:
			cur_trans_mode = GeoScaling;
			break;
		case GLFW_KEY_R:
			cur_trans_mode = GeoRotation;
			break;
		case GLFW_KEY_E:
			cur_trans_mode = ViewEye;
			break;
		case GLFW_KEY_C:
			cur_trans_mode = ViewCenter;
			break;
		case GLFW_KEY_U:
			cur_trans_mode = ViewUp;
			break;
		case GLFW_KEY_L: // switch between directional/point/spotlight [HW2]
			change_light = (change_light + 1) % 3;
			break;
		case GLFW_KEY_K: // switch to light editing mode [HW2]
			cur_trans_mode = Light; ///////////////////////
			break;
		case GLFW_KEY_J: // switch to shininess editing mode [HW2]
			cur_trans_mode = Shininess;
			break;
		case GLFW_KEY_I:
			cout << endl;
			break;
		default:
			break;
		}
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// Apply change on Z axis when scroll the wheel
	// scroll up positive, otherwise it would be negtive
	switch (cur_trans_mode)
	{
	case Shininess:
		light.shininess += 0.5 * (float)yoffset;
		break;
	case Light:
		if (change_light == 0) // 這樣會3種light.Ip連帶影響[改]
		{
			light.Ip[0] += 0.025 * (float)yoffset;
			light.Ip[1] += 0.025 * (float)yoffset;
			light.Ip[2] += 0.025 * (float)yoffset;
		}
		else if (change_light == 1) // 這樣會3種light.Ip連帶影響[改]
		{
			light.Ip[0] += 0.025 * (float)yoffset;
			light.Ip[1] += 0.025 * (float)yoffset;
			light.Ip[2] += 0.025 * (float)yoffset;
		}
		else if (change_light == 2)
		{
			spotL.cutoff += 0.5 * (float)yoffset;
		}
		break;
	case ViewEye:
		main_camera.position.z -= 0.025 * (float)yoffset;
		setViewingMatrix();
		printf("Camera Position = ( %f , %f , %f )\n", main_camera.position.x, main_camera.position.y, main_camera.position.z);
		break;
	case ViewCenter:
		main_camera.center.z += 0.1 * (float)yoffset;
		setViewingMatrix();
		printf("Camera Viewing Direction = ( %f , %f , %f )\n", main_camera.center.x, main_camera.center.y, main_camera.center.z);
		break;
	case ViewUp:
		main_camera.up_vector.z += 0.33 * (float)yoffset;
		setViewingMatrix();
		printf("Camera Up Vector = ( %f , %f , %f )\n", main_camera.up_vector.x, main_camera.up_vector.y, main_camera.up_vector.z);
		break;
	case GeoTranslation:
		models[cur_idx].position.z += 0.1 * (float)yoffset;
		break;
	case GeoScaling:
		models[cur_idx].scale.z += 0.01 * (float)yoffset;
		break;
	case GeoRotation:
		models[cur_idx].rotation.z += (acosf(-1.0f) / 180.0) * 5 * (float)yoffset;
		break;
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// mouse press callback function
	// Apply change on X axis when mouse drag horizontally
	// Apply change on Y axis when mouse drag vertically
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS)
		{
			cout << "Mouse left input working" << endl; //testing
			mouse_pressed = true;

		}
		else if (action == GLFW_RELEASE)
		{
			mouse_pressed = false;
			starting_press_x = -1;
			starting_press_y = -1;
		}
	}
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	// cursor position callback function
	if (mouse_pressed) {
		if (starting_press_x < 0 || starting_press_y < 0) {
			starting_press_x = (int)xpos;
			starting_press_y = (int)ypos;
		}
		else {
			float diff_x = starting_press_x - (int)xpos;
			float diff_y = starting_press_y - (int)ypos;
			starting_press_x = (int)xpos;
			starting_press_y = (int)ypos;
			switch (cur_trans_mode)
			{
			case Light:
				if (change_light == 0)
				{
					directionL.position[0] -= diff_x * (1.0 / 400.0);
					directionL.position[1] += diff_y * (1.0 / 400.0);
				}
				else if (change_light == 1)
				{
					pointL.position[0] -= diff_x * (1.0 / 400.0);
					pointL.position[1] += diff_y * (1.0 / 400.0);
				}
				else if (change_light == 2)
				{
					spotL.position[0] -= diff_x * (1.0 / 400.0);
					spotL.position[1] += diff_y * (1.0 / 400.0);
				}
				break;
			case ViewEye:
				main_camera.position.x += diff_x * (1.0 / 400.0);
				main_camera.position.y += diff_y * (1.0 / 400.0);
				setViewingMatrix();
				printf("Camera Position = ( %f , %f , %f )\n", main_camera.position.x, main_camera.position.y, main_camera.position.z);
				break;
			case ViewCenter:
				main_camera.center.x += diff_x * (1.0 / 400.0);
				main_camera.center.y -= diff_y * (1.0 / 400.0);
				setViewingMatrix();
				printf("Camera Viewing Direction = ( %f , %f , %f )\n", main_camera.center.x, main_camera.center.y, main_camera.center.z);
				break;
			case ViewUp:
				main_camera.up_vector.x += diff_x * 0.1;
				main_camera.up_vector.y += diff_y * 0.1;
				setViewingMatrix();
				printf("Camera Up Vector = ( %f , %f , %f )\n", main_camera.up_vector.x, main_camera.up_vector.y, main_camera.up_vector.z);
				break;
			case GeoTranslation:
				models[cur_idx].position.x += -diff_x * (1.0 / 400.0);
				models[cur_idx].position.y += diff_y * (1.0 / 400.0);
				break;
			case GeoScaling:
				models[cur_idx].scale.x += diff_x * 0.001;
				models[cur_idx].scale.y += diff_y * 0.001;
				break;
			case GeoRotation:
				models[cur_idx].rotation.x += acosf(-1.0f) / 180.0*diff_y*(45.0 / 400.0);
				models[cur_idx].rotation.y += acosf(-1.0f) / 180.0*diff_x*(45.0 / 400.0);
				break;
			}
		}
	}
}

void setShaders()
{
	GLuint v, f, p;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vs");
	fs = textFileRead("shader.fs");

	glShaderSource(v, 1, (const GLchar**)&vs, NULL);
	glShaderSource(f, 1, (const GLchar**)&fs, NULL);

	free(vs);
	free(fs);

	GLint success;
	char infoLog[1000];
	// compile vertex shader
	glCompileShader(v);
	// check for shader compile errors
	glGetShaderiv(v, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(v, 1000, NULL, infoLog);
		std::cout << "ERROR: VERTEX SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// compile fragment shader
	glCompileShader(f);
	// check for shader compile errors
	glGetShaderiv(f, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(f, 1000, NULL, infoLog);
		std::cout << "ERROR: FRAGMENT SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// create program object
	p = glCreateProgram();

	// attach shaders to program object
	glAttachShader(p,f);
	glAttachShader(p,v);

	// link program
	glLinkProgram(p);
	// check for linking errors
	glGetProgramiv(p, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(p, 1000, NULL, infoLog);
		std::cout << "ERROR: SHADER PROGRAM LINKING FAILED\n" << infoLog << std::endl;
	}

	glDeleteShader(v);
	glDeleteShader(f);

	iLocMVP = glGetUniformLocation(p, "mvp");
	iLocViewPos = glGetUniformLocation(p, "viewPos");
	iLocM = glGetUniformLocation(p, "m");
	iLocVerOrPix = glGetUniformLocation(p, "per_vertex_or_per_pixel");
	iLocChangeLight = glGetUniformLocation(p, "change_light");

	// link light attribute
	lightPointers.iLocDir_dir = glGetUniformLocation(p, "directionL.direction");
	lightPointers.iLocDir_pos = glGetUniformLocation(p, "directionL.position");

	lightPointers.iLocPoi_pos = glGetUniformLocation(p, "pointL.position");
	lightPointers.iLocPoi_const = glGetUniformLocation(p, "pointL.constant");
	lightPointers.iLocPoi_linear = glGetUniformLocation(p, "pointL.linear");
	lightPointers.iLocPoi_quad = glGetUniformLocation(p, "pointL.quad");

	lightPointers.iLocSpot_dir = glGetUniformLocation(p, "spotL.direction");
	lightPointers.iLocSpot_pos = glGetUniformLocation(p, "spotL.position");
	lightPointers.iLocSpot_exp = glGetUniformLocation(p, "spotL.exponent");
	lightPointers.iLocSpot_cutoff = glGetUniformLocation(p, "spotL.cutoff");
	lightPointers.iLocSpot_const = glGetUniformLocation(p, "spotL.constant");
	lightPointers.iLocSpot_linear = glGetUniformLocation(p, "spotL.linear");
	lightPointers.iLocSpot_quad = glGetUniformLocation(p, "spotL.quad");

	lightPointers.iLocIa = glGetUniformLocation(p, "light.Ia");
	lightPointers.iLocIp = glGetUniformLocation(p, "light.Ip");
	lightPointers.iLocKa = glGetUniformLocation(p, "light.Ka");
	lightPointers.iLocKd = glGetUniformLocation(p, "light.Kd");
	lightPointers.iLocKs = glGetUniformLocation(p, "light.Ks");
	lightPointers.iLocShininess = glGetUniformLocation(p, "light.shininess");


	if (success)
		glUseProgram(p);
    else
    {
        system("pause");
        exit(123);
    }
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, vector<GLfloat>& normals, tinyobj::shape_t* shape)
{
	vector<float> xVector, yVector, zVector;
	float minX = 10000, maxX = -10000, minY = 10000, maxY = -10000, minZ = 10000, maxZ = -10000;

	// find out min and max value of X, Y and Z axis
	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//maxs = max(maxs, attrib->vertices.at(i));
		if (i % 3 == 0)
		{

			xVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minX)
			{
				minX = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxX)
			{
				maxX = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 1)
		{
			yVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minY)
			{
				minY = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxY)
			{
				maxY = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 2)
		{
			zVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minZ)
			{
				minZ = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxZ)
			{
				maxZ = attrib->vertices.at(i);
			}
		}
	}

	float offsetX = (maxX + minX) / 2;
	float offsetY = (maxY + minY) / 2;
	float offsetZ = (maxZ + minZ) / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		if (offsetX != 0 && i % 3 == 0)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetX;
		}
		else if (offsetY != 0 && i % 3 == 1)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetY;
		}
		else if (offsetZ != 0 && i % 3 == 2)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetZ;
		}
	}

	float greatestAxis = maxX - minX;
	float distanceOfYAxis = maxY - minY;
	float distanceOfZAxis = maxZ - minZ;

	if (distanceOfYAxis > greatestAxis)
	{
		greatestAxis = distanceOfYAxis;
	}

	if (distanceOfZAxis > greatestAxis)
	{
		greatestAxis = distanceOfZAxis;
	}

	float scale = greatestAxis / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//std::cout << i << " = " << (double)(attrib.vertices.at(i) / greatestAxis) << std::endl;
		attrib->vertices.at(i) = attrib->vertices.at(i) / scale;
	}
	size_t index_offset = 0;
	for (size_t f = 0; f < shape->mesh.num_face_vertices.size(); f++) {
		int fv = shape->mesh.num_face_vertices[f];

		// Loop over vertices in the face.
		for (size_t v = 0; v < fv; v++) {
			// access to vertex
			tinyobj::index_t idx = shape->mesh.indices[index_offset + v];
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 0]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 1]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 2]);
			// Optional: vertex colors
			colors.push_back(attrib->colors[3 * idx.vertex_index + 0]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 1]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 2]);
			// Optional: vertex normals
			if (idx.normal_index >= 0) {
				normals.push_back(attrib->normals[3 * idx.normal_index + 0]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 1]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 2]);
			}
		}
		index_offset += fv;
	}
}

string GetBaseDir(const string& filepath) { // 新的
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

void LoadModels(string model_path) // 跟原本的不一樣
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;
	vector<GLfloat> normals; // 多法向量

	string err;
	string warn;

	string base_dir = GetBaseDir(model_path); // handle .mtl with relative path

#ifdef _WIN32
	base_dir += "\\";
#else
	base_dir += "/";
#endif

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str(), base_dir.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Material size %d\n", shapes.size(), materials.size());
	model tmp_model;

	vector<PhongMaterial> allMaterial;
	for (int i = 0; i < materials.size(); i++)
	{
		PhongMaterial material;
		material.Ka = Vector3(materials[i].ambient[0], materials[i].ambient[1], materials[i].ambient[2]);
		material.Kd = Vector3(materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
		material.Ks = Vector3(materials[i].specular[0], materials[i].specular[1], materials[i].specular[2]);
		allMaterial.push_back(material);
		//cout << "allMaterial: " << allMaterial[i].Kd << endl;
	}

	for (int i = 0; i < shapes.size(); i++)
	{

		vertices.clear();
		colors.clear();
		normals.clear();
		normalization(&attrib, vertices, colors, normals, &shapes[i]);
		// printf("Vertices size: %d", vertices.size() / 3);

		Shape tmp_shape;
		glGenVertexArrays(1, &tmp_shape.vao);
		glBindVertexArray(tmp_shape.vao);

		glGenBuffers(1, &tmp_shape.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GL_FLOAT), &vertices.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		tmp_shape.vertex_count = vertices.size() / 3;

		glGenBuffers(1, &tmp_shape.p_color);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_color);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GL_FLOAT), &colors.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glGenBuffers(1, &tmp_shape.p_normal);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_normal);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GL_FLOAT), &normals.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		// not support per face material, use material of first face
		if (allMaterial.size() > 0)
			tmp_shape.material = allMaterial[shapes[i].mesh.material_ids[0]];
		tmp_model.shapes.push_back(tmp_shape);
	}
	shapes.clear();
	materials.clear();
	models.push_back(tmp_model);
}

void initParameter()
{
	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 0.001;
	proj.farClip = 100.0;
	proj.fovy = 80;
	proj.aspect = (float)(WINDOW_WIDTH / 2 ) / (float)WINDOW_HEIGHT; // adjust width for side by side view

	main_camera.position = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	// init light attribute
	// direction light
	directionL.position[0] = 1;
	directionL.position[1] = 1;
	directionL.position[2] = 1;
	directionL.direction[0] = 0;
	directionL.direction[1] = 0;
	directionL.direction[2] = 0;
	// position light
	pointL.position[0] = 0;
	pointL.position[1] = 2;
	pointL.position[2] = 1;
	pointL.constant = 0.01;
	pointL.linear = 0.8;
	pointL.quad = 0.1;
	// spot light
	spotL.position[0] = 0;
	spotL.position[1] = 0;
	spotL.position[2] = 2;
	spotL.direction[0] = 0;
	spotL.direction[1] = 0;
	spotL.direction[2] = -1;
	spotL.exponent = 50;
	spotL.cutoff = 30;
	spotL.constant = 0.05;
	spotL.linear = 0.3;
	spotL.quad = 0.6;
	// Ip (diffuse intensity)
	light.Ip[0] = 1;
	light.Ip[1] = 1;
	light.Ip[2] = 1;
	// Ia (ambient intensity)
	light.Ia[0] = 0.15;
	light.Ia[1] = 0.15;
	light.Ia[2] = 0.15;
	// shininess
	light.shininess = 64;

	setViewingMatrix();
	setPerspective();	//set default projection matrix as perspective matrix
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);

	vector<string> model_list{ "../NormalModels/bunny5KN.obj", "../NormalModels/dragon10KN.obj", "../NormalModels/lucy25KN.obj", "../NormalModels/teapot4KN.obj", "../NormalModels/dolphinN.obj"};
	// [TODO] Load five model at here
	LoadModels(model_list[cur_idx]);
	LoadModels(model_list[cur_idx + 1]);
	LoadModels(model_list[cur_idx + 2]);
	LoadModels(model_list[cur_idx + 3]);
	LoadModels(model_list[cur_idx + 4]);
}

void glPrintContextInfo(bool printExtension)
{
	cout << "GL_VENDOR = " << (const char*)glGetString(GL_VENDOR) << endl;
	cout << "GL_RENDERER = " << (const char*)glGetString(GL_RENDERER) << endl;
	cout << "GL_VERSION = " << (const char*)glGetString(GL_VERSION) << endl;
	cout << "GL_SHADING_LANGUAGE_VERSION = " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (printExtension)
	{
		GLint numExt;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
		cout << "GL_EXTENSIONS =" << endl;
		for (GLint i = 0; i < numExt; i++)
		{
			cout << "\t" << (const char*)glGetStringi(GL_EXTENSIONS, i) << endl;
		}
	}
}


int main(int argc, char **argv)
{
    // initial glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fix compilation on OS X
#endif

    
    // create window
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "109065504 HW2", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    
    // load OpenGL function pointer
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
	// register glfw callback functions
    glfwSetKeyCallback(window, KeyCallback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);

    glfwSetFramebufferSizeCallback(window, ChangeSize);
	glEnable(GL_DEPTH_TEST);
	// Setup render context
	setupRC();

	// main loop
    while (!glfwWindowShouldClose(window))
    {
		// clear canvas
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		// render left view
		glViewport(0, 0, screenWidth / 2, screenHeight);
        RenderScene(0); // per_vertex
		// render right view
		glViewport(screenWidth / 2, 0, screenWidth / 2, screenHeight);
		RenderScene(1); // per_pixel
        
        // swap buffer from back to front
        glfwSwapBuffers(window);
        
        // Poll input event
        glfwPollEvents();
    }
	
	// just for compatibiliy purposes
	return 0;
}
