# tcp
This demo is a client-server program created based on the article by Kris Kaspersky.
Compiled in *.exe file with "Tiny C Compiler" command: tcc filename.c -lwsock2 -o filename.exe

Server: filename.exe -s/--server -n/--null port
Client: filename.exe -c/--client ip_adress port
