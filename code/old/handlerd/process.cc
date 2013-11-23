#include "handlerd/process.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "common/common.h"
#include "common/messages.h"
#include "handlerd/log.h"

#define BADFD -1
namespace Process {

std::map<std::string,uint32_t> handle2generation_;
// Generational info
uint32_t hypervisor_generation_;
uint32_t domain_generation_;

void 
Process::ReplyGroup(app_state_t state) {
    confirm_wait_ms_ = 0;
    struct handlerd_query_response reply;
    reply.last_generation = htonl(handle2generation_[handle_]);
    CopyHandle(reply.handle, handle_.c_str());
    reply.state = state2char[state];
    std::list<int>::iterator i = waiting_pfds_.begin();
    for ( ; i != waiting_pfds_.end(); i++) {
        int send_ret = send(*i, &reply, sizeof(reply), 0);
        if ( send_ret != sizeof(reply)) {
            close(*i);
        }
    }
    queryno_++;
    waiting_pfds_.clear();
}

void
Process::ReplySolo(app_state_t state, int fd) {
    struct handlerd_query_response reply;
    reply.last_generation = htonl(handle2generation_[handle_]);
    CopyHandle(reply.handle, handle_.c_str());
    reply.state = state2char[state];
    int send_ret = send(fd, &reply, sizeof(reply), 0);
    if (send_ret != sizeof(reply)) {
        close(fd);
    }
}

void 
Process::SetConfirmWait(uint32_t confirm_wait_ms) {
    if ( confirm_wait_ms_ == 0 ||
         confirm_wait_ms_ > confirm_wait_ms) {
        confirm_wait_ms_ = confirm_wait_ms;
    }
    return;
}

void 
Process::CloseSocket() {
    if (socket_ != BADFD) {
        close(socket_);
        socket_ = BADFD;
    }
}

void 
Process::Update(pid_t pid, int socket) {
    LOG("Registering non-killed process %s", handle_.c_str());
    pid_ = pid;
    generation_ = handle2generation_[handle_];
    socket_ = socket;
    confirm_wait_ms_ = 0;
    registered_ = true;
}

Process::Process(const char *handle, pid_t pid, int socket) {
    handle_ = std::string(handle);
    pid_ = pid;
    if (handle2generation_[handle_] == 0) {
        handle2generation_[handle_]++;
    }
    generation_ = handle2generation_[handle_];
    socket_ = socket;
    queryno_ = 0;
    confirm_wait_ms_ = 0;
    registered_ = true;
    beakless_ = true;
    waiting_pfds_ = std::list<int>();
}

Process::Process(const char *handle) {
    handle_ = std::string(handle);
    generation_ = handle2generation_[handle_];
    queryno_ = 0;
    registered_ = false;
    beakless_ = true;
    waiting_pfds_ = std::list<int>();
}

bool 
Process::IsValid(void) {
    return registered_;
}

void 
Process::ReplyUp() {
    queryno_++;
    // ReplyGroup(ALIVE);
}

void 
Process::ReplyDown() {
    handle2generation_[handle_]++;
    LogGeneration(handle_);
    generation_++;
    registered_ = false || (beakless_ && socket_ != BADFD);
    ReplyGroup(DEAD);
}

void
Process::ReplyLongDead(int fd) {
    ReplySolo(DEAD, fd);
}

void 
Process::ReplyUnknown(int fd) {
    ReplySolo(UNKNOWN, fd);
}

bool
Process::AddToProbers(int pfd_fd, struct handlerd_query* msg) {
    uint32_t application_generation = ntohl(msg->app_generation);
    uint32_t domain_generation = ntohl(msg->domain_generation);
    uint32_t hypervisor_generation = ntohl(msg->hypervisor_generation);
    uint32_t application_generation_ = handle2generation_[handle_];
    // Check generational stuff
    uint32_t beak_mask = (1 << (sizeof(uint32_t) * 8 - 1));
    bool beakless = true;
    if (hypervisor_generation & beak_mask) {
        hypervisor_generation ^= beak_mask;
    } else if (hypervisor_generation != 0) {
        beakless = false;
    }
    if (hypervisor_generation < hypervisor_generation_) {
    	LOG("long dead hyp: %u %u", hypervisor_generation, hypervisor_generation_);
        ReplyLongDead(pfd_fd);
    } else if (hypervisor_generation > hypervisor_generation_) {
    	LOG("future hyp: %u %u", hypervisor_generation, hypervisor_generation_);
        ReplyUnknown(pfd_fd);
    } else if (domain_generation < domain_generation_) {
    	LOG("long dead domain: %u %u", hypervisor_generation, hypervisor_generation_);
        ReplyLongDead(pfd_fd);
    } else if (domain_generation > domain_generation_) {
    	LOG("future domain: %u %u", hypervisor_generation, hypervisor_generation_);
        ReplyUnknown(pfd_fd);
    } else if (application_generation < application_generation_) {
    	LOG("long dead app: %u %u", hypervisor_generation, hypervisor_generation_);
        ReplyLongDead(pfd_fd);
    } else if (application_generation > application_generation_) {
    	LOG("future app: %u %u", hypervisor_generation, hypervisor_generation_);
        ReplyUnknown(pfd_fd);
    } else if (!IsValid()) {
    	LOG("unregistered app: %u %u", hypervisor_generation, hypervisor_generation_);
        ReplyUnknown(pfd_fd);
    } else {
        if (!beakless) beakless_ = false;
        // Do a probe, or add to the list of guys waiting for a probe
        waiting_pfds_.push_back(pfd_fd);
    	if (waiting_pfds_.size() == 1) {
            Probe();
            return true;
        }
    }
    return false;
}

bool
Process::Probe() {
    if (!IsValid()) return false;
    struct handlerd_unix_probe msg;
    int send_ret = send(socket_, &msg, sizeof(msg), 0);
    if (send_ret != sizeof(msg)) {
        if (Confirm()) {
            LOG1("ReplyDown");
            ReplyDown();
            return false;
        } else {
            return true;
        }
    } else {
        return true;
    }
}

bool 
Process::Assassinate() {
    if (!IsValid()) return false;
    if (!beakless_) {
        kill(pid_, SIGKILL);
    }
    CloseSocket();
    if (ESRCH == errno) {
        LOG("successful assasination %s", handle_.c_str());
        return true;
    } else {
        LOG("unsuccessful assasination %s", handle_.c_str());
        CHECK(EINVAL != errno && EPERM != errno);
        return false;
    }

}

bool 
Process::Confirm() {
    if (!IsValid()) return true;
    kill(pid_, 0);
    if (ESRCH == errno) {
        CloseSocket();
        LOG("successful confirmation %s", handle_.c_str());
        ReplyDown();
	return true;
    } else {
        CloseSocket();
        LOG("unsuccessful confirmation %s", handle_.c_str());
        CHECK(EINVAL != errno && EPERM != errno);
    	return false;
    }
}

bool 
Process::HandleUnixResponse(void) {
    struct handlerd_unix_reply reply;
    int msg_len = recv(socket_, &reply, sizeof(reply), 0);
    if (msg_len != sizeof(reply)) {
      if (Confirm()) {
          return false;
      } else {
          return true;  
      }
    } else if (reply.state != ALIVE) {
        if (!beakless_) LOG("assassinate from handle unix response cw: %d", confirm_wait_ms_);
        if (beakless_ || Assassinate()) {
            LOG1("ReplyDown");
            ReplyDown();
            if (beakless_ && !Confirm()) {
                return true;
            } 
        } else {
            return true;
        }
    } else {
        ReplyUp();
    }
    return false;
}

uint32_t 
Process::GetConfirmWait(void) const {
    return confirm_wait_ms_;
}

pid_t
Process::GetPID() const {
    return pid_;
}

uint32_t 
Process::GetQueryNo(void) const {
    return queryno_;
}

std::map<std::string,Process *> handle2process_;
bool
RegisterProcess(const std::string& handle, pid_t pid, int socket) {
    Process *p = handle2process_[handle];
    if (!p) {
        handle2process_[handle] = new Process(handle.c_str(), pid, socket);
        return true;
    } else if (p->IsValid() && kill(p->GetPID(), 0) == 0) {
        return false;
    } else {
        LOG1("re-initializing applicaiton");
        p->Update(pid, socket);
        return true;
    }
}

Process *
GetProcess(std::string& handle) {
    Process *p = handle2process_[handle];
    if (!p) {
        p = new Process(handle.c_str());
        handle2process_[handle] = p;
    }
    return p;
}

static const uint32_t kBufferSize = 256;
void
InitGenerations() {
    InitGenerationLog(handle2generation_);
    
    int gen_fd = open("/mnt/gen/hypervisor", O_RDONLY);
    CHECK(gen_fd >= 0);
    CHECK(sizeof(uint32_t) == read(gen_fd, &(hypervisor_generation_), sizeof(uint32_t)));
    close(gen_fd);

    char buf[kBufferSize];
    char hostname[kBufferSize];
    CHECK(0 == gethostname(hostname, kBufferSize));
    sprintf(buf, "/mnt/gen/%s", hostname);
    gen_fd = open(buf, O_RDONLY);
    CHECK(gen_fd >= 0);
    CHECK(sizeof(uint32_t) == read(gen_fd, &(domain_generation_), sizeof(uint32_t)));
    close(gen_fd);
    LOG("generations: %d %d", domain_generation_, hypervisor_generation_);
}

} // end namespace Process
