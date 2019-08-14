all: ChatClient ChatServer

ChatClient: ChatClient.c Linker.c
	gcc -w -o ChatClient ChatClient.c Linker.c
ChatServer: ChatServer.c Linker.c
	gcc -w -o ChatServer ChatServer.c Linker.c

	
.PHONY: clean
clean:
	rm -f chatclient chatserver
