// Defines symbols for Viewport management used both by the Platform and the Client.

#ifndef SYNERGY_CLIENT_VIEWPORT_INCLUDED
#define SYNERGY_CLIENT_VIEWPORT_INCLUDED

// Unique identifier for a Viewport allocated by the Platform. Used to annotate relevant input and output.
// Max value is considered an error value.
typedef uint8_t ViewportID;
constexpr ViewportID VIEWPORT_ERROR_ID = ~0;


#endif