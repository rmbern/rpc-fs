void handle_getaddr_error(int getaddr_result);

typedef struct FileHandle
{
  int sd;
  FILE * fd; // client does not need
}
FileHandle;

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
	char buffer[0]; // length determined at runtime.
}C2S_Message;

typedef struct S2C_Message
{
	
}S2C_Message;
