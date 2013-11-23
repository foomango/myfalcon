/* 
 * Copyright (c) 2011 Joshua B. Leners (University of Texas at Austin).
 * All rights reserved.
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of Texas at Austin. The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 */

//
// Diomedes replication service
//
#include "diomedes/diomedes.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <rpc/rpc.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <list>
#include <map>
#include <set>
#include <string>

#include "common.h"
#include "config.h"
#include "diomedes/diomedes_message.h"
#include "libfail/saboteur.h"
#include "process_enforcer/process_observer.h"
#include "client/client.h"

namespace Diomedes {

class Application {
  public:
    Application(const char* aname,
                const char* hname,
                const char* vname,
                const char* sname) :
      app_name(aname), host_name(hname),
      vmm_name(vname), switch_name(sname) {
        app_id.append(aname);
        app_id.append(":");
        app_id.append(hname);
        app_id.append(":");
        app_id.append(vname);
        app_id.append(":");
        app_id.append(sname);
      }
    const char *GetHandle() const {
      return app_name.c_str();
    }
    const char *GetVMMHostname() const {
      return vmm_name.c_str();
    }
    const char* GetHostname() const {
      return host_name.c_str();
    }
    const char* GetSwitchName() const {
      return switch_name.c_str();
    }
    const char* GetAppString() const {
      return app_id.c_str();
    }
  private:
    const std::string app_name;
    const std::string host_name;
    const std::string vmm_name;
    const std::string switch_name;
    std::string app_id;
};

class ApplicationInstance {
  public:
    falcon_target* target;
    ApplicationInstance(Application* app) : app_(app) { }
    Application* GetApplicationIdentifier() {
      return app_;
    }
  private:
    Application *app_;
};

const long kDefaultJoinTimeoutMilliseconds = 1000;
const uint32_t kDefaultSpyWarmup = 100;
const double kDefaultSpyMultiplier = 500.0;

// These are the states a Diomedes replica can be in. Here is a transition 
// diagram:
// BOOTSTRAP ---> BACKUP
//    |          /
//    |         /
//    V       |/_
// NEW_PRIMARY
//    |
//    |
//    V
// WAITING_FOR_REQUESTS <-----> PROCESSING_REQUEST
typedef enum mode_s {
    BOOTSTRAP,
    BACKUP,
    ADDING_REPLICA,
    PROCESSING_REQUEST,
    WAITING_FOR_REQUESTS,
    NEW_PRIMARY,
    NEW_BACKUP
} replica_mode;

replica_mode mode_;

// NEW_BACKUP and ADDING_REPLICA are unused at this time, but would be used 
// for adding a new replica after BOOTSTRAP (The primary coordinates the join,
// which is treated as a no-op request to the state machine. It also requires
// ALL replicas to participate, not just f).
//
// In BOOTSTRAP the replicas look for a set of (f+1) replicas, when they've found
// (f+1) replicas (including themselves) the ElectNewPrimary(). If they are a new
// primary, they wait for the votes or failure of every other replica. 
//
// During normal operation, the primary process one request at a time (in a 
// real system it would likely batch requests), and is either waiting for
// requests or processing requests.
//
// To process a request, the primary broadcasts the request and waits for 
// responses from (f - o) replicas (where o is the number of observed 
// failures. If post-bootstrap joins were implementeted, o would be increased
// (up to f) by failures and decreased (down to zero) by joins.
// Globals

// Assumptions about clients: 
//      1 client per IPv4 address
//      Client sends one request at a time
//      Client will repeat RPC xid for repeat request

// Here is a high level description for the primary:
// wait for everyone to acknowledge self as primary.
// wait for requests req:
//      broadcast req to replicas
//      process req
//      wait for f acks* (meaning f+1 replicas got it)
//      reply to client
// 
// *actually f - o acks where o is the number of observed failures so far
// (see note above)
//
// backup:
// wait for primary request or primary failure:
//   on request:
//      send ack to primary
//      process request
//      add request/response to cache
//   on failure:
//      select new primary
//      if self is not primary, run backup

// File descriptors
int server_socket_;
int __my_pipe_[2];
int failure_pipe_in_;
int failure_pipe_out_;
int my_epoll_;
int request_socket_;
int sm_sock_;

// Useful addresses
struct sockaddr_in my_addr_;
struct sockaddr_in bcast_addr_;
struct sockaddr_in primary_addr_;

// Identification
char handle_[32];

// Useful ids
diomedes_id primary_;
diomedes_id self_;

// Fault tolerence stuff
std::list<ApplicationInstance*> active_servers_;
std::set<std::string> pending_joins_; // Also protected by active_servers_lock_
uint32_t failures_observed_ = 0;
long failures_; // To be set by config
ApplicationInstance *primary_instance_;

// Request processing stuff
// The id of the next request to process
uint32_t curr_req_id_ = 0;
// The id of the last request that got enough ACKs
uint32_t last_valid_req_id_ = 0;
std::set<std::string> ackers_;
// used by the backups
uint32_t backup_poll_interval_ms_ = kDefaultBackupPollIntervalMilliseconds;
bool primary_active_ = false;
bool bootstrap_primary_active_ = false;
std::set<ApplicationInstance*> nacked_;
std::map<uint32_t,sockaddr_in*> client_map_;

// Used by backups in case primary fails
std::map<uint32_t, diomedes_message*> request_map_;
// Used to reply to duplicate requests.
std::map<uint32_t, tcp_rpc_msg*> response_map_;
char __last_primary_request_buf_[kMaxDiomedesMessage];
diomedes_message* last_primary_request_ = (diomedes_message *) __last_primary_request_buf_;
// Synchronization
bool spy_signaled_ = false;
pthread_mutex_t spy_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t spy_cond_ = PTHREAD_COND_INITIALIZER;
pthread_mutex_t failure_pipe_lock_ = PTHREAD_MUTEX_INITIALIZER;
bool pipe_failed_ = false;
uint32_t acks_needed_;
pthread_mutex_t responder_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t responder_cond_ = PTHREAD_COND_INITIALIZER;
pthread_mutex_t active_servers_lock_ = PTHREAD_MUTEX_INITIALIZER;

double spy_multiplier_;
uint32_t spy_warmup_;
uint32_t poll_freq_ms_;

//
// Function Prototypes

// Thread that polls pfd for failure status. Argument is an application
// instance to poll
// Sends notfication of failed ApplicationInstance over the failure pipe.
// NULL argument indicates that the spy was queried to test diomedes server's
// main loop.
void SendFailed(ApplicationInstance*);
// Send message of type * to specified address
void SendJoin(struct sockaddr_in*);
void SendNack(struct sockaddr_in*);
// Periodically sent by the backup. Also used as an ACK to a primary request.
void SendVote();
// Sends tcp_rpc_msg to state machine (if necessary)
bool RunRequest(tcp_rpc_msg* request, uint32_t addr);
// Used by the primary to rebroadcast last received request
void BroadcastPrimaryRequest();
// Places primary request into last_primary_request_buffer_
bool BuildPrimaryRequest(diomedes_message *msg, struct sockaddr_in *from);
void ClearRequestCache();
void ProcessBackupMessages();
void* Responder(void *);
//
// Actual Code

// Utility function for comparing ApplicationInstances. Should this be a part
// of the PFD library?
ApplicationInstance*
MaxApplicationInstance (ApplicationInstance* app1, 
                        ApplicationInstance* app2) {
    if (NULL == app1) {
        return app2;
    } else if (NULL == app2) {
        return app1;
    }
    CHECK(app1 && app2);
    unsigned int count1 = nacked_.count(app1);
    unsigned int count2 = nacked_.count(app2);
    CHECK(count1 == 0 || count2 == 0);
    if (count1 != 0) {
        return app2;
    } else if (count2 != 0) {
        return app1;
    }
    Application* app_one = app1->GetApplicationIdentifier();
    Application* app_two = app2->GetApplicationIdentifier();

    int hyp_cmp = strcmp(app_one->GetVMMHostname(), app_two->GetVMMHostname());
    if (hyp_cmp != 0) {
        return (hyp_cmp < 0) ? app1 : app2;
    }
    int dom_cmp = strcmp(app_one->GetHostname(), app_two->GetHostname());
    if (dom_cmp != 0) {
        return (dom_cmp < 0) ? app1 : app2;
    }
    int handle_cmp = strcmp(app_one->GetHandle(), app_two->GetHandle());
    if (handle_cmp != 0) {
        return (handle_cmp < 0) ? app1 : app2;
    }
    // They are the same, add an assert here...
    LOG("About to crash b/c : %s", app_one->GetAppString());
    LOG("is ths ame as: %s", app_two->GetAppString());
    CHECK(0);
    return app1; // otherwise, returning one of them arbitrarily is correct!
}

// Compare diomedes_id and instance
bool
CompareDiomedesIdToInstance(const diomedes_id* id,
                            ApplicationInstance* inst) {
    CHECK(inst);
    CHECK(id);
    Application*app = inst->GetApplicationIdentifier();
    return !strncmp(app->GetHandle(), id->handle, kNameLength) &&
           !strncmp(app->GetHostname(), id->domain, kNameLength) &&
           !strncmp(app->GetVMMHostname(), id->hypervisor, kNameLength);
}

bool
CompareDiomedesIds(const diomedes_id* id1, const diomedes_id* id2) {
    CHECK(id1);
    CHECK(id2);
    return !strncmp(id1->handle, id2->handle, kNameLength) &&
           !strncmp(id1->domain, id2->domain, kNameLength) &&
           !strncmp(id1->hypervisor, id2->hypervisor, kNameLength);
}

// Select a new primary. Ignore anyone who has nacked an earlier vote.
ApplicationInstance*
SelectNewPrimary() {
    CHECK(0 == pthread_mutex_lock(&active_servers_lock_));
    std::list<ApplicationInstance*>::iterator it;
    ApplicationInstance *max = NULL;
    for (it = active_servers_.begin(); it != active_servers_.end(); it++) {
        // Don't select someone who has nacked
        if (nacked_.count(*it) > 0) continue;
        max = MaxApplicationInstance(max, *it);
    }
    CHECK(0 == pthread_mutex_unlock(&active_servers_lock_));
    return max;
}

// Setup state for new primary
void
ElectPrimary() {
    primary_instance_ = SelectNewPrimary();
    // Although SelectNewPrimary can return NULL, self_ will never nack
    CHECK(primary_instance_);
    const Application* app = primary_instance_->GetApplicationIdentifier();
    strcpy(primary_.handle, app->GetHandle());
    strcpy(primary_.domain, app->GetHostname());
    strcpy(primary_.hypervisor, app->GetVMMHostname());
    // get the primary's address
    struct addrinfo hints;
    struct addrinfo* ret;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    // Perhaps cache address from join rather than do lookup now?
    CHECK(0 == getaddrinfo(primary_.domain, NULL, &hints, &ret));
    CHECK(ret->ai_family == AF_INET);
    memcpy(&primary_addr_, ret->ai_addr, sizeof(primary_addr_));
    primary_addr_.sin_port = htons(kDiomedesServerPort);
    freeaddrinfo(ret);
    primary_active_ = true;
    LOG("Elected a primary: in_addr 0x%08X", primary_addr_.sin_addr.s_addr);
}

// Setup subprocess state machine. Much of the code is taken from Paxos 
// Made Practical source to ensure comapatability of underlying state 
// machines
void 
SetupStateMachine(std::string const &sm_path) {
    int fds[2];
    CHECK(0 == pipe(fds));

    int sm_pid = fork();
    CHECK(sm_pid >= 0);

    if (sm_pid == 0) {
        // Child
        close(fds[0]);
        CHECK(STDOUT_FILENO == dup2(fds[1], STDOUT_FILENO));
        char *args[1] = {NULL};
        CHECK(-1 != execv(sm_path.c_str(), args));
    } else {
        // Parent. Modification of dm's code.
        char buf[kBufferSize];
        close(fds[1]);
        int read_bytes = read(fds[0], buf, kBufferSize);
        CHECK(read_bytes > 0);
        buf[read_bytes - 1] = '\0';
        char prefix[] = "Listening on port ";
        CHECK(0 == strncmp(&buf[0], prefix, strlen(prefix)));
        int port = atoi(&buf[strlen(prefix)]);

        // Setup RPC transport
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        char server[] = "localhost";
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);

        struct hostent *he = gethostbyname(server);
        CHECK(he);
        int on = 1;
        memcpy(&sin.sin_addr, he->h_addr, sizeof(sin.sin_addr));
        sm_sock_ = socket(AF_INET, SOCK_STREAM, 0);
        CHECK(sm_sock_);
        CHECK(connect(sm_sock_, (struct sockaddr *)&sin, sizeof(sin)) == 0);
        CHECK(0 == setsockopt(sm_sock_, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)));
    }
}

