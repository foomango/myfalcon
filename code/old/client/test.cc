#include "client.h"

#include <stdio.h>

#include <string>
#include <vector>

void
cb (const std::vector<std::string>& h, uint32_t s1, uint32_t s2) {
    printf("Yeehaw, its dead Jim!\n");
    printf("%d\t%d\n", s1, s2);
    fflush(stdout);
    return;
}

int
main () {
    FalconClient *c = FalconClient::GetInstance();
    std::vector<std::string> v;
    v.push_back("ntfa_spin_up");
    v.push_back("raz");
    c->StartMonitoring(v, true, &cb);
    for(;;);
    return 0;
}
