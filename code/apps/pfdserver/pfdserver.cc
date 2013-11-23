/* 
 * Copyright (c) 2011 Joshua B. Leners, Wei-Lun Hung (University of Texas at Austin).
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

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>        
#include <sys/un.h>
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "messages.h"
#include "client/client.h"

using namespace Falcon;

uint16_t pfdserver_port_ = 9090;
const int32_t kDefaultE2E = 60 * 5;
bool lethal;
int32_t e2etimeout;

void
pfd_server_cb(const LayerIdList& id_list, void* data, uint32_t falcon_st,
            uint32_t layer_st) {
  int* client_fd_ptr = (reinterpret_cast<int*>(data));
  pfd_server_reply reply;
  reply.state = ((uint64_t)falcon_st << 32) + layer_st;
  LogCallbackData(id_list, falcon_st, layer_st);
  if(sizeof(reply) != send(*client_fd_ptr, &reply, sizeof(reply), 0)) {
      LOG1("Error pushing notification");
  }
  LOG("replied to client, closing connection");
  close(*client_fd_ptr);
  delete client_fd_ptr;
}

void*
ClientHandler(void* arg) {
  int* client_fd_ptr = (reinterpret_cast<int*>(arg));
  //receive query message
  struct pfd_server_handshake handshake;
  memset(&handshake, 0, sizeof(handshake));

  int recv_size = 0;
  int curr_recv;
  do {
    curr_recv = recv(*client_fd_ptr, &handshake, sizeof(handshake), 0);
    recv_size += curr_recv;
  } while (recv_size != sizeof(handshake) && curr_recv > 0);
  if (recv_size != sizeof(handshake)) {
    LOG("Bad handshake recv_size %d handshake %d last_recv %d", recv_size,
        (int)sizeof(handshake), curr_recv);
    close(*client_fd_ptr); 
    return NULL;
  }

  LayerIdList l;
  l.push_back(handshake.app_handle);
  l.push_back(handshake.host_name);
  l.push_back(handshake.vmm_name);
  l.push_back(handshake.switch_name);
  sleep(3);

  falcon_target* t = init(l, lethal, client_fd_ptr);
  if (!t) {
      pfd_server_cb(l, client_fd_ptr, 7, 0);
      return NULL;
  }
  startMonitoring(t, pfd_server_cb, e2etimeout);
  LOG("starting to monitor %s:%s:%s:%s", l[0].c_str(), l[1].c_str(),
      l[2].c_str(), l[3].c_str());
  return NULL;
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
  freopen ("/tmp/pfdserver.log", "w", stdout);
  freopen ("/tmp/pfdserver.log", "w", stderr);
  return;

}
int
main (int argc, char **argv) {
  int unix_socket = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in pfd_addr;
  memset(&pfd_addr, 0, sizeof(pfd_addr));
  pfd_addr.sin_family = AF_INET;
  pfd_addr.sin_port = htons(pfdserver_port_);

  Daemonize();
  if (argc == 2) {
    Config::LoadConfig(argv[1]);
  }
  Config::GetFromConfig("e2etimeout", &e2etimeout, kDefaultE2E);
  Config::GetFromConfig("lethal", &lethal, false);

  CHECK(0 == bind(unix_socket, (struct sockaddr *) &pfd_addr, sizeof(pfd_addr)));
  CHECK(0 == listen(unix_socket, 10));

  for(;;) {
    struct sockaddr tmp_addr;
    socklen_t len = sizeof(tmp_addr);
    int accept_fd = accept(unix_socket, &tmp_addr, &len);
    LOG1("got a new monitor connection");
    pthread_t new_client;
    pthread_create(&new_client, NULL, ClientHandler, new int(accept_fd));
    pthread_detach(new_client);
  }
  return 0;
}
