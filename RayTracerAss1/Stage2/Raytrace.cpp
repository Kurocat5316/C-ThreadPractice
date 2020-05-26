/*  The following code is a VERY heavily modified from code originally sourced from:
Ray tracing tutorial of http://www.codermind.com/articles/Raytracer-in-C++-Introduction-What-is-ray-tracing.html
It is free to use for educational purpose and cannot be redistributed outside of the tutorial pages. */

#define TARGET_WINDOWS

#pragma warning(disable: 4996)
#include "Timer.h"
#include "Primitives.h"
#include "Scene.h"
#include "Lighting.h"
#include "Intersection.h"
#include "ImageIO.h"
#include <iostream> 

unsigned int buffer[MAX_WIDTH * MAX_HEIGHT];

// reflect the ray from an object
Ray calculateReflection(const Ray* viewRay, const Intersection* intersect)
{
	// reflect the viewRay around the object's normal
	Ray newRay = { intersect->pos, viewRay->dir - (intersect->normal * intersect->viewProjection * 2.0f) };

	return newRay;
}


// refract the ray through an object
Ray calculateRefraction(const Ray* viewRay, const Intersection* intersect, float* currentRefractiveIndex)
{
	// change refractive index depending on whether we are in an object or not
	float oldRefractiveIndex = *currentRefractiveIndex;
	*currentRefractiveIndex = intersect->insideObject ? DEFAULT_REFRACTIVE_INDEX : intersect->material->density;

	// calculate refractive ratio from old index and current index
	float refractiveRatio = oldRefractiveIndex / *currentRefractiveIndex;

	// Here we take into account that the light movement is symmetrical from the observer to the source or from the source to the oberver.
	// We then do the computation of the coefficient by taking into account the ray coming from the viewing point.
	float fCosThetaT;
	float fCosThetaI = fabsf(intersect->viewProjection);

	// glass-like material, we're computing the fresnel coefficient.
	if (fCosThetaI >= 1.0f)
	{
		// In this case the ray is coming parallel to the normal to the surface
		fCosThetaT = 1.0f;
	}
	else
	{
		float fSinThetaT = refractiveRatio * sqrtf(1 - fCosThetaI * fCosThetaI);

		// Beyond the angle (1.0f) all surfaces are purely reflective
		fCosThetaT = (fSinThetaT * fSinThetaT >= 1.0f) ? 0.0f : sqrtf(1 - fSinThetaT * fSinThetaT);
	}

	// Here we compute the transmitted ray with the formula of Snell-Descartes
	Ray newRay = { intersect->pos, (viewRay->dir + intersect->normal * fCosThetaI) * refractiveRatio - (intersect->normal * fCosThetaT) };

	return newRay;
}


// follow a single ray until it's final destination (or maximum number of steps reached)
Colour traceRay(const Scene* scene, Ray viewRay)
{
	Colour output(0.0f, 0.0f, 0.0f); 								// colour value to be output
	float currentRefractiveIndex = DEFAULT_REFRACTIVE_INDEX;		// current refractive index
	float coef = 1.0f;												// amount of ray left to transmit
	Intersection intersect;											// properties of current intersection

																	// loop until reached maximum ray cast limit (unless loop is broken out of)
	for (int level = 0; level < MAX_RAYS_CAST; ++level)
	{
		// check for intersections between the view ray and any of the objects in the scene
		// exit the loop if no intersection found
		if (!objectIntersection(scene, &viewRay, &intersect)) break;

		// calculate response to collision: ie. get normal at point of collision and material of object
		calculateIntersectionResponse(scene, &viewRay, &intersect);

		// apply the diffuse and specular lighting 
		if (!intersect.insideObject) output += coef * applyLighting(scene, &viewRay, &intersect);

		// if object has reflection or refraction component, adjust the view ray and coefficent of calculation and continue looping
		if (intersect.material->reflection)
		{
			viewRay = calculateReflection(&viewRay, &intersect);
			coef *= intersect.material->reflection;
		}
		else if (intersect.material->refraction)
		{
			viewRay = calculateRefraction(&viewRay, &intersect, &currentRefractiveIndex);
			coef *= intersect.material->refraction;
		}
		else
		{
			// if no reflection or refraction, then finish looping (cast no more rays)
			return output;
		}
	}

	// if the calculation coefficient is non-zero, read from the environment map
	if (coef > 0.0f)
	{
		Material& currentMaterial = scene->materialContainer[scene->skyboxMaterialId];

		output += coef * currentMaterial.diffuse;
	}

	return output;
}