// This is for join after start.
static void
ForwardJoin(diomedes_join* join) {
    // TODO(leners): Join after start
    CHECK(CompareDiomedesIds(&primary_, &self_));
    return;
}

uint32_t
GetActiveServersCount() {
    uint32_t ret = 0;
    CHECK(0 == pthread_mutex_lock(&active_servers_lock_));
    ret = active_servers_.size();
    CHECK(0 == pthread_mutex_unlock(&active_servers_lock_));
    return ret;
}

void
RemoveFromActiveServers(ApplicationInstance *app) {
    CHECK(0 == pthread_mutex_lock(&active_servers_lock_));
    active_servers_.remove(app);
    CHECK(0 == pthread_mutex_unlock(&active_servers_lock_));
    return;
}

void
AddToActiveServers(ApplicationInstance *app) {
    CHECK(0 == pthread_mutex_lock(&active_servers_lock_));
    active_servers_.push_back(app);
    std::string join_str;
    join_str.append(app->GetApplicationIdentifier()->GetHandle());
    join_str.append(app->GetApplicationIdentifier()->GetHostname());
    join_str.append(app->GetApplicationIdentifier()->GetVMMHostname());
    join_str.append(app->GetApplicationIdentifier()->GetSwitchName());
    pending_joins_.erase(join_str);
    CHECK(0 == pthread_mutex_unlock(&active_servers_lock_));
    return;
}

