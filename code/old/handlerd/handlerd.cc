#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <map>
#include <vector>
#include <queue>
#include <functional>
#include <string>

#include "common/common.h"
#include "common/messages.h"
#include "handlerd/process.h"


namespace {

enum event_type {
    CONFIRM,
    KILL,
    PROBE
};

#define F_POLL_MS 100

class HandlerEvent {
    protected:
        Process::Process*    process_;
        double      time_;
        uint32_t    queryno_;
        event_type  type_;
    public:
        HandlerEvent(Process::Process* process, enum event_type e, double offset) {
            process_ = process;
            time_ = offset + GetRealTime();
            queryno_ = process->GetQueryNo();
            type_ = e;
        }
        double GetTime() const { return time_; }
        HandlerEvent* DoEvent() {
            if (type_ == PROBE) {
                if(process_->Probe()) {
                    return new HandlerEvent(process_, KILL,
                                            kMillisecondsToSeconds * 
                                            kDefaultHandlerdSpyTimeout);
                }
            } else if (type_ == CONFIRM) {
                if (!process_->Confirm()) {
			return new HandlerEvent(process_, CONFIRM,
						kMillisecondsToSeconds *
                                                kDefaultHandlerdConfirmTimeout);
		}
            } else if (type_ == KILL) {
                if (queryno_ < process_->GetQueryNo()) {
                    // pass
                } else if (process_->Assassinate()) {
                    process_->ReplyDown();
                } else {
                    return new HandlerEvent(process_, CONFIRM,
                                            kMillisecondsToSeconds * 
                                            kDefaultHandlerdConfirmTimeout);
                }
            } else {
                CHECK(0);
            }
            return NULL;
        }
};

struct _Operator {
    bool operator() (HandlerEvent &e1, HandlerEvent &e2) {
            return e1.GetTime() > e2.GetTime();
    }
};

std::priority_queue<HandlerEvent,std::vector<HandlerEvent>,_Operator> handlerPQ_;
// Default set in Initialize
struct timespec max_timeout_;
// Two listen sockets and epolls
int unix_socket_;
int net_socket_; 
int main_epoll_fd_;
int net_epoll_fd_; 
int unix_epoll_fd_;
int unix_new_fd_;
// Needed for threads to signal main over the usocket
struct sockaddr_un handler_addr_;
// NTFA counter file descriptor
int proc_fd_;
// Number of events to hand out on an epoll at a time
const uint32_t kNumEpollEvents = 10;
// Function prototypes
void TickleProcfile();
// Main Methods
// Initialize global variables
void Initialize(void);
// Daemonizes the process
void Daemonize(void);

void
Initialize() {
    max_timeout_.tv_sec = 0;
    max_timeout_.tv_nsec = 150 * kMillisecondsToNanoseconds; // 150 msec

    // Set up the UNIX socket
    unlink(ntfa_socket_path);
    errno = 0; // Will succeed if the program needs it to.
    unix_socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(unix_socket_ > 0);
    handler_addr_.sun_family = AF_UNIX;
#define UNIX_MAX_PATH 108 // from the man pages
    strncpy(handler_addr_.sun_path, ntfa_socket_path, UNIX_MAX_PATH);
    CHECK(0 == bind(unix_socket_, (struct sockaddr *) &handler_addr_, sizeof(handler_addr_)));
    chmod(ntfa_socket_path, 722);
    CHECK(0 == listen(unix_socket_, 10));

    // Set up the TCP Socket
    struct sockaddr_in net_addr;
    memset(&net_addr, '\0', sizeof(net_addr));
    net_addr.sin_family = AF_INET;
    net_addr.sin_port = htons(kHandlerdPortNum);
    inet_pton(AF_INET, "0.0.0.0", &(net_addr.sin_addr));
    net_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(net_socket_ >= 0);
    CHECK(0 == bind(net_socket_, (struct sockaddr *) &net_addr, sizeof(net_addr)));
    CHECK(0 == listen(net_socket_, 10));

    // Set up the epolls
    main_epoll_fd_ = epoll_create(6); // arg is ignored, but I like 6
    net_epoll_fd_ = epoll_create(6); 
    unix_epoll_fd_ = epoll_create(6); 
    unix_new_fd_ = epoll_create(6);
    struct epoll_event tmp_ev;
    memset(&tmp_ev, '\0', sizeof(tmp_ev));
    tmp_ev.data.fd = unix_socket_;
    tmp_ev.events = EPOLLIN;
    CHECK(0 == epoll_ctl(main_epoll_fd_, EPOLL_CTL_ADD, unix_socket_, &tmp_ev));
    tmp_ev.data.fd = net_socket_;
    tmp_ev.events = EPOLLIN;
    CHECK(0 == epoll_ctl(main_epoll_fd_, EPOLL_CTL_ADD, net_socket_, &tmp_ev));
    tmp_ev.data.fd = net_epoll_fd_;
    tmp_ev.events = EPOLLIN;
    CHECK(0 == epoll_ctl(main_epoll_fd_, EPOLL_CTL_ADD, net_epoll_fd_, &tmp_ev));
    tmp_ev.data.fd = unix_epoll_fd_;
    tmp_ev.events = EPOLLIN;
    CHECK(0 == epoll_ctl(main_epoll_fd_, EPOLL_CTL_ADD, unix_epoll_fd_, &tmp_ev));
    tmp_ev.data.fd = unix_new_fd_;
    tmp_ev.events = EPOLLIN;
    CHECK(0 == epoll_ctl(main_epoll_fd_, EPOLL_CTL_ADD, unix_new_fd_, &tmp_ev));

    Process::InitGenerations();
    
    proc_fd_ = open("/proc/ntfa", O_WRONLY);
    CHECK(proc_fd_ >= 0);
    CHECK(0 == fcntl(proc_fd_, F_SETFL, O_NONBLOCK));
    CHECK(0 == fcntl(net_socket_, F_SETFL, O_NONBLOCK));
    CHECK(0 == fcntl(unix_socket_, F_SETFL, O_NONBLOCK));

    return;
}

// Taken from www-theorie.physik.unizh.ch/~dpotter/howto/daemonize
void
Daemonize() {
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
    freopen ("/var/log/ntfa.log", "a", stdout);
    freopen ("/var/log/ntfa.log", "a", stderr);
    CHECK(0 == fcntl(0, F_SETFL, O_NONBLOCK));
    CHECK(0 == fcntl(1, F_SETFL, O_NONBLOCK));
    CHECK(0 == fcntl(2, F_SETFL, O_NONBLOCK));

    // Makes the process badass
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_RR);
    CHECK(0 == sched_setscheduler(0, SCHED_RR, &param));
    CHECK(0 == nice(-20) || -20 == nice(-20));
    
