SOURCE_INC_FILE()

// Symbol definitions for processing draw calls and generally drawing to the screen for the Win32 platform.

#include "SynergyClientAPI.h"
#include "Platform/Win32_Platform.h"

bool Win32DrawCallBuffer::BeginWrite()
{
	/* 
		Reset Cursor position to 0 and zero out buffer memory.Output warning message if buffer is too small.
		Output error if buffer is not valid.
	*/
	
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

DrawCall* Win32DrawCallBuffer::NewDrawCall(DrawCallType Type)
{
	size_t requiredSize = GetDrawCallSize(Type);

	if (requiredSize > BufferSize - CursorPosition)
	{
		std::cerr << "ERROR: Out of memory in draw call buffer. Attempted to create draw call of type " << (uint16_t)(Type)
			<< " with only " << BufferSize - CursorPosition << " bytes available.\n";
		return nullptr;
	}

	// Advance cursor by the required number of bytes and return the address where we can build the draw call.
	DrawCall* address = (DrawCall*)(Buffer + CursorPosition);
	address->type = Type;

	CursorPosition += requiredSize;
	return address;
}

bool Win32DrawCallBuffer::BeginRead()
{
	// Reset Cursor Position to 0. Run safety checks on the buffer and check that the first draw call's type is a valid value.
	if (Buffer == nullptr || BufferSize < sizeof(DrawCall))
	{
		/* 
			Here both the buffer itself being null or it being too small are considered fatal errors,
			as the buffer should have been discarded during the writing stage in either of those cases.
		*/
		std::cerr << "FATAL ERROR: Attempted to start reading a draw call buffer without an actual buffer being provided, or one that is too small !\n"
			<< "Please make sure the buffer is discarded during the writing stage.\n";
		return false;
	}

	// Naive check that should catch most "trash" buffers or wrong start positions in memory.
	if (((DrawCall*)(Buffer))[0].type >= DrawCallType::INVALID)
	{
		std::cerr << "ERROR: Attempted to start reading a draw call buffer from faulty memory.\n";
		return false;
	}

	CursorPosition = 0;
	return true;
}

DrawCall* Win32DrawCallBuffer::GetNext()
{
	/*
		Interpret Cursor Position as current reading position.Read first bytes as DrawCall structure and inspect type.
		Make sure that there is enough room left in the buffer to hold the relevant extended data structure.
		Then, return the DrawCall structure.
	*/

	DrawCall* nextCall = (DrawCall*)(Buffer + CursorPosition);
	if (CursorPosition == BufferSize || nextCall->type == DrawCallType::EMPTY)
	{
		// End of buffer reached.
		return nullptr;
	}

	size_t actualDrawCallSize = GetDrawCallSize(nextCall->type);

	if (actualDrawCallSize == 0)
	{
		// Unrecognized type value which has no defined size, probably due to buffer corruption.
		std::cerr << "ERROR: Unrecognized draw call type value" << (uint8_t)(nextCall->type) << "which has no defined size, probably due to buffer corruption.\n";
		return nullptr;
	}

	if (BufferSize - CursorPosition < actualDrawCallSize)
	{
		// There isn't enough data left to hold the last draw call given the type it's supposed to be, which means the buffer was inconsistently populated.
		std::cerr << "ERROR: Inconsistent draw call buffer size. Check that is was populated correctly. Read draw call of type " << (uint16_t)(nextCall->type)
			<< " with only " << BufferSize - CursorPosition << " bytes available.\n";
		return nullptr;
	}

	/* 
		Is it now guaranteed that the drawcall exists and has enough memory "ahead" of it to initialize its full data structure.
		Advance the cursor and return a pointer to the call we just read.
	*/
	CursorPosition += actualDrawCallSize;
	return nextCall;
}

void Win32_ClearPixelBuffer(Win32PixelRGBA PixelColor, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight)
{
	memset(PixelBuffer, PixelColor.full, BufferWidth * BufferHeight * sizeof(Win32PixelRGBA));
}

void DrawLine(LineDrawCallData& LineDrawCall, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight)
{
	Vector2f lineVec;
	lineVec = (LineDrawCall.destination - LineDrawCall.origin);

	// Track iteration count separately as it needs to stay a positive, relative number in all cases.
	int it = -1;

	if (abs(lineVec.x) > abs(lineVec.y))
	{
		float yIncrement = lineVec.y / abs(lineVec.x);
		for (int x = LineDrawCall.origin.x;; lineVec.x > 0 ? x++ : x--)
		{
			it++;
			if (x < 0)
			{
				if (lineVec.x > 0)
					continue;
				else
					break;
			}
			else if (x >= BufferWidth)
			{
				if (lineVec.x < 0)
					continue;
				else
					break;
			}

			// Compute final coordinates of pixel to be colored.
			uint16_t finalY = (uint16_t)(LineDrawCall.origin.y + yIncrement * it);
			if (finalY < 0 || finalY >= BufferHeight)
			{
				continue;
			}

			PixelBuffer[finalY * BufferWidth + x].full = LineDrawCall.color.full;

			if (x == LineDrawCall.destination.x)
			{
				break;
			}
		}
	}
	else
	{
		float xIncrement = lineVec.x / abs(lineVec.y);
		for (int y = LineDrawCall.origin.y;; lineVec.y > 0 ? y++ : y--)
		{
			it++;
			if (y < 0)
			{
				if (lineVec.y > 0)
					continue;
				else
					break;
			}
			else if (y >= BufferHeight)
			{
				if (lineVec.y < 0)
					continue;
				else
					break;
			}

			// Compute final coordinates of pixel to be colored.
			uint16_t finalX = (uint16_t)(LineDrawCall.origin.x + xIncrement * it);
			if (finalX < 0 || finalX >= BufferHeight)
			{
				continue;
			}
			PixelBuffer[y * BufferWidth + finalX].full = LineDrawCall.color.full;

			if (y == LineDrawCall.destination.y)
			{
				break;
			}
		}
	}
}

void DrawRectangle(RectangleDrawCallData& RectDrawCall, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight)
{
	Vector2s minCoord, maxCoord;

	// Min coordinates, INCLUSIVE
	minCoord.x = max(0, RectDrawCall.origin.x);
	minCoord.y = max(0, RectDrawCall.origin.y);

	if (minCoord.x >= BufferWidth || minCoord.y >= BufferHeight)
	{
		// Rectangle is entirely outside screen. Ignore draw call.
		return;
	}

	// Max coordinates, EXCLUSIVE
	maxCoord.x = min(BufferWidth, RectDrawCall.origin.x + RectDrawCall.dimensions.x);
	maxCoord.y = min(BufferHeight, RectDrawCall.origin.y + RectDrawCall.dimensions.y);

	if (maxCoord.x < 0 || maxCoord.y < 0)
	{
		// Rectangle is entirely outside screen. Ignore draw call.
		return;
	}

	// Pixels are stored line by line in memory. Set the memory line by line accordingly.
	for (uint16_t y = minCoord.y; y < maxCoord.y; y++)
	{
		for (uint16_t x = minCoord.x; x < maxCoord.x; x++)
		{
			PixelBuffer[y * BufferWidth + x].full = RectDrawCall.color.full;
		}
	}
}

void Win32_ProcessDrawCall(DrawCall& Call, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight)
{
	LineDrawCallData& line = (LineDrawCallData&)(Call);
	RectangleDrawCallData& rect = (RectangleDrawCallData&)(Call);
	EllipseDrawCallData& ellipse = (EllipseDrawCallData&)(Call);
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
		std::cerr << "WARNING: Unsupported Client Draw Call type " << (uint16_t)(Call.type) << " ! Ignoring...\n";
		break;
	}
}