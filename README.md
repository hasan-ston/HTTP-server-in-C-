## C++ concurrent HTTP server and URL shortener

- A low level http server built in C++ completely from scratch using POSIX sockets
- Multiprocessing -> uses 'fork()' to handle concurrent requests by spawning child processes
- Also implemented zombie cleanup by using signal handling to reap child processses and prevent memory leaks
- Http parser: parses raw http/1.1 requests to extract headers, body, and methods.
- For the url shortener, using file streams to write and read urls to the disk for persistent storage.

## how it works

- When the main server receives a request, it spanws a child worker that processes the HTTP request.
![Server Logs](<assets/Screenshot 2026-05-23 at 1.00.25 PM.png)

- Users can send a `POST` request to `/shorten` with a valid URL. The custom router extracts the body, trims whitespace, generates a unique short code, and saves it to `urls.txt`.
![Logs](<assets/Screenshot 2026-05-23 at 1.01.25 PM.png)
![Logs](<assets/Screenshot 2026-05-23 at 1.06.25 PM.png)

## building

```
bash
   g++ server.cpp -o server
   ./server
```

runs on port 8080

## usage

```
curl -X POST http://localhost:8080/shorten -d "https://github.com"
# returns: short code: 1

curl http://localhost:8080/1
# 301 redirect to https://github.com
```