    struct rlimit rlim;
    rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
    CHECK(0 == setrlimit(RLIMIT_MEMLOCK, &rlim));
    CHECK(0 == mlockall(MCL_CURRENT | MCL_FUTURE));
    return;
}

inline void
TickleProcfile() {
    char crap;
    CHECK(1 == write(proc_fd_, &crap, 1));
    return;
}

// Manages the even queue
void
DoTimecheck(void) {
    TickleProcfile();
    double now = GetRealTime();
    while (!handlerPQ_.empty() && handlerPQ_.top().GetTime() < now) {
        HandlerEvent e = handlerPQ_.top();
        HandlerEvent *new_e = e.DoEvent();
        handlerPQ_.pop();
        if (new_e != NULL) {
            handlerPQ_.push(*new_e);
            free(new_e);
        }
        now = GetRealTime();
        TickleProcfile();
    }
    return;
}

void
HandleFailure(std::string failure) {
    FILE* crash_file = fopen("/sys/kernel/debug/provoke-crash/DIRECT", "w");
    CHECK(crash_file);
    LOG("inducing failure %s after fflush/fsync", failure.c_str());
    fflush(stderr);
    fsync(fileno(stderr));
    if (0 == failure.compare("fail_handlerd")) {
        // Could send ourselves a SIGSEGV instead?
        LOG1("failing handlerd!");
        int *x = NULL;
        *x = 6; 
    } else if (0 == failure.compare("fail_loop")) {
        fwrite("LOOP", 4, 1, crash_file);
    } else if (0 == failure.compare("fail_overflow")) {
        fwrite("OVERFLOW", 8, 1, crash_file);
    } else if (0 == failure.compare("fail_panic")) {
        fwrite("PANIC", 5, 1, crash_file);
    }
    CHECK(0); // This will crash and log that we made it here.
}

void
HandleQuery(int connection_fd) {
    struct handlerd_query msg;
    if (sizeof(msg) != recv(connection_fd, &msg, sizeof(msg),0)) {
        close(connection_fd); // May fail... But that's OK
    } else {
        std::string handle_str(msg.dst_handle);

        LOG("got message %s!", handle_str.c_str());
        if (handle_str.compare(0, 4, "fail") == 0) {
            LOG1("failing!");
            HandleFailure(handle_str);
        }
        Process::Process *process = Process::GetProcess(handle_str);
        if (process->AddToProbers(connection_fd, &msg)) {
            HandlerEvent he(process, KILL,
                            kMillisecondsToSeconds * kDefaultHandlerdSpyTimeout);
            handlerPQ_.push(he);
        }
    }
}

void
DoUnixHandshake(int connection_fd) {
    struct handlerd_unix_handshake handshake;
    int recv_bytes = recv(connection_fd, &handshake, sizeof(handshake), 0);
    if (recv_bytes == sizeof(handshake)) {
        std::string handle_str(handshake.handle);
        if (!Process::RegisterProcess(handle_str, handshake.pid, connection_fd)) {
            close(connection_fd);
            return;
        }
        struct epoll_event tmp_ev;
        tmp_ev.data.ptr = (void *) Process::GetProcess(handle_str);
        tmp_ev.events = EPOLLIN;
        CHECK(0 == epoll_ctl(unix_epoll_fd_, EPOLL_CTL_ADD, connection_fd, &tmp_ev));
    } else {
        // There was an error, but we don't know the process to kill it.
        // The spy should see that it's fd is closed and shoot the process.
        // No one will ever be the wiser.
        close(connection_fd);
    }
    return;
}

