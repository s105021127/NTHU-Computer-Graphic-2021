#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
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
const int WINDOW_WIDTH = 600;
const int WINDOW_HEIGHT = 600;

bool isDrawWireframe = false;
bool mouse_pressed = false;
int starting_press_x = -1; // 最後
int starting_press_y = -1; // 最後

enum TransMode   // switch mode 最後
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	ViewCenter = 3,
	ViewEye = 4,
	ViewUp = 5,
};

GLint iLocMVP;

vector<string> filenames; // .obj filename list

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form
};
vector<model> models; // 最後

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

enum ProjMode
{
	Orthogonal = 0,
	Perspective = 1,
};
ProjMode cur_proj_mode = Orthogonal;
TransMode cur_trans_mode = GeoTranslation; // 最後

Matrix4 view_matrix;
Matrix4 project_matrix;


typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	int materialId;
	int indexCount;
	GLuint m_texture;
} Shape;
Shape quad;
Shape m_shpae;
vector<Shape> m_shape_list;
int cur_idx = 0; // represent which model should be rendered now


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
// val 代表旋轉sita角嗎?
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

// [TODO] compute viewing matrix according to the setting of main_camera
void setViewingMatrix()
{
	// view_matrix [...] = ...
	// 印出來是-0沒關係嗎?
	// 其實可以用上面的Normalize跟Cross function，之後有空再改

	Vector3 rx, ry, rz;
	Vector3 p1p2, p1p3, cross;
	double dist;

	// Calculate Rz
	dist = sqrt(pow(main_camera.center[0] - main_camera.position[0], 2) + pow(main_camera.center[1] - main_camera.position[1], 2) + pow(main_camera.center[2] - main_camera.position[2], 2) );

	rz = -(main_camera.center - main_camera.position)/dist;


	// Calculate Rx
	p1p2 = main_camera.center - main_camera.position;
	p1p3 = main_camera.up_vector - main_camera.position;

	cross = Vector3(p1p2[1] * p1p3[2] - p1p2[2] * p1p3[1], p1p2[2] * p1p3[0] - p1p2[0] * p1p3[2], p1p2[0] * p1p3[1] - p1p2[1] * p1p3[0]);
	dist = sqrt(pow(cross[0], 2) + pow(cross[1], 2) + pow(cross[2], 2));

	rx = cross/dist;

	// Calculate Rx
	ry = Vector3(rz[1] * rx[2] - rz[2] * rx[1], rz[2] * rx[0] - rz[0] * rx[2], rz[0] * rx[1] - rz[1] * rx[0]);

	// View Mateix
	view_matrix = Matrix4(rx[0], rx[1], rx[2], 0,  ry[0], ry[1], ry[2], 0,  rz[0], rz[1], rz[2], 0,  0, 0, 0, 1) * Matrix4(1, 0, 0, (-1)*main_camera.position[0],  0, 1, 0, (-1)*main_camera.position[1],  0, 0, 1, (-1)*main_camera.position[2],  0, 0, 0, 1);

}

// [TODO] compute orthogonal projection matrix
void setOrthogonal()
{
	cur_proj_mode = Orthogonal;
	GLfloat f, radian;
	radian = (proj.fovy * 3.1415926 / 180.0f) / 2;
	f = 1 / tan(radian);

	project_matrix[0] = 2 / (proj.right - proj.left);
	project_matrix[3] = -(proj.right + proj.left) / (proj.right - proj.left); //
	project_matrix[5] = 2 / (proj.top - proj.bottom);
	project_matrix[7] = -(proj.top + proj.bottom) / (proj.top - proj.bottom); //
	project_matrix[10] = -2 / (proj.farClip - proj.nearClip);
	project_matrix[11] = -(proj.farClip + proj.nearClip) / (proj.farClip - proj.nearClip);
	project_matrix[14] = 0;
	project_matrix[15] = 1;
}

// [TODO] compute persepective projection matrix
// Use CG06-p.131
// 為甚麼用 CG06-p.97 跟 p.129是錯的? 
// [答]: 不會錯，只是要在ChangeSize改proj.right跟left的比例
void setPerspective()
{
	cur_proj_mode = Perspective;
	GLfloat f, radian;
	radian = (proj.fovy * 3.1415926 / 180.0f) / 2;
	f = 1/tan(radian);
	
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
	glViewport(0, 0, width, height);
	// [TODO] change your aspect ratio in both perspective and orthogonal view
	proj.aspect = (float)width / (float)height;
	proj.left = (float)-width / (float)height;
	proj.right = (float)width / (float)height;
	if (cur_proj_mode == Perspective) {
		setPerspective();
	}
	else if (cur_proj_mode == Orthogonal) {
		setOrthogonal();
	}
}

