#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdio>  // for perror
#include <cstdlib> // for exit
#include <unistd.h>
#include <sys/wait.h> // For waitpid
#include <signal.h>   // For sigaction and SIGCHILD
#include <map>
#include <sstream>
#include <string>
#include <fstream>

#define MYPORT 8080
#define STORAGE "urls.txt"


void sigchild_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

std::map<std::string, std::string>url_map;
int counter = 1;

std::string make_code() {
    return std::to_string(counter++);
}

std::string get_body(const std::string &req) {
    int pos = req.find("\r\n\r\n"); // find the blank line separating the header from the body 
    if (pos == (int)std::string::npos) return ""; 
    return req.substr(pos + 4); // start slicing after the 4 character long '\r\n\r\n'
}

std::string trim(const std::string &s) {
    int a = s.find_first_not_of(" \t\r\n");
    if (a == (int)std::string::npos) return "";
    int b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
std::string get_method(const std::string &req) {
    return req.substr(0, req.find(' ')); // extracting "GET"; req.find finds the index of the first space it encounters and req.substr creates
    // a substring from index 0 to the index we just found. 
}

std::string get_path(const std::string &req) { 
    int s = req.find(' ') + 1; // in our case, s would be 3 + 1 = 4. Because we want to start extracting after the space
    int e = req.find(' ', s); // starting from index = 4, find the second space
    return req.substr(s, e - s); // starting at index 4, going 'e-s' = 1 length forward 
}

void load_urls() {
    url_map.clear();
    std::ifstream f(STORAGE);
    std::string code, url;
    
    while (f >> code >> url) {
        url_map[code] = url;
    }
}

void save_url(const std::string &code, const std::string &url) {
    std::ofstream f(STORAGE, std::ios::app);
    f << code << " " << url << "\n";
}


void handle(int sock, const std::string &req) {
    load_urls();
    std::string method = get_method(req);
    std::string path = get_path(req);

    if (method == "GET" && path == "/") {
        std::string r = "HTTP/1.1 200 OK\r\n\r\nWelcome to the Shortener!";
        write(sock, r.c_str(), r.size());
        return;
    }

    if (method == "POST" && path == "/shorten") {
        std::string body = trim(get_body(req));
        std::string code = make_code();
        url_map[code] = body;
        save_url(code,body);
    
        std::string r = "HTTP/1.1 201 Created\r\n\r\nShort code created: " + code + "\n";
        write(sock, r.c_str(), r.size());
        return;
    }

    if (method == "GET" && path.size() > 1) {
        std::string code = path.substr(1); // Strip the "/" off the front
        auto it = url_map.find(code);      // Look it up in our dictionary
        
        if (it != url_map.end()) { // If we found a match!
            std::string r = "HTTP/1.1 301 Moved Permanently\r\nLocation: " + it->second + "\r\n\r\n";
            write(sock, r.c_str(), r.size());
            return;
        }
    }
}

int main() {

    int server_fd = socket(AF_INET, SOCK_STREAM, 0); // getting the file descriptor
    if (server_fd == -1) {
        perror("socket creation failed");
        exit(1);
    }

    // fix the ghost problem 
    int yes = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("set socket option failed");
        exit(1);
    }

    // fill out the address struct 
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(MYPORT); //converting little endian to big endian 
    address.sin_addr.s_addr = INADDR_ANY; // so any port 

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("Could not bind");
        exit(1);
    }

    if (listen(server_fd, 5) == -1) {
        perror("could not listen");
        exit(1);
    }

    std::cout << "server connected!" << MYPORT << "..." << std::endl;

    struct sigaction sa;
    sa.sa_handler = sigchild_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    std::cout << "\n[Main server " << getpid() << "] listening on port" << MYPORT << "...\n" << std::endl;

    while (1) {
        struct sockaddr_in client_address; // new struct to hold client data
        socklen_t client_addrln = sizeof(client_address);
        int new_socket = accept(server_fd, (struct sockaddr *)&client_address, &client_addrln);
        if (new_socket == -1) {
            perror("could not connect");
            continue;
        }

        std::cout << "[Main server " << getpid() << "] Connection accepted. Creating worker" << std::endl;

        pid_t pid = fork(); 

        if (pid == 0) { 
            close(server_fd);            
            char buf[4096] = {};
            read(new_socket, buf, 4096);
            
            std::cout << "[worker " << getpid() << "] Processing HTTP request" << std::endl;
            handle(new_socket, std::string(buf));
            std::cout << "[worker " << getpid() << "] shutting down." << std::endl;
    
            close(new_socket);
            exit(0);
        } else { 
            std::cout << "[Main server " << getpid() << "] worker " << pid << " deployed. Back to listening phase.\n" << std::endl;
            close(new_socket);
        }
    }
}