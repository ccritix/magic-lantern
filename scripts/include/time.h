/**
 * Magic Lantern scripting API
 * Time routines
 */

#ifndef _time_h_
#define _time_h_

/** Get current date/time */
struct time * get_time();

struct time
{
    int hour;
    int minute;
    int second;
    int year;
    int month;
    int day;
};

/** Sleep X.X seconds */
void sleep(float seconds);

/** Sleep X milliseconds */
void msleep(int milliseconds);

#endif