void
diomedes_wrapper(const Falcon::LayerIdList& l, void* arg,
                 uint32_t fs, uint32_t rs) {
  Falcon::LogCallbackData(l, fs, rs);
  SendFailed((ApplicationInstance*) arg);
}

void *
AddAppInstance(void *arg) {
    Application* app_id = (Application *) arg;
    ApplicationInstance* joiner = new ApplicationInstance(app_id);
    std::vector<std::string> id_list;
    id_list.push_back(app_id->GetHandle());
    id_list.push_back(app_id->GetHostname());
    id_list.push_back(app_id->GetVMMHostname());
    id_list.push_back(app_id->GetSwitchName());
    falcon_target* f = Falcon::init(id_list, true, joiner, -1);
    joiner->target = f;
    AddToActiveServers(joiner);
    return NULL;
}

// Process a join. Returns true if joiner was added to active servers and 
// false otherwise.
static bool
ProcessJoin(diomedes_join* join, struct sockaddr_in* addr) {
    CHECK(0 == pthread_mutex_lock(&active_servers_lock_));
    std::string join_str;
    join_str.append(join->joiner.handle);
    join_str.append(join->joiner.domain);
    join_str.append(join->joiner.hypervisor);
    join_str.append(kSwitchName);
    std::list<ApplicationInstance*>::iterator it;
    bool need_to_add = true;
    if (pending_joins_.count(join_str) > 0) {
        need_to_add = false;
    } else {
        for (it = active_servers_.begin(); it != active_servers_.end(); it++) {
            if (CompareDiomedesIdToInstance(&join->joiner, *it)) {
                // Anyone we've acknowledged who's broadcasting could need our 
                // join. Perhaps we should make sure the instance we think it is
                // is still up, but that is for Join After Start Logic
                if (join->broadcast) {
                    SendJoin(addr);
                }
                need_to_add = false;
                break;
            }
        }
    }
    if (need_to_add) {
        pending_joins_.insert(join_str);
        Application* app_id = new Application(join->joiner.handle,
                                              join->joiner.domain,
                                              join->joiner.hypervisor,
                                              kSwitchName);
        pthread_t joiner;
        CHECK(0 == pthread_create(&joiner, NULL, AddAppInstance, app_id));
        CHECK(0 == pthread_detach(joiner));
    }
    CHECK(0 == pthread_mutex_unlock(&active_servers_lock_));
    return need_to_add;
}

// Replica believes it is in bootstrap collecting joins. If it gets a vote, it
// will NACK b/c its not ready. This will need to be changed for join after
// start
static void
ProcessJoins() {
    static char buf[kMaxDiomedesMessage];
    static diomedes_message *msg = (diomedes_message *) buf;
    struct sockaddr_in from;
    CHECK(mode_ == BOOTSTRAP);
    for (;;) {
        socklen_t len = sizeof(from);
        int read_bytes = recvfrom(server_socket_, buf, kMaxDiomedesMessage, 
                                  MSG_DONTWAIT, 
                                  (struct sockaddr *) &from, &len);
        if (read_bytes < 0) {
            CHECK (errno == EAGAIN || errno == EWOULDBLOCK);
            break;
        }
        // Not from ourselves
        if (from.sin_addr.s_addr == my_addr_.sin_addr.s_addr) continue;
        uint16_t msg_len = htons(msg->length);
        if (msg_len + kDiomedesHeaderSize  != read_bytes) {
            LOG("Malformed join: expected %d got %d", msg_len + kDiomedesHeaderSize,
                                                      read_bytes);
            continue;
        }
        // We are still in Bootstrap, we can't be elected.
        if (msg->type == 'V') {
            SendNack(&from);
        }
        // TODO(leners): Post-bootstrap join
        if (msg->type != 'J') continue;
        diomedes_join* join = &(msg->payload.join);
        ProcessJoin(join, &from);
    } 
    return;
}

// Spy-related functions. This could be changed dynamically, for now it is
// fixed by a constant, for eval it will be fixed by a config option.
uint32_t
GetPollFrequency(ApplicationInstance* app) {
    return poll_freq_ms_;
}

// Background thread for checking failure.
/*
void*
PollForFailure(void* arg) {
    pthread_detach(pthread_self());
    ApplicationInstance* app = (ApplicationInstance *) arg;
    app_status status;
    uint32_t freq_ms = GetPollFrequency(app);
    LOG("Starting to monitor %s",
            app->GetApplicationIdentifier()->GetAppString());
    do {
        usleep(kMillisecondsToMicroseconds * freq_ms);
        status = pfd_->GetAppStatus(app);
    } while (status != DOWN_FROM_HANDLER &&
             status != DOWN_FROM_VMM &&
             status != DOWN_FROM_SWITCH);
    LOG("Something failed! %s",
            app->GetApplicationIdentifier()->GetAppString());
    LOG("Status %s", status2str[status]);
    SendFailed(app);
    return NULL;
}
*/

// Actual spy just checks to see that the main loop is processing failures.
uint32_t
Spy() {
    static uint64_t probes_seen = 0;
    static double time_waiting_on_loop = 0.0;
    CHECK(0 == pthread_mutex_lock(&spy_lock_));
    spy_signaled_ = false;
    SendFailed(NULL);
    int rc = 0;
    double start = GetRealTime();
    // Could do an moving average, but for now just use warmup.
    if (probes_seen < spy_warmup_) {
        probes_seen++;
        while(!spy_signaled_) {
            CHECK(0 == pthread_cond_wait(&spy_cond_, &spy_lock_));
        }
        double end = GetRealTime();
        time_waiting_on_loop += end - start;
    } else {
        struct timespec tp;
        double time_to_wait = (spy_multiplier_) * (time_waiting_on_loop/probes_seen);
        clock_gettime(CLOCK_REALTIME, &tp);
        IncrementTimespecSeconds(&tp, time_to_wait);
        while (!spy_signaled_ && rc != ETIMEDOUT) {
            rc = pthread_cond_timedwait(&spy_cond_, &spy_lock_, &tp);
            CHECK(rc == 0 || rc == ETIMEDOUT);
        }
    }
    if (rc == ETIMEDOUT) {
        LOG("About to die b/c of timing: %f", (spy_multiplier_) * (time_waiting_on_loop/probes_seen));
    }
    CHECK(0 == pthread_mutex_unlock(&spy_lock_));
    return (rc == ETIMEDOUT) ? 1 : 0;
}