// render scene at given width and height and anti-aliasing level
void render(Scene* scene, const int width, const int height, const int aaLevel, int threadsId, int threads, bool colourRise, unsigned int* lineCount)
{
	// angle between each successive ray cast (per pixel, anti-aliasing uses a fraction of this)
	const float dirStepSize = 1.0f / (0.5f * width / tanf(PIOVER180 * 0.5f * scene->cameraFieldOfView));


	// pointer to output buffer
	unsigned int* out = buffer;


	unsigned int iy;

	// potentially loop through entire image size (ie. (0,0) to (width - 1, height - 1)), doing a single row at a time
	while ((iy = InterlockedIncrement(lineCount)) < height) {
		int y = iy - height / 2;
		if (threadsId == 1)
			int i = 0;

		out = buffer + width * (iy);

		for (int x = -width / 2; x < width / 2; x += 1)
		{
			Colour output(0.0f, 0.0f, 0.0f);

			// calculate multiple samples for each pixel
			const float sampleStep = 1.0f / aaLevel, sampleRatio = 1.0f / (aaLevel * aaLevel);

			// loop through all sub-locations within the pixel
			for (float fragmentx = float(x); fragmentx < x + 1.0f; fragmentx += sampleStep)
			{
				for (float fragmenty = float(y); fragmenty < y + 1.0f; fragmenty += sampleStep)
				{
					// direction of default forward facing ray
					Vector dir = { fragmentx * dirStepSize, fragmenty * dirStepSize, 1.0f };

					// rotated direction of ray
					Vector rotatedDir = {
						dir.x * cosf(scene->cameraRotation) - dir.z * sinf(scene->cameraRotation),
						dir.y,
						dir.x * sinf(scene->cameraRotation) + dir.z * cosf(scene->cameraRotation) };

					// view ray starting from camera position and heading in rotated (normalised) direction
					Ray viewRay = { scene->cameraPosition, normalise(rotatedDir) };

					// follow ray and add proportional of the result to the final pixel colour
					output += sampleRatio * traceRay(scene, viewRay);
				}
			}

			
			//color rise processing
			if (colourRise) {
				output.colourise(threadsId % 7);
			}

			*out++ = output.convertToPixel(scene->exposure);

		}

	}

	

}

//set up thread struct
struct ThreadData
{
	unsigned int id;
	Scene scene;
	int width;
	int height;
	int sample;
	int threads;
	bool colorRise;
	unsigned int* lineCount;
};

//initial process with current thread value
DWORD __stdcall ThreadStart(LPVOID threadData)
{
	// cast the pointer to void (i.e. an untyped pointer) into something we can use
	ThreadData* data = (ThreadData*)threadData;

	render(&data->scene, data->width, data->height, data->sample, data->id, data->threads, data->colorRise, data->lineCount);

	ExitThread(NULL);
}


// read command line arguments, render, and write out BMP file
int main(int argc, char* argv[])
{
	int width = 1024;
	int height = 1024;
	int samples = 1;

	// rendering options
	int times = 1;
	unsigned int threads = 3;
	bool colourise = false;				// currently unused
	unsigned int blockSize = -1;		// currently unused

										// default input / output filenames
	const char* inputFilename = "../Scenes/cornell.txt";
	char outputFilenameBuffer[1000];
	char* outputFilename = outputFilenameBuffer;

	// do stuff with command line args
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-size") == 0)
		{
			width = atoi(argv[++i]);
			height = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-samples") == 0)
		{
			samples = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-input") == 0)
		{
			inputFilename = argv[++i];
		}
		else if (strcmp(argv[i], "-output") == 0)
		{
			outputFilename = argv[++i];
		}
		else if (strcmp(argv[i], "-runs") == 0)
		{
			times = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-threads") == 0)
		{
			threads = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-colourise") == 0)
		{
			colourise = true;
		}
		else if (strcmp(argv[i], "-blockSize") == 0)
		{
			blockSize = atoi(argv[++i]);
		}
		else
		{
			std::string tmp = argv[i];
			if (tmp.find("Sences") || tmp.find("sences")) {
				inputFilename = strdup(tmp.c_str());
				printf("%s\n", inputFilename);
			}
			else
				fprintf(stderr, "unknown argument: %s\n", argv[i]);
		}
	}

	sprintf(outputFilenameBuffer, "../Outputs/Thread_%d_%s_%dx%dx%d_%s.bmp", threads, (strrchr(inputFilename, '/') + 1), width, height, samples, (strrchr(argv[0], '\\') + 1));

	// read scene file
	Scene scene;
	if (!init(inputFilename, scene))
	{
		fprintf(stderr, "Failure when reading the Scene file.\n");
		return -1;
	}


	// total time taken to render all runs (used to calculate average)



		

		

		HANDLE* threadHandles = new HANDLE[threads];
		ThreadData* threadData = new ThreadData[threads];

		// shared count of last line allocated to a thread (starts at -1 because we are using InterlockedIncrement)
		unsigned int lineCount = -1;
		//initial value in each threads
		for (int i = 0; i < threads; i++) {
			threadData[i].id = i;					//thread Id
			threadData[i].width = width;			//img width
			threadData[i].height = height;			//img height
			threadData[i].scene = scene;			//img scene
			threadData[i].sample = samples;			//img samples
			threadData[i].threads = threads;		//total thread number
			threadData[i].colorRise = colourise;	//Colour rise
			threadData[i].lineCount = &lineCount;	//line count

			threadHandles[i] = CreateThread(NULL, 0, ThreadStart, (void*)&threadData[i], 0, NULL);
		}


		int totalTime = 0;
		for (int i = 0; i < times; i++)
		{

			Timer timer;									// create timer

			for (unsigned int i = 0; i < threads; i++)
				WaitForSingleObject(threadHandles[i], INFINITE);

			timer.end();									// record end time
			totalTime += timer.getMilliseconds();			// record total time taken
		}


		// output timing information (times run and average)
		printf("Thread: %d_average time taken (%d run(s)): %ums\n", threads, times, totalTime / times);


		delete[] threadHandles;
		delete[] threadData;

		// output BMP file
		write_bmp(outputFilename, buffer, width, height, width);

}