void
HandleNetConnection(void) {
    struct sockaddr_in tmp_addr;
    struct epoll_event tmp_ev;
    socklen_t tmp_len = sizeof(tmp_addr);
    int accept_fd = accept(net_socket_, (struct sockaddr *) &tmp_addr, &tmp_len);
    if (accept_fd == -1) {
        LOG1("Error accepting on NET socket");
        return;
    }
    CHECK(0 == fcntl(accept_fd, F_SETFL, O_NONBLOCK));
    memset(&tmp_ev, '\0', sizeof(tmp_ev));
    tmp_ev.data.fd = accept_fd;
    tmp_ev.events = EPOLLIN;
    CHECK(0 == epoll_ctl(net_epoll_fd_, EPOLL_CTL_ADD, accept_fd, &tmp_ev));
    return;
}

void
HandleUnixConnection(void) {
    struct sockaddr tmp_addr;
    struct epoll_event tmp_ev;
    socklen_t tmp_len = sizeof(tmp_addr);
    int accept_fd = accept(unix_socket_, &tmp_addr, &tmp_len);
    if (accept_fd == -1) {
        LOG1("Error accepting on UNIX socket");
        return;
    }
    fcntl(accept_fd, F_SETFL, O_NONBLOCK);
    memset(&tmp_ev, '\0', sizeof(tmp_ev));
    tmp_ev.data.fd = accept_fd;
    tmp_ev.events = EPOLLIN | EPOLLONESHOT;
    CHECK(0 == epoll_ctl(unix_new_fd_, EPOLL_CTL_ADD, accept_fd, &tmp_ev));
    return;
}
    
void
HandleNetMessage() {
    struct epoll_event events[kNumEpollEvents];
    int num_fds = epoll_wait(net_epoll_fd_, events, kNumEpollEvents, 0);
    DoTimecheck();
    for (int i = 0; i < num_fds; i++) {
        int curr_fd = events[i].data.fd;
        uint32_t event_list = events[i].events;
        if (event_list & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            // Something bad happened
            close(curr_fd);
        } else {
            HandleQuery(curr_fd);
        }
        DoTimecheck();
    }
    return;
}

void
HandleUnixMessage() {
    struct epoll_event events[kNumEpollEvents];
    int num_fds = epoll_wait(unix_epoll_fd_, events, kNumEpollEvents, 0);
    DoTimecheck();
    for (int i = 0; i < num_fds; i++) {
        Process::Process *process = (Process::Process *) events[i].data.ptr; 
        uint32_t event_list = events[i].events;
        if (event_list & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            if (process->Confirm()) {
        	process->ReplyDown();
	    } else {
		HandlerEvent he(process, CONFIRM, 
				kMillisecondsToSeconds *
				kDefaultHandlerdConfirmTimeout);
		handlerPQ_.push(he);
	    }
        } else {
            if (process->HandleUnixResponse()) {
		HandlerEvent he(process, CONFIRM, 
				kMillisecondsToSeconds *
				kDefaultHandlerdConfirmTimeout);
		handlerPQ_.push(he);
            } else {
                // TODO(leners): Add a probe event
                HandlerEvent he(process, PROBE,
                                kMillisecondsToSeconds * F_POLL_MS);
		handlerPQ_.push(he);
            }
        }
        DoTimecheck();
    }
    return;
}

void
HandleUnixNewConnection() {
    struct epoll_event events[kNumEpollEvents];
    int num_fds = epoll_wait(unix_new_fd_, events, kNumEpollEvents, 0);
    for (int i = 0; i < num_fds; i++) {
        int curr_fd = events[i].data.fd;
        uint32_t event_list = events[i].events;
        if (event_list & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            close(curr_fd);
        } else {
            DoUnixHandshake(curr_fd);
        }
    }
    return;
}

// Main loop of the program. Handles epoll events.
void
Loop() {
    struct epoll_event events[kNumEpollEvents];
    for (;;) {
        int num_fds = epoll_wait(main_epoll_fd_, events, kNumEpollEvents, 1);
	fflush(stderr);
	DoTimecheck();
        for (int i = 0; i < num_fds; i++) {
            int curr_fd = events[i].data.fd;
            if (curr_fd == unix_socket_) {
                HandleUnixConnection();
            } else if (curr_fd == net_socket_) {
                HandleNetConnection();
            } else if (curr_fd == net_epoll_fd_) {
                HandleNetMessage();
            } else if (curr_fd == unix_epoll_fd_) {
                HandleUnixMessage();
            } else if (curr_fd == unix_new_fd_) {
                HandleUnixNewConnection();
            }
        DoTimecheck();
        }
    }
}
} // end anonymous namespace

int
main(int argc, char **argv) {
    Initialize();
    Daemonize();
    Loop();
    return EXIT_FAILURE;
}