// Called by ProcessFailures to show the spy that the main loop is running
void
SignalSpy() {
    CHECK(0 == pthread_mutex_lock(&spy_lock_));
    spy_signaled_ = true;
    CHECK(0 == pthread_cond_signal(&spy_cond_));
    CHECK(0 == pthread_mutex_unlock(&spy_lock_));
    return;
}

// Put a failure on the failure pipe
void
SendFailed(ApplicationInstance* failed) {
    pthread_mutex_lock(&failure_pipe_lock_);
    if (pipe_failed_) return;
    CHECK(sizeof(intptr_t) == write(failure_pipe_in_, &failed,
                                   sizeof(intptr_t)));
    pthread_mutex_unlock(&failure_pipe_lock_);
    return;
}

// Get a failure off the failure pipe
void
ReadFailed(ApplicationInstance** failed) {
    pthread_mutex_lock(&failure_pipe_lock_);
    if (pipe_failed_) return;
    CHECK(sizeof(intptr_t) == read(failure_pipe_out_, failed,
                                   sizeof(intptr_t)));
    pthread_mutex_unlock(&failure_pipe_lock_);
    return;
}

// Handles failure sent by the spy or failure polling threads
void
ProcessFailure() {
    ApplicationInstance* failed;
    ReadFailed(&failed);
    // It is a message from the spy thread
    if (failed == NULL) {
        SignalSpy();
    } else if (failed == (ApplicationInstance *)(&self_)) { // This is a hack to allow for livelock.
        return;
    } else {
        LOG("Got a failure %s",
                failed->GetApplicationIdentifier()->GetAppString());
        if (failed == primary_instance_) {
            primary_active_ = false;
            bootstrap_primary_active_ = false;
        }
        RemoveFromActiveServers(failed);
        failures_observed_++;
        acks_needed_ = failures_ - failures_observed_;
        pthread_cond_broadcast(&responder_cond_);
    }
    return;
}
// End Spy-related functions

// Add a vote to ackers, if there are enough acks, signal the thread waiting
// to respond to the client.
void
AddToAckers(diomedes_message* msg) {
    if (ntohl(msg->payload.vote.last_req.req_id) < curr_req_id_) {
        // BroadcastPrimaryRequest();
        return;
    }
    if (msg->payload.vote.last_req.request.msg.rm_xid != last_primary_request_->payload.preq.request.msg.rm_xid) {
        LOG("0x%08x vs. 0x%08x", (uint32_t)last_primary_request_->payload.preq.request.msg.rm_xid,
            (uint32_t) msg->payload.vote.last_req.request.msg.rm_xid);
        LOG("%d %d", ntohl(msg->payload.vote.last_req.req_id), curr_req_id_);
        LOG("%c %s", msg->type, msg->payload.vote.voter.domain);
    }
    CHECK(last_primary_request_->payload.preq.request.msg.rm_xid == 
          msg->payload.vote.last_req.request.msg.rm_xid);
    std::string acker(msg->payload.vote.voter.handle);
    acker.append(msg->payload.vote.voter.domain);
    acker.append(msg->payload.vote.voter.hypervisor);
    ackers_.insert(acker);
    if (ackers_.size() == acks_needed_) {
        pthread_mutex_lock(&responder_lock_);
        last_valid_req_id_++;
        ackers_.clear();
        pthread_cond_signal(&responder_cond_);
        pthread_mutex_unlock(&responder_lock_);
        mode_ = WAITING_FOR_REQUESTS;
    }
    return;
}

// Catch up a very behind node (in the case of n > f + 1 replicas, OR join 
// after start)
void
DoCatchup(diomedes_message* msg) {
    CHECK(mode_ != PROCESSING_REQUEST);
    // Could implement as follows:
    // For join-after-start state_file from the rsm, send state to the backup 
    // via TCP
    // For catchup look at the literature for logging/garbage collection/and
    // such
    return;
}

void
RunFromRequestCache() {
    struct epoll_event events[kNumEpollEvents];
    while (request_map_.size() > 0) {
        int num_events = epoll_wait(my_epoll_, events, kNumEpollEvents, -1);
        for (int i = 0; i < num_events; i++) {
            // Only server left, no matter what reset to waiting for requests
            if (GetActiveServersCount() == 1) mode_ = WAITING_FOR_REQUESTS;
            int curr_fd = events[i].data.fd;
            if (curr_fd == failure_pipe_out_) {
                ProcessFailure();
            } else if (curr_fd == server_socket_) {
                ProcessBackupMessages();
            } else {
                LOG1("Some kind of prank message");
            }

        }
        if (mode_ == WAITING_FOR_REQUESTS) {
            diomedes_message* request = request_map_.begin()->second;
            sockaddr_in* from = client_map_[request->payload.creq.client_id];
            CHECK(BuildPrimaryRequest(request, from));
            mode_ = PROCESSING_REQUEST;
            curr_req_id_++;
            BroadcastPrimaryRequest();
            CHECK(RunRequest(&(last_primary_request_->payload.preq.request),
                    last_primary_request_->payload.preq.client_id));
            pthread_t new_thread;
            ackers_.clear();
            acks_needed_ = failures_ - failures_observed_;
            void** arg = new void*[3];
            sockaddr_in *addr = new sockaddr_in(*from);
            arg[0] = (void *) addr;
            uint32_t* req_no = new uint32_t(curr_req_id_);
            arg[1] = (void *) req_no;
            uint32_t* cli_id = new uint32_t(last_primary_request_->payload.preq.client_id);
            arg[2] = (void *) cli_id;
            pthread_create(&new_thread, NULL, &Responder, arg);
            pthread_detach(new_thread);
            request_map_.erase(request_map_.begin());
        }
    }
    return;
}

