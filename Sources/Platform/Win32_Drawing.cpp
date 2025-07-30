// Symbol definitions for processing draw calls and generally drawing to the screen for the Win32 platform.

#include "Win32_Platform.h"
#include "SynergyClientDrawBuffer.h"

void ClearPixelBuffer(PixelRGBA PixelColor, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight)
{
	for (int x = 0; x < BufferWidth; x++)
	{
		for (int y = 0; y < BufferHeight; y++)
		{
			PixelBuffer[y * BufferWidth + x].full = 0xff000000;
		}
	}
}

void DrawLine(LineDrawCallData& DrawCall, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight)
{
	float vecX, vecY;
	vecX = static_cast<float>(DrawCall.destX - DrawCall.x);
	vecY = static_cast<float>(DrawCall.destY - DrawCall.y);

	// Track iteration count separately as it needs to stay a positive, relative number in all cases.
	int it = -1;

	if (abs(vecX) > abs(vecY))
	{
		float yIncrement = vecY / abs(vecX);
		for (int x = DrawCall.x;; vecX > 0 ? x++ : x--)
		{
			it++;
			if (x < 0)
			{
				if (vecX > 0)
					continue;
				else
					break;
			}
			else if (x >= BufferWidth)
			{
				if (vecX < 0)
					continue;
				else
					break;
			}

			// Compute final coordinates of pixel to be colored.
			uint16_t finalY = static_cast<uint16_t>(DrawCall.y + yIncrement * it);
			if (finalY < 0 || finalY >= BufferHeight)
			{
				continue;
			}

			PixelBuffer[finalY * BufferWidth + x].full = DrawCall.color.full;

			if (x == DrawCall.destX)
			{
				break;
			}
		}
	}
	else
	{
		float xIncrement = vecX / abs(vecY);
		for (int y = DrawCall.y;; vecY > 0 ? y++ : y--)
		{
			it++;
			if (y < 0)
			{
				if (vecY > 0)
					continue;
				else
					break;
			}
			else if (y >= BufferHeight)
			{
				if (vecY < 0)
					continue;
				else
					break;
			}

			// Compute final coordinates of pixel to be colored.
			uint16_t finalX = static_cast<uint16_t>(DrawCall.x + xIncrement * it);
			if (finalX < 0 || finalX >= BufferHeight)
			{
				continue;
			}
			PixelBuffer[y * BufferWidth + finalX].full = DrawCall.color.full;

			if (y == DrawCall.destY)
			{
				break;
			}
		}
	}
}

void DrawRectangle(RectangleDrawCallData& DrawCall, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight)
{
	// TODO: Use simd ? 
	for (int y = DrawCall.y; y < DrawCall.height + DrawCall.y; y++)
	{
		if (y < 0)
		{
			continue;
		}
		else if (y >= BufferHeight)
		{
			break;
		}
		for (int x = DrawCall.x; x < DrawCall.width + DrawCall.x; x++)
		{
			if (x < 0)
			{
				continue;
			}
			else if (x >= BufferWidth)
			{
				break;
			}

			PixelBuffer[y * BufferWidth + x].full = DrawCall.color.full;
		}
	}
}

void ProcessDrawCall(DrawCall& Call, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight)
{
	LineDrawCallData& line = static_cast<LineDrawCallData&>(Call);
	RectangleDrawCallData& rect = static_cast<RectangleDrawCallData&>(Call);
	EllipseDrawCallData& ellipse = static_cast<EllipseDrawCallData&>(Call);
	switch (Call.type)
	{
	case(DrawCallType::LINE):
		DrawLine(line, PixelBuffer, BufferWidth, BufferHeight);
		break;
	case(DrawCallType::RECTANGLE):
		DrawRectangle(rect, PixelBuffer, BufferWidth, BufferHeight);
		break;
	case(DrawCallType::ELLIPSE):
		break;
	default:
		std::cerr << "WARNING: Unsupported Client Draw Call type " << static_cast<uint16_t>(Call.type) << " ! Ignoring...\n";
		break;
	}
}