void drawPlane()
{
	GLfloat vertices[18]{ 1.0, -0.9, -1.0,
		1.0, -0.9,  1.0,
		-1.0, -0.9, -1.0,
		1.0, -0.9,  1.0,
		-1.0, -0.9,  1.0,
		-1.0, -0.9, -1.0 };

	GLfloat colors[18]{ 0.0,1.0,0.0,
		0.0,0.5,0.8,
		0.0,1.0,0.0,
		0.0,0.5,0.8,
		0.0,0.5,0.8,
		0.0,1.0,0.0 };


	// [TODO] draw the plane with above vertices and color
	Matrix4 MVP;
	GLfloat mvp[16];
	// [TODO] multiply all the matrix
	// [TODO] row-major ---> column-major
	mvp[0] = 1;  mvp[4] = 0;   mvp[8] = 0;    mvp[12] = 0;
	mvp[1] = 0;  mvp[5] = 1;   mvp[9] = 0;    mvp[13] = 0;
	mvp[2] = 0;  mvp[6] = 0;   mvp[10] = 1;   mvp[14] = 0;
	mvp[3] = 0; mvp[7] = 0;  mvp[11] = 0;   mvp[15] = 1;

	Matrix4 vp;
	vp = Matrix4(project_matrix * view_matrix);
	mvp[0] = vp[0];  mvp[4] = vp[1];   mvp[8] = vp[2];    mvp[12] = vp[3];
	mvp[1] = vp[4];  mvp[5] = vp[5];   mvp[9] = vp[6];    mvp[13] = vp[7];
	mvp[2] = vp[8];  mvp[6] = vp[9];   mvp[10] = vp[10];   mvp[14] = vp[11];
	mvp[3] = vp[12];  mvp[7] = vp[13];  mvp[11] = vp[14];   mvp[15] = vp[15];

	//please refer to LoadModels function
	//glGenVertexArrays..., glBindVertexArray...
	//glGenBuffers..., glBindBuffer..., glBufferData...
	Shape plane_shape;
	glGenVertexArrays(1, &plane_shape.vao);
	glGenBuffers(1, &plane_shape.vbo);

	glBindVertexArray(plane_shape.vao);

	glBindBuffer(GL_ARRAY_BUFFER, plane_shape.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	
	glGenBuffers(1, &plane_shape.p_color);
	glBindBuffer(GL_ARRAY_BUFFER, plane_shape.p_color);
	glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	//glBindBuffer(GL_ARRAY_BUFFER, 0);
	//glBindVertexArray(0);

	// clear canvas
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	// draw our first triangle
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glBindVertexArray(plane_shape.vao);
	glDrawArrays(GL_TRIANGLES, 0, 18);
	
}

// Render function for display rendering
void RenderScene(void) {	
	// clear canvas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Matrix4 T, R, S;
	// [TODO] update translation, rotation and scaling (最後)

	// geometrical transformation
	T = translate(models[cur_idx].position);
	R = rotate(models[cur_idx].rotation);
	S = scaling(models[cur_idx].scale);

	Matrix4 MVP;
	GLfloat mvp[16];

	// [TODO] multiply all the matrix
	// [TODO] row-major ---> column-major

	mvp[0] = 1;  mvp[4] = 0;   mvp[8] = 0;    mvp[12] = 0;
	mvp[1] = 0;  mvp[5] = 1;   mvp[9] = 0;    mvp[13] = 0;
	mvp[2] = 0;  mvp[6] = 0;   mvp[10] = 1;   mvp[14] = 0;
	mvp[3] = 0; mvp[7] = 0;  mvp[11] = 0;   mvp[15] = 1;

	Matrix4 vp;
	vp = Matrix4(project_matrix * view_matrix * R * S * T);
	/*cout << project_matrix * view_matrix << endl;
	cout << vp[11] << endl;*/
	// 為甚麼transpose mvp matrix就會對呢?
	mvp[0] = vp[0];  mvp[4] = vp[1];   mvp[8] = vp[2];    mvp[12] = vp[3];
	mvp[1] = vp[4];  mvp[5] = vp[5];   mvp[9] = vp[6];    mvp[13] = vp[7];
	mvp[2] = vp[8];  mvp[6] = vp[9];   mvp[10] = vp[10];   mvp[14] = vp[11];
	mvp[3] = vp[12];  mvp[7] = vp[13];  mvp[11] = vp[14];   mvp[15] = vp[15];

	// use uniform to send mvp to vertex shader
	// [TODO] draw 3D model in solid or in wireframe mode here, and draw plane
	glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, mvp);
	glBindVertexArray(m_shape_list[cur_idx].vao);
	glDrawArrays(GL_TRIANGLES, 0, m_shape_list[cur_idx].vertex_count);
	glBindVertexArray(0);

	if (isDrawWireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	else
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	
	drawPlane();

}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard
	// close the window
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		cout << "Close Window" << endl;
	}

	// switch 3D model in solid or in wireframe mode
	else if (key == GLFW_KEY_W && action == GLFW_PRESS) {
		if (isDrawWireframe == false) {
			isDrawWireframe = true;
		}
		else if (isDrawWireframe == true) {
			isDrawWireframe = false;
		}
	}

	// switch the model (left)
	else if (key == GLFW_KEY_Z && action == GLFW_PRESS)
	{
		cur_idx = (cur_idx + 1) % 5; // five models
	}

	// switch the model (right)
	else if (key == GLFW_KEY_X && action == GLFW_PRESS)
	{
		cur_idx = (cur_idx + 4) % 5; // five models
	}

	// switch to Orthogonal projection
	else if (key == GLFW_KEY_O && action == GLFW_PRESS)
	{
		setOrthogonal();
	}

	// switch to NDC Perspective projection
	else if (key == GLFW_KEY_P && action == GLFW_PRESS)
	{
		setPerspective();
	}

	// switch to translation mode
	else if (key == GLFW_KEY_T && action == GLFW_PRESS)
	{
		cur_trans_mode = GeoTranslation;
	}

	// switch to scale mode
	else if (key == GLFW_KEY_S && action == GLFW_PRESS)
	{
		cur_trans_mode = GeoScaling;
	}

	// switch to rotation mode
	else if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		cur_trans_mode = GeoRotation;
	}

	// switch to translate eye position mode
	else if (key == GLFW_KEY_E && action == GLFW_PRESS)
	{
		cur_trans_mode = ViewEye;
	}

	// switch to translate viewing center position mode
	else if (key == GLFW_KEY_C && action == GLFW_PRESS)
	{
		cur_trans_mode = ViewCenter;
	}

	// switch to translate camera up vector position mode
	else if (key == GLFW_KEY_U && action == GLFW_PRESS)
	{
		cur_trans_mode = ViewUp;
	}

	// print information
	else if (key == GLFW_KEY_I && action == GLFW_PRESS)
	{
		cout << endl;
		cout << "Matrix Value: " << endl;
		cout << "View Matrix: " << endl;
		cout << view_matrix << endl;
		cout << "Project Matrix: " << endl;
		cout << project_matrix << endl;
		cout << "Tranlation Matrix: " << endl;
		cout << translate(models[cur_idx].position) << endl;
		cout << "Rotation Matrix: " << endl;
		cout << rotate(models[cur_idx].rotation) << endl;
		cout << "Scaling Matrix: " << endl;
		cout << scaling(models[cur_idx].scale) << endl;
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be negtive
	// Apply change on Z axis when scroll the wheel
	// scroll up positive, otherwise it would be negtive
	switch (cur_trans_mode)
	{
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
	//models[cur_idx].position.z = models[cur_idx].position.z + yoffset;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// [TODO] Call back function for mouse
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
	// [TODO] Call back function for cursor position
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

	if (success)
		glUseProgram(p);
    else
    {
        system("pause");
        exit(123);
    }
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, tinyobj::shape_t* shape)
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
		attrib->vertices.at(i) = attrib->vertices.at(i)/ scale;
	}
	size_t index_offset = 0;
	vertices.reserve(shape->mesh.num_face_vertices.size() * 3);
	colors.reserve(shape->mesh.num_face_vertices.size() * 3);
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
		}
		index_offset += fv;
	}
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;

	string err;
	string warn;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Maerial size %d\n", shapes.size(), materials.size());
	
	normalization(&attrib, vertices, colors, &shapes[0]);

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

	m_shape_list.push_back(tmp_shape);
	model tmp_model;
	models.push_back(tmp_model);


	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	shapes.clear();
	materials.clear();
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
	proj.aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	main_camera.position = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

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

	vector<string> model_list{ "../ColorModels/bunny5KC.obj", "../ColorModels/dragon10KC.obj", "../ColorModels/lucy25KC.obj", "../ColorModels/teapot4KC.obj", "../ColorModels/dolphinC.obj"};
	// [TODO] Load five model at here
	LoadModels(model_list[cur_idx]);
	LoadModels(model_list[cur_idx+1]);
	LoadModels(model_list[cur_idx+2]);
	LoadModels(model_list[cur_idx+3]);
	LoadModels(model_list[cur_idx+4]);
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
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "109065504 HW1", NULL, NULL);
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
	// Print context info
	glPrintContextInfo(false);
	// Setup render context
	setupRC();


	// main loop
    while (!glfwWindowShouldClose(window))
    {
        // render
        RenderScene();
        
        // swap buffer from back to front
        glfwSwapBuffers(window);
        
        // Poll input event
        glfwPollEvents();
    }
	
	// just for compatibiliy purposes
	return 0;
}
