#ifndef POSIX_TIME
#define POSIX_TIME

#include <boost/date_time/posix_time/posix_time.hpp>


time_t to_time_t(boost::posix_time::ptime aPtime)
{
  boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  boost::posix_time::time_duration diff(aPtime - epoch);
  return diff.total_seconds();
}

boost::posix_time::ptime from_ustime_t(double secsEpoch1970)
{
  time_t sec(static_cast<time_t>(secsEpoch1970));
  long usec(static_cast<long>(1000000 * (secsEpoch1970 - sec)));
  return boost::posix_time::from_time_t(sec) + boost::posix_time::microseconds(usec);
}

#endif
