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

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "common.h"

bool working = true;

void
sig_handler(int sig) {
  if (sig == SIGALRM) {
      exit(EXIT_SUCCESS);
  }
  working = false;
  return;
}

void
daemonize() {
  pid_t pid, sid;
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  } else if (pid > 0) {
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
  freopen("/dev/null", "r", stdin);
  freopen("/dev/null", "w", stdout);
  freopen("/dev/null", "w", stderr);
  return;
}

int
main (int argc, char** argv) {
  assert(argc > 1);

  Config::LoadConfig(argv[1]);

  int num;
  double target_cpu;
  int duration;

  CHECK(Config::GetFromConfig("num_processes", &num));
  CHECK(Config::GetFromConfig("target_cpu", &target_cpu));
  CHECK(Config::GetFromConfig("duration_s", &duration));

  for (int i = 1; i < num && 0 < fork(); ++i);

  daemonize();
  signal(SIGPROF, sig_handler);
  signal(SIGALRM, sig_handler);
  long clock_tick = sysconf(_SC_CLK_TCK);
  double sec_per_tick = 1.0 / clock_tick;

  // In ticks per second.
  int sleep_time = clock_tick * (1.0 - target_cpu);
  int work_time = clock_tick * target_cpu;

  sleep_time *= sec_per_tick * 1000 * 1000;
  itimerval timerv;
  timerv.it_interval.tv_sec = 0;
  timerv.it_interval.tv_usec = 0;
  timerv.it_value.tv_sec = duration;
  timerv.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &timerv, NULL);

  if (target_cpu > 0.0 && target_cpu < 1.0) {
      timerv.it_interval.tv_sec = 0;
      timerv.it_interval.tv_usec = 1000 * 1000 * sec_per_tick * work_time;
      timerv.it_value = timerv.it_interval;
      setitimer(ITIMER_PROF, &timerv, NULL);
      for (;;) {
        if (working) {
          sqrt(random());
        } else {
          usleep(sleep_time);
          working = true;
        }
      }
  } else if (target_cpu <= 0.0) {
      return 0;
  } else {
      for (;;) {
          sqrt(random());
      }
  }
  return 0;
}
