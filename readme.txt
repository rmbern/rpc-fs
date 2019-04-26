PA 2 - Filesystem RPC

The following code implements a basic filesystem over rpc implementing
remote analogues of open, read, write, seek and close.

The project, when built, contains a server executable, and a static .a
library for a client to be built against. It also provides a sample
client called "test" that can be ran to demonstrate the how the api
works. The test can be ran in such a way that it demonstrates thread
saftey. See testplan.txt for details.
