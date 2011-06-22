/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *      * The names of its contributors may not be used to endorse or
 *  promote products derived from this software without specific prior
 *  written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <config.h>

#include <libgearman/gearman.h>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cstdio>
#include <string>
#include <iostream>
#include <tests/workers.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

gearman_return_t echo_or_react_worker_v2(gearman_job_st *job, void *)
{
  const void *workload= gearman_job_workload(job);
  size_t result_size= gearman_job_workload_size(job);

  if (workload == NULL or result_size == 0)
  {
    assert(workload == NULL and result_size == 0);
    return GEARMAN_SUCCESS;
  }
  else if (result_size == gearman_literal_param_size("fail") and (not memcmp(workload, gearman_literal_param("fail"))))
  {
    return GEARMAN_FATAL;
  }
  else if (result_size == gearman_literal_param_size("exception") and (not memcmp(workload, gearman_literal_param("exception"))))
  {
    gearman_return_t rc= gearman_job_send_exception(job, gearman_literal_param("test exception"));
    if (gearman_failed(rc))
    {
      return GEARMAN_ERROR;
    }
  }
  else if (result_size == gearman_literal_param_size("warning") and (not memcmp(workload, gearman_literal_param("warning"))))
  {
    gearman_return_t rc= gearman_job_send_warning(job, gearman_literal_param("test warning"));
    if (gearman_failed(rc))
    {
      return GEARMAN_ERROR;
    }
  }

  if (gearman_failed(gearman_job_send_data(job, workload, result_size)))
  {
    return GEARMAN_ERROR;
  }

  return GEARMAN_SUCCESS;
}

gearman_return_t echo_or_react_chunk_worker_v2(gearman_job_st *job, void *)
{
  const char *workload= (const char *)gearman_job_workload(job);
  size_t workload_size= gearman_job_workload_size(job);

  bool fail= false;
  if (workload_size == gearman_literal_param_size("fail") and (not memcmp(workload, gearman_literal_param("fail"))))
  {
    fail= true;
  }
  else if (workload_size == gearman_literal_param_size("exception") and (not memcmp(workload, gearman_literal_param("exception"))))
  {
    if (gearman_failed(gearman_job_send_exception(job, gearman_literal_param("test exception"))))
    {
      return GEARMAN_ERROR;
    }
  }
  else if (workload_size == gearman_literal_param_size("warning") and (not memcmp(workload, gearman_literal_param("warning"))))
  {
    if (gearman_failed(gearman_job_send_warning(job, gearman_literal_param("test warning"))))
    {
      return GEARMAN_ERROR;
    }
  }

  for (size_t x= 0; x < workload_size; x++)
  {
    // Chunk
    {
      if (gearman_failed(gearman_job_send_data(job, &workload[x], 1)))
      {
        return GEARMAN_ERROR;
      }
    }

    // report status
    {
      if (gearman_failed(gearman_job_send_status(job, (uint32_t)x, (uint32_t)workload_size)))
      {
        return GEARMAN_ERROR;
      }

      if (fail)
      {
        return GEARMAN_FATAL;
      }
    }
  }

  return GEARMAN_SUCCESS;
}

// payload is unique value
gearman_return_t unique_worker_v2(gearman_job_st *job, void *)
{
  const char *workload= static_cast<const char *>(gearman_job_workload(job));

  assert(job->assigned.command == GEARMAN_COMMAND_JOB_ASSIGN_UNIQ);
  assert(gearman_job_unique(job));
  assert(strlen(gearman_job_unique(job)));
  assert(gearman_job_workload_size(job));
  assert(strlen(gearman_job_unique(job)) == gearman_job_workload_size(job));
  assert(not memcmp(workload, gearman_job_unique(job), gearman_job_workload_size(job)));
  if (gearman_job_workload_size(job) == strlen(gearman_job_unique(job)))
  {
    if (not memcmp(workload, gearman_job_unique(job), gearman_job_workload_size(job)))
    {
      if (gearman_failed(gearman_job_send_data(job, workload, gearman_job_workload_size(job))))
      {
        return GEARMAN_ERROR;
      }

      return GEARMAN_SUCCESS;
    }
  }

  return GEARMAN_FATAL;
}

static pthread_mutex_t increment_reset_worker_mutex= PTHREAD_MUTEX_INITIALIZER;

gearman_return_t increment_reset_worker_v2(gearman_job_st *job, void *)
{
  static long counter= 0;
  long change= 0;
  const char *workload= (const char*)gearman_job_workload(job);

  if (gearman_job_workload_size(job) == gearman_literal_param_size("reset") and (not memcmp(workload, gearman_literal_param("reset"))))
  {
    pthread_mutex_lock(&increment_reset_worker_mutex);
    counter= 0;
    pthread_mutex_unlock(&increment_reset_worker_mutex);

    return GEARMAN_SUCCESS;
  }
  else if (workload and gearman_job_workload_size(job))
  {
    if (gearman_job_workload_size(job) > GEARMAN_MAXIMUM_INTEGER_DISPLAY_LENGTH)
    {
      return GEARMAN_FATAL;
    }

    char temp[GEARMAN_MAXIMUM_INTEGER_DISPLAY_LENGTH +1];
    memcpy(temp, workload, gearman_job_workload_size(job));
    temp[gearman_job_workload_size(job)]= 0;
    change= strtol(temp, (char **)NULL, 10);
    if (change ==  LONG_MIN or change == LONG_MAX or ( change == 0 and errno < 0))
    {
      gearman_job_send_warning(job, gearman_literal_param("strtol() failed"));
      return GEARMAN_FATAL;
    }
  }

  {
    pthread_mutex_lock(&increment_reset_worker_mutex);
    counter= counter +change;

    char result[GEARMAN_MAXIMUM_INTEGER_DISPLAY_LENGTH +1];
    size_t result_size= size_t(snprintf(result, sizeof(result), "%ld", counter));
    if (gearman_failed(gearman_job_send_data(job, result, result_size)))
    {
      return GEARMAN_FATAL;
    }

    pthread_mutex_unlock(&increment_reset_worker_mutex);
  }

  return GEARMAN_SUCCESS;
}
