#version 330

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoord;

out vec2 texCoord;
out vec3 vertex_color;
out vec3 vertex_normal;
out vec3 vertex_position; // [增加]

uniform mat4 um4p;	
uniform mat4 um4v;
uniform mat4 um4m;

/*--The followings are same as fragment shader--*/
struct direction_light
{
	vec3 position;
	vec3 direction;
};

struct point_light
{
	vec3 position;
	// attenuation
	float constant;
	float linear;
	float quad;
};

struct spot_light
{
	vec3 position;
	vec3 direction;
	float exponent;
	float cutoff; // degree
	// attenuation
	float constant;
	float linear;
	float quad;
};

struct lightAttribute
{
	vec3 Ia;
	vec3 Ip;
	vec3 Ka;
	vec3 Kd;
	vec3 Ks;
	float shininess; // n
};

uniform direction_light directionL;
uniform point_light pointL;
uniform spot_light spotL;
uniform lightAttribute light;
uniform vec3 viewPos;

vec3 Normalize(vec3 v)
{
	float l;

	l = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;

	return v;
}

float Dot(vec3 v1, vec3 v2)
{
	float dot;

	dot = v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];

	return dot;
}

float max(float f1, float f2)
{
	if( f1 >= f2 )
		return f1;
	else
		return f2;
}

float min(float f1, float f2)
{
	if( f1 <= f2 )
		return f1;
	else
		return f2;
}

vec3 reflect(vec3 lightDir, vec3 norm)
{
	vec3 reflectDir;
	reflectDir = 2 * dot(lightDir, norm) * norm - lightDir;
	return reflectDir;
}

float distance(vec3 v1, vec3 v2)
{
	float depth;
	depth = sqrt( pow(v1[0]-v2[0],2) + pow(v1[1]-v2[1],2) + pow(v1[2]-v2[2],2) );
	return depth;
}
/*-------*/

uniform int per_vertex_or_per_pixel; // left window: per vertex, right window: per pixel
uniform int change_light;

// [TODO] passing uniform variable for texture coordinate offset
uniform bool hasEye;
uniform int isEye;
uniform float xoffset;
uniform float yoffset;
uniform int cur_eye_offset_idx;


void main() 
{
	// [TODO]
	// texCoord = aTexCoord;
	if(isEye == 1){
		if(cur_eye_offset_idx == 0 || cur_eye_offset_idx == 1 || cur_eye_offset_idx == 2 || cur_eye_offset_idx == 3 ){
			texCoord = vec2(aTexCoord.x , aTexCoord.y - 0.25 * (cur_eye_offset_idx % 4));
		}
		else if(cur_eye_offset_idx == 4 || cur_eye_offset_idx == 5 || cur_eye_offset_idx == 6){
			texCoord = vec2(aTexCoord.x + 0.5 , aTexCoord.y - 0.25 * (cur_eye_offset_idx % 4));
		}
	}
	else{
		texCoord = aTexCoord;
	}
	// texCoord = vec2(aTexCoord.x, aTexCoord.y - 0.75);
	
	gl_Position = um4p * um4v * um4m * vec4(aPos, 1.0);

	// Transform the normal's orientation into world space.
	vertex_normal = vec3(um4m * vec4(aNormal.x, aNormal.y, aNormal.z, 1.0));

	// Transform the vertex into world space
	vertex_position = vec3(um4m * vec4(aPos.x, aPos.y, aPos.z, 1.0));

	if(per_vertex_or_per_pixel == 0){
		float f; // attenuation

		// 環境光
		vec3 ambient = light.Ia * light.Ka;

		// 漫反射
		vec3 norm = Normalize(vertex_normal);
		vec3 lightDir;
		if(change_light == 0){
			lightDir = Normalize(directionL.position - directionL.direction);
			f = 1;
		}
		else if(change_light == 1){
			lightDir = Normalize(pointL.position - vertex_position);
			// attenuation
			float depth = distance(pointL.position, vertex_position);
			f = min( 1/(pointL.constant + pointL.linear * depth + pointL.quad * pow(depth,2)), 1 );
		}
		else if(change_light == 2){
			lightDir = Normalize( spotL.position - vertex_position );
			float theta = dot(lightDir, -spotL.direction); // 要加負號[why?]
			if(theta > cos(spotL.cutoff * 3.1415926/180)){ // 換弧度
				// spotlight effect
				float spotlight_effect = pow(max(theta,0), spotL.exponent); // 讓spot light隨距離變暗

				// attenuation
				float depth = distance(spotL.position, vertex_position);
				f = min( 1/(spotL.constant + spotL.linear * depth + spotL.quad * pow(depth,2)), 1 ) * spotlight_effect ;
			}
			else{
				f = 0;
			}
		}
		float diff = max(dot(norm, lightDir), 0.0);
		vec3 diffuse = light.Ip * (light.Kd * diff);

		// 鏡面光
		vec3 viewDir = Normalize(viewPos - vertex_position);
		vec3 reflectDir = reflect(lightDir, norm);
		float spec = pow(max(dot(reflectDir, viewDir), 0.0), light.shininess);
		vec3 specular = light.Ip * (light.Ks * spec);

		vertex_color = (ambient + f * (diffuse + specular)) * aColor;
	}
	else if(per_vertex_or_per_pixel == 1){

		vertex_color = aColor;

	}
}