// Count votes to make transition from NEW_PRIMARY to PRIMARY
void
CountVote(diomedes_message* msg, struct sockaddr_in* backup) {
    LOG1("I gots a vote!");
    // We are only a primary waiting to be elected once.
    static size_t votes_needed = GetActiveServersCount() - 1;
    static std::set<std::string> votes;
    static uint32_t min_req_id = curr_req_id_;
    static uint32_t max_req_id = curr_req_id_;
    if (votes_needed == 0) return;
    std::string voter(msg->payload.vote.voter.handle);
    voter.append(msg->payload.vote.voter.domain);
    voter.append(msg->payload.vote.voter.hypervisor);
    votes.insert(voter);
    uint32_t vote_req_id = htonl(msg->payload.vote.last_req.req_id);
    // Note, these assume the primary can only be ahead or behind by one
    if (vote_req_id < min_req_id) {
        // Need to rerun last request
        BroadcastPrimaryRequest();
        min_req_id = vote_req_id;
    } else if (vote_req_id > max_req_id) {
        // Need to run this new request.
        LOG("Running request: %d %d, thanks %s %c", vote_req_id, max_req_id,
            voter.c_str(), msg->type);
        max_req_id = vote_req_id;
        int preq_size = ntohs(msg->length) - sizeof(diomedes_id);
        memcpy(&(last_primary_request_->payload.preq),
               &(msg->payload.vote.last_req),
               preq_size);
        last_primary_request_->length = htons(preq_size);
        BroadcastPrimaryRequest();
        RunRequest(&(last_primary_request_->payload.preq.request),
                   last_primary_request_->payload.preq.client_id);
    }
    if (votes_needed == votes.size()) {
        // Everyone was in agreement about where we were
        // time to start responding.
        if (min_req_id == max_req_id) {
            last_valid_req_id_ = curr_req_id_;
        }
        LOG1("I am king!");
        ClearRequestCache();
        mode_ = WAITING_FOR_REQUESTS;
        RunFromRequestCache();
    } else {
        LOG("I still need %d votes I have %d", (int) votes_needed, (int)votes.size());
    }
    return;
}

// How the primary handles replica
void
ProcessBackupMessage(diomedes_message* msg, struct sockaddr_in *backup) {
    switch (msg->type) {
        case 'J':
            // TODO(leners): Join after start
            if (ProcessJoin(&(msg->payload.join), backup)) {
                mode_ = ADDING_REPLICA;
                ForwardJoin(&(msg->payload.join));
            }
            break;
        case 'V':
            if (mode_ != NEW_PRIMARY && mode_ != PROCESSING_REQUEST) {
                break; // Need to check how behidn replica is for n > (f+1)
                       // case
            } else if (mode_ == NEW_PRIMARY) {
                CountVote(msg, backup);
            } else {
                AddToAckers(msg);
            }
            break;
        case 'R':
            // Shouldn't happen
            CHECK(0);
            break;
        default:
            LOG("Unknown message type received: %c", msg->type);
            break;
    }
    return;
}

// Send a response to a client.
void
SendClientResponse(struct sockaddr_in* from, uint32_t client_id) {
    static char buf[kMaxDiomedesMessage];
    memset(buf, 0, kMaxDiomedesMessage);
    diomedes_message *msg = (diomedes_message*) buf;
    msg->type = CLIENT_RESPONSE;
    msg->version = 0;
    msg->payload.resp.client_id = client_id;
    int msg_size = 0xFFFFFF & response_map_[client_id]->size;
    memcpy(&(msg->payload.resp.request), response_map_[client_id], msg_size);
    msg->length = htons(msg_size + sizeof(diomedes_client_response)
                      - sizeof(tcp_rpc_msg));
    int send_size = ntohs(msg->length) + kDiomedesHeaderSize;
    CHECK(send_size == sendto(request_socket_, msg, send_size, 0,
                              (struct sockaddr *) from, sizeof(*from)));
    return;
}

// Thread waits for acks_needed_ = 0 (b/c of failures) or for 
// last_valid_req_id_ to be incremeneted.
void*
Responder(void *arg) {
    void** arg_i = (void **) arg;
    struct sockaddr_in* from = static_cast<struct sockaddr_in *>(arg_i[0]);
    uint32_t* req_id = (uint32_t *)arg_i[1];
    uint32_t* client_id = (uint32_t *)arg_i[2];
    delete [] arg_i;
    pthread_mutex_lock(&responder_lock_);
    while (last_valid_req_id_ < *req_id && acks_needed_ != 0) {
        pthread_cond_wait(&responder_cond_, &responder_lock_);
    }
    if (acks_needed_ == 0) last_valid_req_id_ = *req_id;
    SendClientResponse(from, *client_id);
    pthread_mutex_unlock(&responder_lock_);
    delete from;
    delete req_id;
    delete client_id;
    return NULL;
}

// Loop to pull backup messages off of server socket
void
ProcessBackupMessages() {
    static char buf[kMaxDiomedesMessage];
    static diomedes_message* msg = (diomedes_message*) buf;
    memset(msg, 0, kMaxDiomedesMessage);
    struct sockaddr_in from;
    socklen_t alen = sizeof(from);
    int recv_bytes = recvfrom(server_socket_, buf, kMaxDiomedesMessage, 
                              MSG_DONTWAIT, (struct sockaddr *) &from, &alen);
    do {
        // Not malformed or from ourselves
        if (ntohs(msg->length) + kDiomedesHeaderSize == recv_bytes &&
            from.sin_addr.s_addr != my_addr_.sin_addr.s_addr) {
            ProcessBackupMessage(msg, &from);
        }
        memset(msg, 0, kMaxDiomedesMessage);
        recv_bytes = recvfrom(server_socket_, buf, kMaxDiomedesMessage, 
                              MSG_DONTWAIT, (struct sockaddr *) &from, &alen);
    } while(recv_bytes > 0);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    return;
}

// Build a primary request from a client request.
bool
BuildPrimaryRequest(diomedes_message *msg, struct sockaddr_in *from) {
    CHECK(mode_ == WAITING_FOR_REQUESTS);
    if (msg->type != CLIENT_REQUEST) {
        return false;
    }
    memcpy(last_primary_request_, msg, kMaxDiomedesMessage);
    msg = last_primary_request_;
    msg->type = PRIMARY_REQUEST;
    msg->version = 0;
    // Leave addr + length + request the same.
    msg->payload.preq.req_id = htonl(curr_req_id_ + 1);
    return true;
}

// Broadcast the last built primary request.
void
BroadcastPrimaryRequest() {
    diomedes_message *msg = last_primary_request_;
    int send_size = ntohs(msg->length) + kDiomedesHeaderSize;
    CHECK(msg->type == PRIMARY_REQUEST);
    CHECK(send_size == sendto(server_socket_, msg, send_size, 0, 
                              (struct sockaddr *) &bcast_addr_,
                              sizeof(bcast_addr_)));
}

