// Defines symbols used by the Client to output draw calls to the platform layer which it must support.

#ifndef SYNERGYCLIENT_DRAWBUFFER_INCLUDED
#define SYNERGYCLIENT_DRAWBUFFER_INCLUDED

#include <iostream>

union ColorRGBA
{
	struct
	{
		uint8_t r, g, b, a;
	};

	uint32_t full;
};

/*
	Type of a draw call. Tells the render layer what kind of drawing to do, and what function to call to retrieve the correct
	data structure from the draw call memory.
*/
enum class DrawCallType
{
	EMPTY, // Empty draw call that wasn't initialized from 0ed memory.
	LINE, // Draw pixels in a straight line from origin to specific destination coordinates with a given width.
	RECTANGLE, // Draw pixels with a corner origin and a specific width and height.
	ELLIPSE, // Draw pixels in an ellipse with a specific origin and radius.
	BITMAP, // Same as rectangle, but stretch / shrink a bitmap of pixels to fit inside the rectangle.
	INVALID, // Keep this value at the bottom of the enum. Indicates a draw call that was read from faulty memory.
};

// Base class for all draw call data classes. Contains spatial and visual transform information relevant to all types.
struct DrawCall
{
	DrawCallType type;

	// Origin coordinates of the draw call to be interpreted differently depending on type.
	uint16_t x, y;

	// Rotation in degrees of the drawn shape.
	uint16_t angleDeg;

	ColorRGBA color;
};

/*
	Data for a Line type draw call. Origin coordinates should be interpreted as the start point of the line.
	Angle should be interpreted as Origin-to-Destination axis along cosinus, and left normal of that axis along sinus. 
*/
struct LineDrawCallData : public DrawCall
{
	// Destination point of the line.
	uint16_t destX, destY;

	// Width of the line along its main axis in pixels.
	uint16_t width;
};

/*
	Data for a Rectangle type draw call. Origin coordinates should be interpreted as top left corner position.
	Angle should be interpreted as Rectangle Width along cosinus, Height along sinus at Angle = 0.
*/
struct RectangleDrawCallData : public DrawCall
{
	// Dimensions of rectangle.
	uint16_t width, height;
};

/*
	Data for a Ellipse type draw call. Origin coordinates should be interpreted as the center of the ellipse where the medians intersect.
	Angle should be interpreted as radius X along cosinus, radius Y along sinus at Angle = 0.
*/
struct EllipseDrawCallData : public DrawCall
{
	float radiusX, radiusY;
};

/*
	Data for Bitmap type draw call. Behaves the same as a Rectangle draw call but attempts to stretch / shrink the pixels with the given resolution
	to fit the rectangle.
	TODO Figure out pixel memory management strategy.
*/
struct BitmapDrawCallData : public RectangleDrawCallData
{
	uint16_t resolutionX, resolutionY;
	// TODO Bitmap stuff in here. How do we handle variable sized pixel buffers ? Probably need some guarantee that the pixel data stays in
	// memory somewhere. AllocateBitmap platform call ? Use Client memory ?
};

#endif