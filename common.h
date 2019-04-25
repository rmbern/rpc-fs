void handle_getaddr_error(int getaddr_result);

typedef int FileHandle; // specifically a socket descriptor

// We implicitly define our protocol for message transfer as structs.
// We know ahead of time that the architectures of our server and client
// are going to be identical.
//
// This allows us to greatly simplify our code, as we don't have to
// account for enums mapping to inconsistent integers, endian
// compatibility issues, and other similar problems.

typedef struct C2S_Message // C2S -> Client 2 Server
{
	char operation;
	int flags;
	int mode;
	int length;
  long offset;
  int whence;
	char buffer[0]; // length determined at runtime.
                  // GNU C ONLY
} C2S_Message;

typedef struct S2C_Message // S2C -> Server 2 Client 
{
  int err;
  int byte_count; // for bytes read, written, or seeked depending on operation
} S2C_Message;