// Process a client socket with data to be read.
void
HandleRequestAsPrimary() {
    CHECK(mode_ == WAITING_FOR_REQUESTS);
    static char buf[kMaxDiomedesMessage];
    struct sockaddr_in from;
    memset(buf, 0, kMaxDiomedesMessage);
    diomedes_message *msg = (diomedes_message *) buf;
    socklen_t alen = sizeof(from);
    int recv_bytes = recvfrom(request_socket_, msg, kMaxDiomedesMessage, 0, 
                              (struct sockaddr *) &from, &alen);
    CHECK(recv_bytes > 0); // epoll got us here
    if (!BuildPrimaryRequest(msg, &from)) return;
    curr_req_id_++;
    BroadcastPrimaryRequest();
    // Responded from our cache, don't need to wait for acks
    if (!RunRequest(&(last_primary_request_->payload.preq.request),
                    last_primary_request_->payload.preq.client_id)) {
        SendClientResponse(&from, last_primary_request_->payload.preq.client_id);
        return;
    }
    mode_ = PROCESSING_REQUEST;
    pthread_t new_thread;
    ackers_.clear();
    acks_needed_ = failures_ - failures_observed_;
    void** arg = new void*[3];
    sockaddr_in *addr = new sockaddr_in(from);
    arg[0] = (void *) addr;
    uint32_t* req_no = new uint32_t(curr_req_id_);
    arg[1] = (void *) req_no;
    uint32_t* cli_id = new uint32_t(last_primary_request_->payload.preq.client_id);
    arg[2] = (void *) cli_id;
    pthread_create(&new_thread, NULL, &Responder, arg);
    pthread_detach(new_thread);
    return;
}

void
ClearRequestCache() {
    std::map<uint32_t,diomedes_message*>::iterator it;
    for (it = request_map_.begin(); it != request_map_.end();) {
        uint32_t id = it->first;
        diomedes_message* request = it->second;
        tcp_rpc_msg* response = response_map_[id];
        it++;
        if (request->payload.creq.request.msg.rm_xid == response->msg.rm_xid) {
            SendClientResponse(client_map_[id], id);
            request_map_.erase(id);
            free(request);
        } 
    }
    LOG("request map is %d after clearing", (int) request_map_.size());
    return;
}

void
UpdateClientMap(uint32_t id, sockaddr_in* from) {
    if (client_map_[id] == NULL) {
        client_map_[id] = new sockaddr_in(*from);
    } else if (client_map_[id]->sin_addr.s_addr != from->sin_addr.s_addr) {
        delete client_map_[id];
        client_map_[id] = new sockaddr_in(*from);
    }
    return;
}

void
HandleRequestAsBackup() {
    // Just pull the message from the queue
    // TODO(leners): Put into a map so that the backups can remove things that
    // have been processed by the primary, and on failover/election can process
    // them as new.
    diomedes_message* msg = (diomedes_message *) malloc(kMaxDiomedesMessage);
    struct sockaddr_in from;
    socklen_t alen = sizeof(from);
    int recv_bytes = recvfrom(request_socket_, msg, kMaxDiomedesMessage, 0, 
                              (struct sockaddr *) &from, &alen);
    CHECK(recv_bytes >= 0);
    diomedes_message* last_msg = request_map_[msg->payload.creq.client_id];
    free(last_msg);
    request_map_[msg->payload.creq.client_id] = msg;
    UpdateClientMap(msg->payload.creq.client_id, &from);
    return;
}


void
StartMonitoringBackups() {
    pthread_mutex_lock(&active_servers_lock_);
    std::list<ApplicationInstance*>::iterator it;
    for (it = active_servers_.begin(); it != active_servers_.end(); it++) {
        // Not interested in monitoring ourselves
        if (*it == primary_instance_) continue;
        Falcon::startMonitoring((*it)->target, diomedes_wrapper, -1);
    }
    pthread_mutex_unlock(&active_servers_lock_);
    return;
}
// Begin the primary loop.
void
RunAsPrimary() {
    // First, start watching everyone
    StartMonitoringBackups(); 
    struct epoll_event events[kNumEpollEvents];
    CHECK(mode_ == NEW_PRIMARY);
    for (;;) {
        int num_events = epoll_wait(my_epoll_, events, kNumEpollEvents, -1);
        for (int i = 0; i < num_events; i++) {
            // Only server left, no matter what reset to waiting for requests
            if (GetActiveServersCount() == 1) mode_ = WAITING_FOR_REQUESTS;
            int curr_fd = events[i].data.fd;
            if (curr_fd == failure_pipe_out_) {
                ProcessFailure();
            } else if (curr_fd == server_socket_) {
                ProcessBackupMessages();
            } else if (curr_fd == request_socket_) {
                if (mode_ == WAITING_FOR_REQUESTS) {
                    HandleRequestAsPrimary();
                }
            } else {
                LOG1("Some kind of prank message");
            }
        }
    }
}

// Nacks are sent in response to votes when the primary-elect doesn't know that everyone
// knows who it is.
void
SendNack(struct sockaddr_in* target) {
    static char buf[kMaxDiomedesMessage];
    diomedes_message* msg = (diomedes_message*) buf;
    msg->type = NACK;
    msg->version = 0;
    msg->length = htons(sizeof(diomedes_ack));
    memcpy(&(msg->payload.ack.acker), &self_, sizeof(self_));
    msg->payload.ack.req_id = curr_req_id_;
    int msg_size = sizeof(diomedes_ack) + kDiomedesHeaderSize;
    CHECK(msg_size == sendto(server_socket_, buf, msg_size, 0,
                             (struct sockaddr *) target,
                             sizeof(*target)));
    return;
}



// Creates a response buffer for the client and fills it with the response from
// that request.
bool
RunRequest(tcp_rpc_msg* request, uint32_t client_id) {
    tcp_rpc_msg* response = response_map_[client_id];
    if (response == NULL) {
        response = (tcp_rpc_msg *) malloc(sizeof(char) *kMaxDiomedesMessage);
        CHECK(response);
        response_map_[client_id] = response;
    } else if (request->msg.rm_xid == response->msg.rm_xid) {
        return false;
    } 
    memset(response, 0, kMaxDiomedesMessage);
    int32_t send_bytes = (sizeof(uint32_t) + (0xFFFFFF & ntohl(request->size)));
    // TODO(leners): someone might be putting something nasty in our state machine?
    CHECK(send_bytes == send(sm_sock_, request, send_bytes, 0));
    int32_t recv_bytes;
    recv_bytes = recv(sm_sock_, response, kMaxDiomedesMessage, 0);
    if (recv_bytes <= 0) {
        LOG("0x%08X", client_id);
    }
    CHECK(recv_bytes > 0);
    return true;
}

