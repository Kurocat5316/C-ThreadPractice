/*  The following code is a VERY heavily modified from code originally sourced from:
	Ray tracing tutorial of http://www.codermind.com/articles/Raytracer-in-C++-Introduction-What-is-ray-tracing.html
	It is free to use for educational purpose and cannot be redistributed outside of the tutorial pages. */

// YOU SHOULD _NOT_ NEED TO MODIFY THIS FILE

#ifndef __SCENE_H
#define __SCENE_H

#include "SceneObjects.h"

// description of a single static scene
typedef struct Scene 
{
	Point cameraPosition;					// camera location
	float cameraRotation;					// direction camera points
    float cameraFieldOfView;				// field of view for the camera

	float exposure;							// image exposure

	unsigned int skyboxMaterialId;

	// scene object counts
	unsigned int numMaterials;
	unsigned int numSpheres;
	unsigned int numTriangles;
	unsigned int numLights;

	// scene objects
	Material* materialContainer;	
	Sphere* sphereContainer;
	Triangle* triangleContainer;
	Light* lightContainer;
} Scene;

bool init(const char* inputName, Scene& scene);

#endif // __SCENE_H
