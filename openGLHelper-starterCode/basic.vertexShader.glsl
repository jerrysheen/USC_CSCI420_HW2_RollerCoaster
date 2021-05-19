#version 150

in vec3 position;
//in vec4 color;
in vec3 normal;

//out vec4 col;
in vec2 atexCoord;

out vec2 tc;

out vec3 viewNormal;
out vec3 viewPosition;

uniform int mode;
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

void main()
{
  if(mode == 0){
    vec4 viewPosition4 = modelViewMatrix * vec4(position, 1.0f);
    viewPosition = viewPosition4.xyz;

    gl_Position = projectionMatrix * viewPosition4;
    viewNormal = normalize((modelViewMatrix * vec4(normal, 0.0f)).xyz);
    //col = color;
    //col = vec4(0.5, 0, 0,1);
  }
  else if(mode == 1){
    // compute the transformed and projected vertex position (into gl_Position) 
    // compute the vertex color (into col)
    
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0f);
    
    
    tc = atexCoord;
  }
   
}