// Loop to demultiplex primary message.
void
ProcessPrimaryMessage(diomedes_message* msg) {
    CHECK(mode_ == BACKUP);
    uint32_t req_id = ntohl(msg->payload.preq.req_id);
    switch (msg->type) {
        case PRIMARY_REQUEST:
            // The primary is acting like one, it will deal with the nackers
            nacked_.clear();
            // Check that req_id is right
            if (req_id <= curr_req_id_) {
                SendVote();
                break; // Already processed this one, so resend our ack.
            }
            if (req_id - 1 == curr_req_id_) {
                // Ugh, I wish there were a way to avoid this copy.
                memcpy(last_primary_request_, msg, kMaxDiomedesMessage);
                // Doesn't use the response
                RunRequest((tcp_rpc_msg *)&(msg->payload.preq.request),
                           msg->payload.preq.client_id);
                // Know that the previous request was dealt with okay.
                last_valid_req_id_ = curr_req_id_;
                curr_req_id_++;
                // log?
                SendVote();
            } else {
                LOG("I'm behind! (or very ahead?)"
                     "req_id = %d curr_req = %d", req_id, curr_req_id_);
            }
            break;
        case JOIN:
            // The primary's confused because it didn't finish bootstrapping
            // if it's not the bootsrap primary, the primary is acting funny.
            if(bootstrap_primary_active_ &&
               msg->payload.join.broadcast) {
                LOG1("Bootstrapping a primary");
                SendJoin(&primary_addr_);
            }
            break;
        case NACK:
            // Primary's not ready to be primary
            if (!bootstrap_primary_active_) {
                LOG("Ignoring potential primary %s",
                    primary_instance_->
                    GetApplicationIdentifier()->GetAppString());
                primary_active_ = false;
                nacked_.insert(primary_instance_);
            }
            break;
        case PRIMARY_JOIN:
            CHECK(0); // Unimplemented
            // TODO(leners): Add to active_servers_
            if (req_id <= curr_req_id_) break; // Already processed this one
            if (req_id - 1 == curr_req_id_) {
                // add to active servers
                curr_req_id_++;
                SendVote();
                // Send a join to the n00b
            } else {
                LOG("I'm behind! (or very ahead?)"
                     "req_id = %d curr_req = %d", req_id, curr_req_id_);
            }
            break;
        default:
            LOG("unexpected message from primary: %c", msg->type);
            break;
    }
    return;
}

// Main loop to pull messages off of server socket for the backup (perhaps
// combine with ProcessPrimaryMessages into ProcessServerMessages?
void
ProcessPrimaryMessages() {
    static char buf[kMaxDiomedesMessage];
    diomedes_message* msg = (diomedes_message*) buf;
    int bytes_read;
    struct sockaddr_in from;
    socklen_t flen = sizeof(from);
    do {
        memset(msg, 0, kMaxDiomedesMessage);
        bytes_read = recvfrom(server_socket_, msg, kMaxDiomedesMessage,
                              MSG_DONTWAIT, (struct sockaddr *) &from, &flen);
        // Ignore anything not from the primary, or invalid.
        if (from.sin_addr.s_addr == primary_addr_.sin_addr.s_addr &&
            htons(msg->length) + kDiomedesHeaderSize == bytes_read) {
            ProcessPrimaryMessage(msg);
        } else {
            // Unless it's a newb joining, but that's a TODO
        }
    } while(bytes_read > 0);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    return;
}

// Backup servers send votes to whomever they think is the primary.
void
SendVote() {
    static char vote_buffer[kMaxDiomedesMessage];
    diomedes_message* msg = (diomedes_message *) vote_buffer;
    msg->type = 'V';
    msg->version = 0;
    memcpy(&(msg->payload.vote.voter), &self_, sizeof(self_));
    memcpy(&(msg->payload.vote.last_req),
           &(last_primary_request_->payload.preq),
           ntohs(last_primary_request_->length));
    uint32_t last_preq_id = ntohl(last_primary_request_->payload.preq.req_id);
    CHECK(last_preq_id == curr_req_id_);
    CHECK(last_primary_request_->type == 'R' || last_primary_request_->type == '\0');
    msg->length = htons(sizeof(diomedes_id) 
                  + ntohs(last_primary_request_->length));
    int send_size = ntohs(msg->length) + kDiomedesHeaderSize;
    CHECK(send_size == sendto(server_socket_, msg, send_size, 0,
                              (struct sockaddr *) &primary_addr_, 
                              sizeof(primary_addr_)));
    return;
}

// Wait for messages from the primary, or an acknowledged n00b
void
RunAsBackup() {
    LOG1("Starting run as backup");
    // Backups ignore messages from anyone but the primary.
    CHECK(mode_ == BACKUP);
    struct epoll_event events[kNumEpollEvents];
    SendVote();
    double last_vote = GetRealTime();
    while(primary_active_) {
        int num_events = epoll_wait(my_epoll_, events, kNumEpollEvents,
                                    backup_poll_interval_ms_);
        for (int i = 0; i < num_events; i++) {
            int curr_fd = events[i].data.fd;
            if (curr_fd == failure_pipe_out_) {
                ProcessFailure();
            } else if (curr_fd == server_socket_) {
                ProcessPrimaryMessages();
            } else if (curr_fd == request_socket_) {
                HandleRequestAsBackup();
            }
        }
        double now = GetRealTime();
        if (backup_poll_interval_ms_ < ((now - last_vote) * kSecondsToMilliseconds)) {
            SendVote();
            last_vote = now;
        }
    }
    return;
}

// What a simple run loop.
void
Run() {
    ElectPrimary();
    while (!CompareDiomedesIds(&self_, &primary_)) {
        // Backups only watch the primary. This might make leader election
        // nasty (i.e. take a while) if a whole bunch of backups fail, but
        // it might make sense to have the primary send out a "hey, check
        // this guy for failure" message.
        Falcon::startMonitoring(primary_instance_->target, 
                                diomedes_wrapper, -1);
        RunAsBackup();
        ElectPrimary();
    }
    mode_ = NEW_PRIMARY;
    RunAsPrimary();
    return;
}

// Send a join request to addr
// if addr is NULL, broadcast the join
void
SendJoin(struct sockaddr_in* addr) {
    static char join_buffer[kMaxDiomedesMessage];
    diomedes_message* msg = (diomedes_message *) join_buffer;
    msg->type = 'J';
    msg->version = 0;
    msg->length = htons(sizeof(diomedes_join));
    memcpy(&msg->payload.join.joiner, &self_, sizeof(self_));
    memcpy(&msg->payload.join.primary, &primary_, sizeof(primary_));
    msg->payload.join.broadcast = (addr) ? 0 : 1;
    int send_size = sizeof(diomedes_join) + kDiomedesHeaderSize;
    if (addr) {
        CHECK(send_size == sendto(server_socket_, join_buffer, send_size, 0,
                                  (struct sockaddr *) addr, sizeof(*addr)));
    } else {
        CHECK(send_size == sendto(server_socket_, join_buffer, send_size, 0,
                                  (struct sockaddr *) &bcast_addr_,
                                  sizeof(bcast_addr_)));
    }
    return;
}

