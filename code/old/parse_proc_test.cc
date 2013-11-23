#include "handlerd/parse_proc.h"

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* This is a unit test for the /proc/ processing features. This library was
 * written because other alternatives were too poorly documented.
 */

const char *kRealHandle = "ntfa_parseproc";
const char *kBogusHandle = "ntfa_bogus";
const char *kReallyBogusHandle = "bogus";

int
main() {
    assert(0 == prctl(PR_SET_NAME, kRealHandle, NULL, NULL));
    assert(NULL == GetMatchingProcesses(kReallyBogusHandle));
    assert(NULL == GetMatchingProcesses(kBogusHandle));
    std::list<proc_info_s>* my_list = GetMatchingProcesses(kRealHandle);
    assert(NULL != my_list);
    // Check that the first entry is this program.
    proc_info_s my_info = my_list->front();
    assert(my_info.pid == getpid());
    // Check that there is only one entry on the list.
    my_list->pop_front();
    assert(my_list->size() == 0);
    delete my_list;
    // Check that memory is freed on lazy delete.
    my_list = GetMatchingProcesses(kRealHandle);
    delete my_list;
    proc_info_p my_info_p = GetFirstMatchingProcess(kRealHandle);
    assert(my_info_p->pid == getpid());
    // Check handle propagation
    if ( fork() != 0 ) {
        my_list = GetMatchingProcesses(kRealHandle);
        assert(my_list->size() == 2);
        // Kill the child
        std::list<proc_info_s>::iterator i;
        for(i=my_list->begin(); i != my_list->end(); ++i) {
            if (i->pid != my_info_p->pid) {
                // We kill it
                assert(0 == kill(i->pid, SIGKILL));
                int status;
                assert(i->pid == waitpid(i->pid, &status, NULL));
                // Make sure it died because we killed it.
                assert(WIFSIGNALED(status));
            }
        }
        delete my_list;
        free(my_info_p);

    } else {
        for (;;);
    }
    return 0;
}
