#include "router/util.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <string>

#include "bridge/libbridge/libbridge.h"
#include "common/common.h"
#include "common/messages.h"

pthread_mutex_t get_port_from_ip_ = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t keep_alive_map_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t generation_map_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t agent_lock_map_lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t agent_cond_map_lock_ = PTHREAD_MUTEX_INITIALIZER;
// jbl: Make sure things are spelled correctly: lack -> lock
pthread_mutex_t libvirt_port_map_lock_ = PTHREAD_MUTEX_INITIALIZER;

// jbl: Keep our state in maps.
std::map<std::string, double> keep_alive_map_; 
std::map<std::string, uint32_t> generation_map_;
std::map<std::string, pthread_mutex_t*> agent_lock_map_;
std::map<std::string, pthread_cond_t*>  agent_cond_map_;
std::map<std::string, int> libvirtd_port_map_;
std::map<std::string, bool> status_map_;
/*
 * Note, the underscore_functions in this file are hacks, but they are
 * how we get the port number for shutting down via the proc file.
 * use GetPortFromIP() to get the port from an IP value.
 *
 */
#define LINE_BUF_SIZE 512
    static bool
get_mac_from_ip(const char *ip, uint8_t* mac) 
{
    char *line_buf = (char *) malloc(LINE_BUF_SIZE);
    CHECK(line_buf);
    FILE *arp_table = fopen("/proc/net/arp", "r");
    CHECK(arp_table);
    for (;;) {
        if (NULL == fgets(line_buf, LINE_BUF_SIZE, arp_table)) {
            free(line_buf);
            fclose(arp_table);
            return false;
        }
        char **sep_start;
        char *sep_string = line_buf;
        sep_start = &sep_string;
        char *arp_ip = strsep(sep_start, " ");
        char *arp_mac;
        if(!strcmp(arp_ip, ip)) {
            while(sep_start != NULL) {
                arp_mac = strsep(sep_start, " ");
                // Length of the MAC string
                if (strlen(arp_mac) == 17) {
                    unsigned int tmp[6];
                    sscanf(arp_mac, "%2x:%2x:%2x:%2x:%2x:%2x",
                            &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
                    for(int i = 0; i < 6; i++) {
                        mac[i] = static_cast<uint8_t>(tmp[i]);
                    }
                    free(line_buf);
                    fclose(arp_table);
                    return true;
                }
            }
        }
    }
    CHECK(0); // Shouldn't ever reach here...
    fclose(arp_table);
    return false;
}


// Code for this function largely comes from bridge/brctl.c
static int
get_port_from_mac(uint8_t mac[6]) {
    const char *brname = "br0";
#define CHUNK 128
    int i, j, n;
    struct fdb_entry *fdb = NULL;
    int offset = 0;

    for(;;) {
        fdb = (struct fdb_entry *) realloc(fdb, (offset + CHUNK) * sizeof(struct fdb_entry));
        if (!fdb) {
            fprintf(stderr, "Out of memory\n");
            free(fdb);
            return -1;
        }

        n = br_read_fdb(brname, fdb+offset, offset, CHUNK);
        if (n == 0)
            break;

        if (n < 0) {
            fprintf(stderr, "read of forward table failed: %s\n",
                    strerror(errno));
            return -1;
        }
        offset += n;
    }
    for (i = 0; i < offset; i++) {
        const struct fdb_entry *f = fdb + i;
        bool mac_match = true;
        for (j = 0; j < 6; j++) {
            if (mac[j] != f->mac_addr[j]) {
                mac_match = false;
                break;
            }
        }
        if (mac_match) {
            // /proc/bridge/port starts at 0
            // fdb starts at 2
            return f->port_no - 1;
        }
    }
    return -1;
}

int
GetPortFromIP(const char *ip) {
    pthread_mutex_lock(&get_port_from_ip_);
    int ret = -1;
    uint8_t mac[6];
    if (get_mac_from_ip(ip, mac)) {
        ret = get_port_from_mac(mac);
    }
    pthread_mutex_unlock(&get_port_from_ip_);
    return ret;
}

void
Daemonize(void) {
    pid_t pid, sid;
    if (getppid() == 1) return;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }   
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }   

    umask(0);
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) {
        exit(EXIT_FAILURE);
    }

    freopen ("/dev/null", "r", stdin);
    freopen ("/jffs/assassin.log", "w", stdout);
    freopen ("/jffs/assassin.log", "w", stderr);
    return;

}

// Keep Alive Map
void
UpdateLastHeartbeat(char* hostname) {
    pthread_mutex_lock(&keep_alive_map_lock_);
    keep_alive_map_[hostname] = GetRealTime();
    pthread_mutex_unlock(&keep_alive_map_lock_);
    return;
}

double
GetLastHeartbeat(const char* hostname) {
    double ret;
    pthread_mutex_lock(&keep_alive_map_lock_);
    ret = keep_alive_map_[hostname];
    pthread_mutex_unlock(&keep_alive_map_lock_);
    return ret;
}


// Generation map
uint32_t
IncrementGeneration(char *hostname) {
    pthread_mutex_lock(&generation_map_lock_);
    generation_map_[hostname]++;
    uint32_t ret = generation_map_[hostname];
    pthread_mutex_unlock(&generation_map_lock_);
    return ret;
}