// This is bootstrapping.
void
JoinDiomedesGroup() {
    mode_ = BOOTSTRAP;
    // Get potential join config options
    long start_size;
    Config::GetFromConfig<long>("start_size", &start_size, failures_ + 1);
    long join_timeout_ms;
    Config::GetFromConfig("join_timeout_ms", &join_timeout_ms, 
                         kDefaultJoinTimeoutMilliseconds);
    // Join ourselves
    Application* app_self = new Application(self_.handle, self_.domain,
                                           self_.hypervisor, kSwitchName);
    ApplicationInstance* self = new ApplicationInstance(app_self);
    AddToActiveServers(self);
    struct epoll_event events[kNumEpollEvents];
    double last = GetRealTime();
    SendJoin(NULL);
    // Wait until start_size servers are active
    while (GetActiveServersCount() < static_cast<size_t>(start_size)) {
        int num_events = epoll_wait(my_epoll_, events, kNumEpollEvents,
                                    join_timeout_ms);
        for (int i = 0; i < num_events; i++) {
            int curr_fd = events[i].data.fd;
            if (curr_fd == failure_pipe_out_) {
                // TODO(leners): Should just wipe out our state and start over?
                ProcessFailure();
            } else if (curr_fd == server_socket_) {
                // This will ignore our broadcast join
                ProcessJoins();
            } else {
                // Don't accept and respond to clients because we don't know
                // WTF is going on.
            }
        }
        double now = GetRealTime();
        if (now - last > (join_timeout_ms * kMillisecondsToSeconds)) {
            last = now;
            SendJoin(NULL);
            LOG("%d %ld", GetActiveServersCount(), start_size);
        }
    }
    LOG("%d %ld", GetActiveServersCount(), start_size);
    bootstrap_primary_active_ = true;
    mode_ = BACKUP;
    return;
}

// Adapted from cs.ui.ac.id
void
SetCloseOnExec(int fd) {
    int flags = fcntl(fd, F_GETFD, 0);
    CHECK(flags >= 0);
    flags |= FD_CLOEXEC;
    CHECK(0 == fcntl(fd, F_SETFD, flags));

}

void
Sabotuer(const char* failure) {
    if (!strcmp("lockspy", failure)) {
        pthread_mutex_lock(&spy_lock_);
        for(;;);
    } else if (!strcmp("segfault", failure)) {
        int *x = NULL;
        *x = 11;
    } else if (!strcmp("badloop", failure)) {
        pthread_mutex_lock(&failure_pipe_lock_);
        pipe_failed_ = true;
        pthread_mutex_unlock(&failure_pipe_lock_);
        for(;;);
    } else if (!strcmp("livelock", failure)) {
        for(;;) {
            // special hook in process failure for this addr
            SendFailed((ApplicationInstance*)(&self_));
        }
    }
}

// A bunch of initialization code. Super boring.
void
Init() {
    // For setsockopt
    int on = 1;
    // TODO(leners): potential options from config
    CHECK(Config::GetFromConfig("failures", &failures_));
    std::string handle;
    Config::GetFromConfig("handle", &handle, static_cast<std::string>("diomedes"));
    strncpy(handle_, handle.c_str(),32);
    SetSpy(&Spy, handle_);
    Config::GetFromConfig("backup_poll_interval_ms", &backup_poll_interval_ms_,
                          kDefaultBackupPollIntervalMilliseconds);
    Config::GetFromConfig("spy_multiplier", &spy_multiplier_, kDefaultSpyMultiplier);
    Config::GetFromConfig("spy_warmup", &spy_warmup_, kDefaultSpyWarmup);
    Config::GetFromConfig("poll_freq_ms", &poll_freq_ms_, kDefaultPollIntervalMilliseconds);
    
    // Setup ITC
    CHECK(0 == pipe(__my_pipe_));
    failure_pipe_out_ = __my_pipe_[0];
    failure_pipe_in_ = __my_pipe_[1];
    SetCloseOnExec(failure_pipe_in_);
    SetCloseOnExec(failure_pipe_out_);
    
    // Client socket
    struct sockaddr_in listen_addr;
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(kDiomedesClientPort);
    request_socket_ = socket(PF_INET, SOCK_DGRAM, 0);
    CHECK(0 == bind(request_socket_, (sockaddr *) &listen_addr, sizeof(listen_addr)));
    CHECK(request_socket_ >= 0);
    SetCloseOnExec(request_socket_);
    
    // Server socket
    server_socket_ = socket(PF_INET, SOCK_DGRAM, 0);
    CHECK(server_socket_ >= 0);
    listen_addr.sin_port = htons(kDiomedesServerPort);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    CHECK(0 == bind(server_socket_, (struct sockaddr *) &listen_addr,
                    sizeof(listen_addr)));
    CHECK(0 == setsockopt(server_socket_, SOL_SOCKET, SO_BROADCAST, &on, 
                          sizeof(on)));
    CHECK(0 == setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &on,
                          sizeof(on)));
    bcast_addr_.sin_family = AF_INET;
    bcast_addr_.sin_port = htons(kDiomedesServerPort);
    bcast_addr_.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    SetCloseOnExec(server_socket_);
    
    // Diomedes setup
    memset(&self_, '\0', sizeof(self_));
    strncpy(self_.handle, handle_, 32);
    CHECK(0 == gethostname(self_.domain, kNameLength));
    FILE* name_file = fopen("/mnt/gen/hypervisor_name", "r");
    CHECK(name_file);
    fread(self_.hypervisor, sizeof(char), kNameLength, name_file);
    CHECK(0 == ferror(name_file));
    fclose(name_file);
    // For filtering our UDP broadcast packets.
    struct addrinfo hints;
    struct addrinfo* ret;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    CHECK(0 == getaddrinfo(self_.domain, NULL, &hints, &ret));
    CHECK(ret->ai_family == AF_INET);
    memcpy(&my_addr_, ret->ai_addr, sizeof(my_addr_));
    freeaddrinfo(ret);
    memset(last_primary_request_, 0, kMaxDiomedesMessage);

    // Setup sabotuer
    SetSabotage(&Sabotuer);

    // Create epoll
    my_epoll_ = epoll_create(6); // arg is ignored
    CHECK(my_epoll_ >= 0);
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = server_socket_;
    CHECK(0 == epoll_ctl(my_epoll_, EPOLL_CTL_ADD, server_socket_, &ev));
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = failure_pipe_out_;
    CHECK(0 == epoll_ctl(my_epoll_, EPOLL_CTL_ADD, failure_pipe_out_, &ev));
    ev.data.fd = request_socket_;
    ev.events = EPOLLIN;
    CHECK(0 == epoll_ctl(my_epoll_, EPOLL_CTL_ADD, request_socket_, &ev));
    SetCloseOnExec(my_epoll_);
    
    // Create RSM 
    std::string rsm_path;
    CHECK(Config::GetFromConfig("rsm_path", &rsm_path));
    SetupStateMachine(rsm_path);

    // JoinDiomedesGroup
    LOG1("Joining a Diomedes group");
    JoinDiomedesGroup();
    LOG1("Joined a Diomedes group");
}

} // end Diomedes namespace

// What a simple main!
int
main (int argc, char** argv) {
    if (argc != 2) {
        LOG("usage: %s config_file", argv[0]);
        exit(1);
    }
    Config::LoadConfig(argv[1]);
    Diomedes::Init();
    Diomedes::Run();
    return 1;
}
