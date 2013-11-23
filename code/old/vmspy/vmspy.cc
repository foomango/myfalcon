#include "common/common.h"
#include "common/messages.h"
#include "third_party/libjson/libJSON.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <map>
#include <set>
#include <string>
#include <vector>

// Constants
static const int32_t kJSONBufSize = 4096;
static const int32_t kNumEpollEvents = 10;
static const int kVMSpyWait_ms = 250;
static int kRouterControlPort = 2001;
const char *kVMDirectory = "/usr/local/vms/";
// Globals
static int vm_spy_epoll_;
static int qmp_epoll_;
static int net_epoll_;
static int router_socket_;
static int listen_socket_;
static std::map<std::string,int> vm_socket_map_;
static std::map<std::string,std::vector<int> > vm_pending_map_;

void
DoVMMHandshake(const char *vm_name) {
    int sock = vm_socket_map_[vm_name];
    char *json_buf = (char *) malloc(kJSONBufSize);
    CHECK(json_buf);
    memset(json_buf, '\0', kJSONBufSize);
    // get handshake
    int recv_ret = recv(sock, json_buf, kJSONBufSize -1, 0); 
    if (recv_ret > 0) {
        // handle handshake
        JSONNODE *handshake = json_parse(json_buf);
        if (handshake == NULL) {
            LOG("error in vmm handshake: json_buf: %s", json_buf);
            free(json_buf);
            close(sock);
            vm_socket_map_[vm_name] = -1;
            return;
        }
        // For now just check that we are using QMP
        // TODO(leners) add ntfa_probe to extensions and check for 
        // extention within qemu
        JSONNODE *qmp_node = json_get(handshake, "QMP");
        if (qmp_node == NULL) {
            LOG("error in vmm handshake: json_buf: %s", json_buf);
            free(json_buf);
            close(sock);
            vm_socket_map_[vm_name] = -1;
            return;
        }
        json_delete(handshake);
    } else {
        free(json_buf);
        close(sock);
        vm_socket_map_[vm_name] = -1;
        return;
    }
    JSONNODE *handshake_reply = json_new(JSON_NODE);
    JSONNODE *reply_contents = json_new_a("execute", "qmp_capabilities");
    json_push_back(handshake_reply, reply_contents);
    char *reply_buf = json_write(handshake_reply);
    int send_ret = send(sock, reply_buf, strlen(reply_buf), 0);
    json_delete(handshake_reply);
    if (send_ret != (int)(strlen(reply_buf))) {
        LOG1("error sending vmm handshake");
        free(json_buf);
        free(reply_buf);
        close(sock);
        vm_socket_map_[vm_name] = -1;
        return;
    }
    // This is a null response. 
    // TODO(leners) Add error checking incase this is an asynchronous message.
    recv_ret = recv(sock, json_buf, kJSONBufSize, 0);
    free(json_buf);
    free(reply_buf);
    return;
}

void
VMConnect(const char *vm_name) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, kVMDirectory);
    strcat(addr.sun_path, vm_name);
    if (0 != connect(sock, (struct sockaddr *) &addr, sizeof(addr))) {
        LOG("failed to connect to VM %s", vm_name);
        vm_socket_map_[vm_name] = -1;
    } else {
        LOG("Connected %s", vm_name);
        vm_socket_map_[vm_name] = sock;
        DoVMMHandshake(vm_name);
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.ptr = (void *) malloc(strlen(vm_name) + 1);
        strcpy((char *)ev.data.ptr, vm_name);
        CHECK(vm_socket_map_[(char *)ev.data.ptr] == vm_socket_map_[vm_name]);
        CHECK(0 == epoll_ctl(qmp_epoll_, EPOLL_CTL_ADD, sock, &ev));
    }
    return;
}

void
UpdateVMSocketMap() {
    DIR *dp;
    struct dirent *dirp;
    dp = opendir(kVMDirectory);
    CHECK(dp != NULL);
    while ((dirp = readdir(dp)) != NULL) {
        if (!strcmp("..", dirp->d_name) || !strcmp(".", dirp->d_name)) continue;
        if (vm_socket_map_[dirp->d_name] == 0) VMConnect(dirp->d_name); // Don't allow restarts to re-connect
    }
    closedir(dp);
    return;
}

void
InitEpoll(void) {
    qmp_epoll_ = epoll_create(6);
    net_epoll_ = epoll_create(6);
    vm_spy_epoll_ = epoll_create(6);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = qmp_epoll_;
    CHECK(0 == epoll_ctl(vm_spy_epoll_, EPOLL_CTL_ADD, qmp_epoll_, &ev));
    ev.data.fd = net_epoll_;
    CHECK(0 == epoll_ctl(vm_spy_epoll_, EPOLL_CTL_ADD, net_epoll_, &ev));
}