uint32_t
GetGeneration(char* hostname) {
    uint32_t ret;
    pthread_mutex_lock(&generation_map_lock_);
    ret = generation_map_[hostname];
    pthread_mutex_unlock(&generation_map_lock_);
    return ret;
}

void
SetAgentStatus(char* hostname, bool status) {
    pthread_mutex_t* agent_lock = GetAgentLock(hostname);
    CHECK(agent_lock != NULL);
    pthread_cond_t* agent_cond = GetAgentCond(hostname);
    CHECK(agent_cond != NULL);
    pthread_mutex_lock(agent_lock);
    status_map_[hostname] = status;
    pthread_mutex_unlock(agent_lock);
    return;
}

// Should be protected by lock (forgive me Mike D.)
bool
IsAgentUp(char* hostname) {
    return status_map_[hostname];
}

// Lock Map
pthread_mutex_t*
GetAgentLock(char* hostname) {
    pthread_mutex_lock(&agent_lock_map_lock_);
    pthread_mutex_t* ret = agent_lock_map_[hostname];
    if (ret == NULL) {
        ret = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(ret, NULL);
        agent_lock_map_[hostname] = ret;
    }
    pthread_mutex_unlock(&agent_lock_map_lock_);
    return ret;
}

// Cond Map
pthread_cond_t*
GetAgentCond(char* hostname) {
    pthread_mutex_lock(&agent_cond_map_lock_);
    pthread_cond_t* ret = agent_cond_map_[hostname];
    if (ret == NULL) {
        ret = (pthread_cond_t*) malloc(sizeof(pthread_cond_t));
        pthread_cond_init(ret, NULL);
        agent_cond_map_[hostname] = ret;
    }
    pthread_mutex_unlock(&agent_cond_map_lock_);
    return ret;
}

// Port Map
int
GetPort(char* hostname) {
    pthread_mutex_lock(&libvirt_port_map_lock_);
    int ret = libvirtd_port_map_[hostname];
    pthread_mutex_unlock(&libvirt_port_map_lock_);
    return ret;
}

void
SetPort(char* hostname, int port) {
    pthread_mutex_lock(&libvirt_port_map_lock_);
    libvirtd_port_map_[hostname] = port;
    pthread_mutex_unlock(&libvirt_port_map_lock_);
}

// Broadcast Agent
void
BroadcastAgentResponse(char* hostname) {
    pthread_mutex_t* agent_lock = GetAgentLock(hostname);
    CHECK(agent_lock != NULL);
    pthread_cond_t* agent_cond = GetAgentCond(hostname);
    CHECK(agent_cond != NULL);
    // better to hold the lock...
    SetAgentStatus(hostname, false);
    pthread_mutex_lock(agent_lock);
    pthread_cond_broadcast(agent_cond);
    pthread_mutex_unlock(agent_lock);
    return;
}

enum libvirt_state
GetLibvirtState(char* hostname, router_query q) {
    enum libvirt_state ret = ACTIVE;
    uint32_t gen_num = GetGeneration(hostname);
    uint32_t query_gen = ntohl(q.generation_num);
    uint32_t beakmask = 1 << (sizeof(uint32_t) * 8 - 1); 
    query_gen &= ~beakmask;

    LOG("%08X %08X", query_gen, gen_num);
    if (NULL == GetAgentCond(hostname) || NULL == GetAgentLock(hostname)) {
        LOG1("invalid");
        ret = INVALID;
    } else if (gen_num > query_gen) {
        LOG1("long dead");
        ret = LONG_DEAD;
    } else if (gen_num < query_gen) {
        LOG1("future");
        ret = FUTURE;
    }
    return ret;
}

enum libvirt_state
ClientWaitResponse(char *hostname, uint32_t generation) {
    enum libvirt_state ret = INVALID;
    pthread_mutex_t* lock = GetAgentLock(hostname);
    pthread_cond_t* cond = GetAgentCond(hostname);
    pthread_mutex_lock(lock);
    int rc = 0;
    while(rc == 0 && IsAgentUp(hostname)) { // Check the state
        rc = pthread_cond_wait(cond, lock);
    }
    if (!IsAgentUp(hostname)) ret = TIMEDOUT;
    pthread_mutex_unlock(lock);
    return ret; 
}

static FILE *generation_log_ = NULL;
const static size_t kBufferSize = 128;

void
InitGenerationLog() {
    CHECK(!generation_log_);
    generation_log_ = fopen("/jffs/ntfa.gen", "a+");
    CHECK(generation_log_);
    char hostname[kBufferSize];
    while (NULL != fgets(hostname, kBufferSize, generation_log_)) {
        size_t new_line = strlen(hostname) - 1;
        hostname[new_line] = '\0';
        generation_map_[hostname]++;
    }
    return;
}

void
LogGeneration(const std::string& hostname) {
    const char *buf = hostname.c_str();

    CHECK(0 <= fprintf(generation_log_, "%s\n", buf));
    CHECK(0 == fflush(generation_log_));
    CHECK(0 == fsync(fileno(generation_log_)));
    return;
}
