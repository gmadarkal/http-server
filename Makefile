default: compile

compile:
	gcc -pthread -o server ./httpechosrv.c -g
