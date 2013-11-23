#ifndef _NTFA_HANDLERD_PROCESS_H
#define _NTFA_HANDLERD_PROCESS_H

#include <stdint.h>

#include <list>
#include <string>

#include "common/common.h"
#include "common/messages.h"

namespace Process {

class Process {
    public:
        Process(const char *handle, pid_t pid, int socket);
        Process(const char *handle);
        bool        IsValid();
        void        ReplyUp();
        void        ReplyDown();
        void        ReplyLongDead(int fd);
        void        ReplyUnconfirmed();
        void        ReplyUnknown(int fd);
        bool        Probe();
        bool        AddToProbers(int pfd_fd, struct handlerd_query* query);
        bool        Assassinate();
        bool        Confirm();
        bool        HandleUnixResponse();
        uint32_t    GetConfirmWait() const;
        uint32_t    GetQueryNo() const;
        pid_t       GetPID() const;
        void        Update(pid_t pid, int socket);
    private:
        std::string         handle_;
        pid_t               pid_;
        int                 socket_;
        uint32_t            confirm_wait_ms_;
        std::list<int>      waiting_pfds_;
        uint32_t            queryno_;
	bool		    registered_;
        bool                beakless_;
	uint32_t	    generation_;

        void        ReplyGroup(app_state_t state);
        void        ReplySolo(app_state_t state, int fd);
        void        SetConfirmWait(uint32_t confirm_wait_ms);
        void        CloseSocket();
}; // class process

bool RegisterProcess(const std::string& handle, pid_t, int socket);
Process *GetProcess(std::string& handle);
void InitGenerations();

} // namespace process
#endif // _NTFA_HANDLERD_PROCESS_H
