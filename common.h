void handle_getaddr_error(int getaddr_result);

typedef struct FileHandle
{
  int sd;
  FILE * fd; // client does not need
}
FileHandle;
