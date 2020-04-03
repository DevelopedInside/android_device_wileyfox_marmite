/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <pthread.h>
#include <dlfcn.h>

#include <errno.h>
#include <time.h>

// From bionic_constants.h
#define NS_PER_S 1000000000

int pthread_cond_timedwait(pthread_cond_t *cond_interface, pthread_mutex_t * mutex,
                           const timespec *abstime) {
  // HAX: Timespec checks are failing if tv_nsec >= 1000000000L (aka 1 sec).
  // Increment tv_sec while subtracting NS_PER_S from tv_nsec till tv_nsec is
  // < 1000000000L such that tv_nsec doesn't overflow and passes check_timespec().
  while (abstime->tv_nsec >= NS_PER_S) {
    const_cast<timespec*>(abstime)->tv_nsec -= NS_PER_S;
    const_cast<timespec*>(abstime)->tv_sec++;
  }

  int (*real_pthread_cond_timedwait)(pthread_cond_t*, pthread_mutex_t*, const timespec*);
  *(void **)&real_pthread_cond_timedwait = dlsym(RTLD_NEXT, "pthread_cond_timedwait");
  return real_pthread_cond_timedwait(cond_interface, mutex, abstime);
}
