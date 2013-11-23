#ifndef _NTFA_ROUTER_UTIL_H_
#define _NTFA_ROUTER_UTIL_H_

#include <pthread.h>

#include <map>
#include <string>

#include "common/messages.h"

enum libvirt_state {
    INVALID,
    FUTURE,
    TIMEDOUT,
    LONG_DEAD,
    ACTIVE
};

void Daemonize(void); 
int GetPortFromIP(const char* ip);
pthread_mutex_t *GetAgentLock(char* hostname);
pthread_cond_t *GetAgentCond(char* hostname);
int GetPort(char* hostname);
void SetPort(char* hostname, int port);
enum libvirt_state GetLibvirtState(char *hostname, router_query q);
enum libvirt_state ClientWaitResponse(char *hostname, uint32_t timeout_ms);
void SetAgentStatus(char* hostname, bool status);

uint32_t IncrementGeneration(char* hostname);
uint32_t GetGeneration(char* hostname);
void UpdateLastHeartbeat(char* hostname);
void BroadcastAgentResponse(char* hostname);

void InitGenerationLog();
void LogGeneration(const std::string& hostname);
#endif // _NTFA_ROUTER_UTIL_H_
