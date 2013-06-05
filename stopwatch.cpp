#include "stopwatch.h"

#include <time.h>

Stopwatch::Stopwatch() : m_start(0.0), m_stop(0.0), running(false)
{
}

int Stopwatch::start()
{
  struct timespec start;
  int ret = clock_gettime(CLOCK_MONOTONIC, &start);
  if (ret == 0) {
    m_start = start.tv_sec + (start.tv_nsec * 1.0e-9);
    running = true;
  }
  return ret;
}

int Stopwatch::stop()
{
  struct timespec stop;
  int ret = clock_gettime(CLOCK_MONOTONIC, &stop);
  if (ret == 0) {
    m_stop = stop.tv_sec + (stop.tv_nsec * 1.0e-9);
    running = false;
  }
  return ret;
}

double Stopwatch::elapsed_time()
{
  if (is_running()) {
    stop();
  }
  double elapsed = m_stop - m_start;
  return elapsed;
}

void Stopwatch::reset()
{
  m_start = m_stop = 0.0;
  running = false;
}

bool Stopwatch::is_running()
{
  return running;
}

