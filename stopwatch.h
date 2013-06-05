#ifndef _H_STOPWATCH
#define _H_STOPWATCH

class Stopwatch
{
public:
  Stopwatch();
  int start();
  int stop();
  void reset();
  double elapsed_time();
  bool is_running();
private:
  double m_start, m_stop;
  bool running;
};

#endif /* _H_STOPWATCH */
