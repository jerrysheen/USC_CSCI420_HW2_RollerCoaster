#version 150

//in vec4 col;
in vec2 tc;

in vec3 viewPosition;
in vec3 viewNormal;

//in vex3 n; //normals

out vec4 c;


uniform int mode;
uniform sampler2D textureImage; 
uniform vec3 viewLightDirection;
void main()
{
  if(mode == 1){
    c = texture(textureImage, tc);
  }
  else if(mode == 0){

    vec3 eyedir = normalize(vec3(0,0,0) - viewPosition);
    //vec3 viewLightDirection = vec3(0, 1, 0);
    
    // reflected light direction
    vec3 reflectDir = -reflect(viewLightDirection, viewNormal);

    // Phong lighting
    float d = max(dot(viewLightDirection, viewNormal), 0.0f);
    float s = max(dot(reflectDir, eyedir), 0.0f);
    
    vec4 ka = vec4(0.5, 0.5, 0.5, 1);
    vec4 kd = vec4(1, 1, 1, 1);
    vec4 ks = vec4(0.9, 0.9, 0.9, 1);

    vec4 La = vec4(0.5, 0.5, 0.5, 1);
    vec4 Ld = vec4(0.5, 0.5, 0.5, 1);
    vec4 Ls = vec4(0.7, 0.7, 0.7, 1);
    float alpha = 32;
    // compute the final color
    c = ka * La + d * kd * Ld + pow(s, alpha) * ks * Ls;
  }
}