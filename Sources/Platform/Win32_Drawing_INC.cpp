// Symbol definitions for processing draw calls and generally drawing to the screen for the Win32 platform.

#include "Platform/Win32_Platform.h"
#include "SynergyClientDrawing.h"

#ifndef TRANSLATION_UNIT
static_assert(0, "INC File " __FILE__ " must be included within a translation unit and NOT compiled on its own !");
#endif

#ifndef WIN32_DRAWING_INCLUDED
#define WIN32_DRAWING_INCLUDED
#else
static_assert(0, "INC File " __FILE__ " has been included twice !");
#endif

/*
	Returns the expected actual size of a draw call data structure.
	Returns 0 if the call is invalid for any reason.
*/
inline size_t GetDrawCallSize(DrawCallType CallType)
{
	switch (CallType)
	{
	case(DrawCallType::LINE):
		return sizeof(LineDrawCallData);
	case(DrawCallType::RECTANGLE):
		return sizeof(RectangleDrawCallData);
	case(DrawCallType::ELLIPSE):
		return sizeof(EllipseDrawCallData);
	case(DrawCallType::BITMAP):
		return sizeof(BitmapDrawCallData);
	default:
		return 0;
	}
}

bool ClientFrameDrawCallBuffer::BeginWrite()
{
	// Reset Cursor position to 0 and zero out buffer memory. Output warning message if buffer is too small.
	// Output error if buffer is not valid.

	if (Buffer == nullptr)
	{
		std::cerr << "ERROR: Attempted to make draw call buffer writeable without an actual buffer being provided !\n";
		return false;
	}

	if (BufferSize < sizeof(DrawCall))
	{
		std::cerr << "WARNING: Made a draw call buffer of size " << BufferSize << " writeable, which is too small for any kind of draw call.\n";
	}

	CursorPosition = 0;
	memset(Buffer, 0, BufferSize);
	return true;
}

DrawCall* ClientFrameDrawCallBuffer::NewDrawCall(DrawCallType Type)
{
	size_t requiredSize = GetDrawCallSize(Type);

	if (requiredSize > BufferSize - CursorPosition)
	{
		std::cerr << "ERROR: Out of memory in draw call buffer. Attempted to create draw call of type " << static_cast<uint16_t>(Type)
			<< " with only " << BufferSize - CursorPosition << " bytes available.\n";
		return nullptr;
	}

	// Advance cursor by the required number of bytes and return the address where we can build the draw call.
	DrawCall* address = reinterpret_cast<DrawCall*>(Buffer + CursorPosition);
	address->type = Type;

	CursorPosition += requiredSize;
	return address;
}

bool ClientFrameDrawCallBuffer::BeginRead()
{
	// Reset Cursor Position to 0. Run safety checks on the buffer and check that the first draw call's type is a valid value.
	if (Buffer == nullptr || BufferSize < sizeof(DrawCall))
	{
		// Here both the buffer itself being null or it being too small are considered fatal errors, 
		// as the buffer should have been discarded during the writing stage in either of those cases.
		std::cerr << "FATAL ERROR: Attempted to start reading a draw call buffer without an actual buffer being provided, or one that is too small !\n"
			<< "Please make sure the buffer is discarded during the writing stage.\n";
		return false;
	}

	if (reinterpret_cast<DrawCall*>(Buffer)[0].type >= DrawCallType::INVALID)
	{
		std::cerr << "ERROR: Attempted to start reading a draw call buffer from faulty memory.\n";
		return false;
	}

	CursorPosition = 0;
	return true;
}

DrawCall* ClientFrameDrawCallBuffer::GetNext()
{
	// Interpret Cursor Position as current reading position. Read first bytes as DrawCall structure and inspect type.
	// Make sure that there is enough room left in the buffer to hold the relevant extended data structure.
	// Then, return the DrawCall structure.

	DrawCall* nextCall = reinterpret_cast<DrawCall*>(Buffer + CursorPosition);
	if (CursorPosition == BufferSize || nextCall->type == DrawCallType::EMPTY)
	{
		// End of buffer reached.
		return nullptr;
	}

	size_t actualDrawCallSize = GetDrawCallSize(nextCall->type);

	if (actualDrawCallSize == 0)
	{
		// Unrecognized type value which has no defined size, probably due to buffer corruption.
		std::cerr << "ERROR: Unrecognized draw call type value" << static_cast<uint8_t>(nextCall->type) << "which has no defined size, probably due to buffer corruption.\n";
		return nullptr;
	}

	if (BufferSize - CursorPosition < actualDrawCallSize)
	{
		std::cerr << "ERROR: Inconsistent draw call buffer size. Check that is was populated correctly. Read draw call of type " << static_cast<uint16_t>(nextCall->type)
			<< " with only " << BufferSize - CursorPosition << " bytes available.\n";
		return nullptr;
	}

	// Is it now guaranteed that the drawcall exists and has enough memory "ahead" of it to initialize its full data structure.
	// Advance the cursor and return a pointer to the call we just read.
	CursorPosition += actualDrawCallSize;
	return nextCall;
}

void ClearPixelBuffer(PixelRGBA PixelColor, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight)
{
	memset(PixelBuffer, PixelColor.full, BufferWidth * BufferHeight * sizeof(PixelRGBA));
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