void
InitNet(const char *router_ip, const char *listen_ip) {
    router_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(listen_socket_ > 0);
    CHECK(router_socket_ > 0);

    // bind + listen
    struct sockaddr_in listen_addr;
    memset(&listen_addr, '\0', sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(kVMSpyPort);
    CHECK(0 != inet_aton(listen_ip, &listen_addr.sin_addr));
    CHECK(0 == bind(listen_socket_, (struct sockaddr *) &listen_addr,
                    sizeof(listen_addr)));
    CHECK(0 == listen(listen_socket_, kNumEpollEvents));
    // connect
    struct sockaddr_in router_addr;
    memset(&router_addr, '\0', sizeof(router_addr));
    router_addr.sin_family = AF_INET;
    router_addr.sin_port = htons(kRouterControlPort);
    CHECK(0 != inet_aton(router_ip, &router_addr.sin_addr));
    CHECK(0 == connect(router_socket_, (struct sockaddr *) &router_addr,
                       sizeof(router_addr)));
    // add to epoll
    struct epoll_event tmp_ev;
    tmp_ev.events = EPOLLIN;
    tmp_ev.data.fd = listen_socket_;
    CHECK(0 == epoll_ctl(net_epoll_, EPOLL_CTL_ADD, listen_socket_, &tmp_ev));
    tmp_ev.data.fd = router_socket_;
    CHECK(0 == epoll_ctl(net_epoll_, EPOLL_CTL_ADD, router_socket_, &tmp_ev));
    return;
}

void
ReplyStatus(const char *vm_name, app_state_t status) {
    struct vmspy_response resp;
    strcpy(resp.vm_name, vm_name);
    resp.status = state2char[status];
    printf("responding %s is %c", vm_name, resp.status);
    for (uint32_t i = 0; i < vm_pending_map_[vm_name].size(); i++) {
        int sock = vm_pending_map_[vm_name][i];
        int send_ret = send(sock, &resp, sizeof(resp), 0);
        if (send_ret != sizeof(resp)) {
            close(sock);
        }
    }
    vm_pending_map_[vm_name].clear();
    return;
}

void
ReplyUp(const char *vm_name) {
    ReplyStatus(vm_name, ALIVE);
    return;
}

void
ReplyDown(const char *vm_name) {
    ReplyStatus(vm_name, DEAD);
    return;
}

void
ReplyNoVM(const char *vm_name) {
    ReplyStatus(vm_name, UNKNOWN);
    return;
}

void
ProbeQEMU(struct vmspy_query &msg) {
    uint32_t delay_ms = ntohl(msg.delay_ms);
    int sock = vm_socket_map_[msg.vm_name];
    JSONNODE *query = json_new(JSON_NODE);
    JSONNODE *execute = json_new_a("execute", "ntfa_probe");
    JSONNODE *arguments = json_new(JSON_NODE);
    JSONNODE *n_delay_ms = json_new_i("delay_ms", delay_ms);
    json_push_back(query, execute);
    json_set_name(arguments, "arguments");
    json_push_back(arguments, n_delay_ms);
    json_push_back(query, arguments);

    char *query_str = json_write(query);
    int query_len = strlen(query_str) + 1;
    int send_ret = send(sock, query_str, query_len, 0);
    json_delete(query);
    if (send_ret != query_len) {
        LOG("error sending vmm query :%s", query_str);
    }
    free(query_str);
    return;
}

void
HandleQuery(int sock) {
    struct vmspy_query msg;
    int recv_ret = recv(sock, &msg, sizeof(msg), 0);
    if (recv_ret != sizeof(msg)) {
        close(sock);
        return;
    }
    vm_pending_map_[msg.vm_name].push_back(sock);
    if (vm_socket_map_[msg.vm_name] == 0) {
        ReplyNoVM(msg.vm_name);
        return;
    } else if (vm_socket_map_[msg.vm_name] < 0) {
        ReplyDown(msg.vm_name);
        return;
    }
    // Didn't need to respond down.
    if (vm_pending_map_[msg.vm_name].size() == 1) {
        ProbeQEMU(msg);
    }
    return;
}

void
HandleQMPMessage(int sock, const char *vm_name) {
    char *json_buf = (char *) malloc(kJSONBufSize);
    CHECK(json_buf);
    memset(json_buf, '\0', kJSONBufSize);
    int recv_ret = recv(sock, json_buf, kJSONBufSize -1, 0);
    if (recv_ret < 0) {
        free(json_buf);
        close(sock);
        vm_socket_map_[vm_name] = -1;
        // Kill vm?
        // TODO(leners) Logic of what happens here.
        return;
    }
    JSONNODE *json_msg = json_parse(json_buf);
    if (json_msg == NULL) {
        free(json_buf);
        close(sock);
        vm_socket_map_[vm_name] = -1;
        // TODO(leners) see above
        return;
    }
    // ASYNC or Repy?
    JSONNODE *test_type = json_get(json_msg, "return");
    if (test_type != NULL) {
        // Handle Reply
        JSONNODE *json_status = json_get(test_type, "ntfa_status");
        if (json_status == NULL) {
            json_delete(json_msg);
            free(json_buf);
            close(sock);
            vm_socket_map_[vm_name] = -1;
            // TODO(leners) see above
            return;
        }
        char *status = json_as_string(json_status);
        if (!strcmp("down",status)) {
            ReplyDown(vm_name);
        } else if (!strcmp("pending",status) ) {
            // pass
        } else {
            // This is an error
            LOG1("error in qemu spy.");
            close(sock);
            vm_socket_map_[vm_name] = -1;
        }
        free(status);
        json_delete(json_msg);
        free(json_buf);
        return;
    } else {
        // Handle ASYNC
        JSONNODE *json_async_status = json_parse(json_buf);
        if (json_async_status == NULL) {
            free(json_buf);
            close(sock);
            vm_socket_map_[vm_name] = -1;
            //TODO(leners) see above
            return;
        }
        JSONNODE *json_async_str = json_get(json_async_status, "event");
        if (json_async_str == NULL) {
            json_delete(json_async_status);
            free(json_buf);
            close(sock);
            vm_socket_map_[vm_name] = -1;
            //TODO(leners) see above
            return;
        }
        char *async_status = json_as_string(json_async_str);
        if (!strcmp("NTFA_ALIVE", async_status)) {
            json_delete(json_async_status);
            free(json_buf);
            free(async_status);
            ReplyUp(vm_name);
            return;
        } else if (!strcmp("SHUTDOWN",async_status) || 
                   !strcmp("STOP", async_status) ||
                   !strcmp("RESET", async_status)) {
            json_delete(json_async_status);
            free(json_buf);
            free(async_status);
            ReplyDown(vm_name);
            return;
        } else {
            // some random message... (e.g. changing the time. See qemu
            // documentation for more detail)
            json_delete(json_async_status);
            free(json_buf);
            free(async_status);
            return;
        }
    }
    return;
}

void
DoKeepAlive(int sock) {
    struct keepalive_msg msg;
    int recv_ret = recv(sock, &msg, sizeof(msg), 0);
    CHECK(recv_ret == sizeof(msg));
    uint32_t tmp_uint = ntohl(msg.counter);
    tmp_uint++;
    msg.counter = htonl(tmp_uint);
    int send_ret = send(sock, &msg, sizeof(msg), 0);
    CHECK(send_ret == sizeof(msg));
    return;
}

void
HandleNetMessage(void) {
    struct epoll_event events[kNumEpollEvents];
    int num_fds = epoll_wait(net_epoll_, events, kNumEpollEvents, 0);
    for (int i = 0; i < num_fds; i++) {
        int curr_fd = events[i].data.fd;
        if (curr_fd == listen_socket_) {
            // Connection
            struct epoll_event tmp_ev;
            struct sockaddr tmp_addr;
            socklen_t len = sizeof(tmp_addr);
            int new_fd = accept(listen_socket_, &tmp_addr, &len);
            CHECK(new_fd > 0);
            tmp_ev.events = EPOLLIN;
            tmp_ev.data.fd = new_fd;
            CHECK(0 == epoll_ctl(net_epoll_, EPOLL_CTL_ADD, new_fd, &tmp_ev));
            continue;
        } else if (curr_fd == router_socket_) {
            // Keep-alive
            DoKeepAlive(curr_fd);
            continue;
        } else {
            if (!(events[i].events & EPOLLIN)) {
                close(curr_fd);
            } else {
                HandleQuery(curr_fd);
            }
            continue;
        }
    }
    return;
}

void
DemuxQMP(void) {
    struct epoll_event events[kNumEpollEvents];
    int num_fds = epoll_wait(qmp_epoll_, events, kNumEpollEvents, 0);
    for (int i = 0; i < num_fds; i++) {
        char *vm_name = (char *) events[i].data.ptr;
        HandleQMPMessage(vm_socket_map_[(char *)vm_name], vm_name);
    }
    return;
}

void
Loop(void) {
    struct epoll_event events[kNumEpollEvents];
    for (;;) {
        UpdateVMSocketMap();
        int num_fds = epoll_wait(vm_spy_epoll_, events, kNumEpollEvents,
                                 kVMSpyWait_ms);
        for (int i = 0; i < num_fds; i++) {
            if (events[i].data.fd == qmp_epoll_) {
                DemuxQMP();
            } else if (events[i].data.fd == net_epoll_) {
                HandleNetMessage();
            } else {
                LOG1("Error in Loop");
            }
        }
    }
    return;
}

void
Daemonize(void) {
    pid_t pid, sid;
    if (getppid() == 1) return;
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    umask(0);
    sid = setsid();
    if (sid < 0) exit(EXIT_FAILURE);
    if (chdir("/") < 0) exit(EXIT_FAILURE);

    freopen ("/dev/null", "r", stdin);
    freopen ("/tmp/vmspy.log", "a", stdout);
    freopen ("/tmp/vmspy.elog", "a", stderr);
    CHECK(0 == fcntl(0, F_SETFL, O_NONBLOCK));
    CHECK(0 == fcntl(1, F_SETFL, O_NONBLOCK));
    CHECK(0 == fcntl(2, F_SETFL, O_NONBLOCK));
}

int
main(int argc, const char **argv) {
    InitEpoll();
    InitNet(argv[1], argv[2]);
    Daemonize();
    Loop();
    return 0;
}
