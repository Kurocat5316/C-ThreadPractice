/*  The following code is a VERY heavily modified from code originally sourced from:
	Ray tracing tutorial of http://www.codermind.com/articles/Raytracer-in-C++-Introduction-What-is-ray-tracing.html
	It is free to use for educational purpose and cannot be redistributed outside of the tutorial pages. */

#include "Intersection.h"

// test to see if collision between ray and a plane happens before time t (equivalent to distance)
// updates closest collision time (/distance) if collision occurs
// see: http://en.wikipedia.org/wiki/Line-sphere_intersection
// see: http://www.codermind.com/articles/Raytracer-in-C++-Part-I-First-rays.html
// see: Step 8 of http://meatfighter.com/juggler/ 
// this code make heavy use of constant term removal due to ray always being a unit vector
bool isSphereIntersected(const Sphere* s, const Ray* r, float* t)
{
    // Intersection of a ray and a sphere, check the articles for the rationale
    Vector dist = s->pos - r->start;
    float B = r->dir * dist;
    float D = B * B - dist * dist + s->size * s->size;

	// if D < 0, no intersection, so don't try and calculate the point of intersection
	if (D < 0.0f) return false;

	// calculate both intersection times(/distances)
	float t0 = B - sqrtf(D);
    float t1 = B + sqrtf(D);

	// check to see if either of the two sphere collision points are closer than time parameter
    if ((t0 > EPSILON) && (t0 < *t))
    {
        *t = t0;
		return true;
    } 
	else if ((t1 > EPSILON) && (t1 < *t))
    {
        *t = t1;
		return true;
    }

	return false;
}


// test to see if collision between ray and a triangle happens before time t (equivalent to distance)
// updates closest collision time (/distance) if collision occurs
// based on: https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
// explanation at: https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection
// another: http://hugi.scene.org/online/hugi25/hugi%2025%20-%20coding%20corner%20graphics,%20sound%20&%20synchronization%20ken%20ray-triangle%20intersection%20tests%20for%20dummies.htm
bool isTriangleIntersected(const Triangle* tri, const Ray* r, float* t)
{
	// two edges of the triangle (as world coordinates offsets)
	Vector e1 = tri->p2 - tri->p1;
	Vector e2 = tri->p3 - tri->p1;

	// vector perpendicular to the ray's direction and the second edge of the triangle
	Vector h = cross(r->dir, e2);

	// determinant of above vector and the first edge of the triangle
	float det = e1 * h;

	// no intersection if this value is small (i.e. ray is parallel with triangle surface)
	if (det > -EPSILON && det < EPSILON) return false;
	
	float invDet = 1.0f / det;

	// distance vector between start of ray and first point of triangle
	Vector s = r->start - tri->p1;

	// barycentric coord u (i.e. p2 in coordinate system based around triangle's extents)
	float u = invDet * (s * h);

	// no intersection if value is "before" or "after" the triangle
	if (u < 0.0f || u > 1.0f) return false;

	// barycentric coord v (i.e. p3)
	Vector q = cross(s, e1);
	float v = invDet * (q * r->dir);

	// no intersection if value is "before" or "after" the triangle
	if (v < 0.0f || u + v > 1.0f) return false;

	// find point of intersection
	float t0 = invDet * (e2 * q);

	// check to see if triangle collision point is closer than time parameter
	if (t0 > EPSILON && t0 < *t)
	{
		*t = t0;
		return true;
	}

	return false;
}


// calculate collision normal, viewProjection, object's material, and test to see if inside collision object
void calculateIntersectionResponse(const Scene* scene, const Ray* viewRay, Intersection* intersect)
{
	switch (intersect->objectType)
	{
	case Intersection::SPHERE:
		intersect->normal = normalise(intersect->pos - intersect->sphere->pos);
		intersect->material = &scene->materialContainer[intersect->sphere->materialId];
		break;
	case Intersection::TRIANGLE:
		intersect->normal = intersect->triangle->normal;
		intersect->material = &scene->materialContainer[intersect->triangle->materialId];
	}

	// calculate view projection
	intersect->viewProjection = viewRay->dir * intersect->normal; 

	// detect if we are inside an object (needed for refraction)
	intersect->insideObject = (intersect->normal * viewRay->dir > 0.0f);

	// if inside an object, reverse the normal
    if (intersect->insideObject)
    {
        intersect->normal = intersect->normal * -1.0f;
    }
}


// test to see if collision between ray and any object in the scene
// updates intersection structure if collision occurs
bool objectIntersection(const Scene* scene, const Ray* viewRay, Intersection* intersect)
{
	// set default distance to be a long long way away
    float t = MAX_RAY_DISTANCE;

	// no intersection found by default
	intersect->objectType = Intersection::NONE;

	// search for sphere collisions, storing closest one found
    for (unsigned int i = 0; i < scene->numSpheres; ++i)
    {
        if (isSphereIntersected(&scene->sphereContainer[i], viewRay, &t))
        {
			intersect->objectType = Intersection::SPHERE;
			intersect->sphere = &scene->sphereContainer[i];
        }
    }

	// search for triangle collisions, storing closest one found
	for (unsigned int i = 0; i < scene->numTriangles; ++i)
	{
		if (isTriangleIntersected(&scene->triangleContainer[i], viewRay, &t))
		{
			intersect->objectType = Intersection::TRIANGLE;
			intersect->triangle = &scene->triangleContainer[i];
		}
	}

	// nothing detected, return false
	if (intersect->objectType == Intersection::NONE)
	{
		return false;
	}

	// calculate the point of the intersection
	intersect->pos = viewRay->start + viewRay->dir * t;

	return true;
}
