#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <thread>
#include <vector>

using namespace std;

void usage() {
    cout << "syntax : echo-server <port> [-e[-b]]\n";
    cout << "  -e : echo\n";
    cout << "  -b : broadcast\n";
    cout << "sample : echo-server 1234 -e -b\n";
}

struct Client {
    int cli_sd;
    vector<int> *clients;
};

struct Param {
    bool echo{false};
    bool broadcast{false};
    uint16_t port{0};

    bool parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                continue;
            }
            if (strcmp(argv[i], "-b") == 0) {
                broadcast = true;
                continue;
            }
            port = stoi(argv[i]);
        }
        return port != 0;
    }
} param;

void recvThread(struct Client cl) {
    cout << "connected\n";
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];
    bool flag = false;
    while (true) {
        ssize_t res = recv(cl.cli_sd, buf, BUFSIZE - 1, 0);
        if (res == 0 || res == -1) {
            cerr << "recv return " << res << endl;
            perror("recv");
            break;
        }
        buf[res] = '\0';

        cout << buf;
        cout.flush();

        if (param.echo) {
            if(param.broadcast) {
                for(int cli_sd : *cl.clients) {
                    res = send(cli_sd, buf, res, 0);
                    if (res == 0 || res == -1) {
                        cerr << "send return " << res << endl;
                        perror("send");
                        flag = true;
                        break;
                    }
                }
                if (flag == true)
                    break;
            } else {
                res = send(cl.cli_sd, buf, res, 0);
                if (res == 0 || res == -1) {
                    cerr << "send return " << res << endl;
                    perror("send");
                    break;
                }
            }
        }
    }
    for (auto it = cl.clients->begin(); it != cl.clients->end(); it++) {
        if (*it == cl.cli_sd) {
            cl.clients->erase(it);
            break;
        }
    }
    cout << "disconnected\n";
    close(cl.cli_sd);
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("socket");
        return -1;
    }

    int optval = 1;
    int res = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (res == -1) {
        perror("setsockopt");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(param.port);

    ssize_t res2 = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
    if (res2 == -1) {
        perror("bind");
        return -1;
    }

    res = listen(sd, 5);
    if (res == -1) {
        perror("listen");
        return -1;
    }

    vector<int> clients;
    while (true) {
        struct sockaddr_in cli_addr;
        socklen_t len = sizeof(cli_addr);
        int cli_sd = accept(sd, (struct sockaddr *)&cli_addr, &len);
        if (cli_sd == -1) {
            perror("accept");
            break;
        }
        clients.push_back(cli_sd);
        thread* t = new thread(recvThread, Client {cli_sd, &clients});
        t->detach();
    }
    close(sd);
}
