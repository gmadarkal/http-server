# HTTP-SERVER
This is a mock of how a HTTP server works. To start this server just compile it by running "make", once compiled, start the server by using the object file ./server <portno>.
Once the server is up and running, go to the browser and make get requests to get the html and resource files.
This server serves all kinds of file extensions that are ideally found in a server.
It handles multiple requests by spawning threads for each request.
If a request sends a connection keep alive header then the server socket is kept open and wait for the next request.
If a request arrives then the socket is reused otherwise it is closed. 
