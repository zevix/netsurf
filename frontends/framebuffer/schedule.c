/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Changed 2026-09-24 for netsurf_upy (GPLv2 section 2(a), a dated
 * notice of change).  This file gained:
 * framebuffer_schedule() adds to the back of the callback list, so
 * callbacks that come due together run in the order they were asked
 * for.  VitaSurf gates this on __vita__; this fork takes it ungated,
 * the bug being nothing to do with the Vita.
 *
 * Adapted from VitaSurf <https://github.com/Breezyslasher/VitaSurf>,
 * patches/0026-netsurf-timer-order.patch, by Breezyslasher.
 * GPL-2.0, same as NetSurf.
 *
 * The fork and the rest of its changes: netsurf_upy/ in the ubitron
 * repository; see netsurf_upy/README.md.
 */

#include <time.h>
#include <stdlib.h>

#include "utils/sys_time.h"
#include "utils/log.h"

#include "framebuffer/schedule.h"

/* linked list of scheduled callbacks */
static struct nscallback *schedule_list = NULL;

/**
 * scheduled callback.
 */
struct nscallback
{
        struct nscallback *next;
	struct timeval tv;
	void (*callback)(void *p);
	void *p;
};

/**
 * Unschedule a callback.
 *
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 *
 * All scheduled callbacks matching both callback and p are removed.
 */
static nserror schedule_remove(void (*callback)(void *p), void *p)
{
        struct nscallback *cur_nscb;
        struct nscallback *prev_nscb;
        struct nscallback *unlnk_nscb;

	/* check there is something on the list to remove */
        if (schedule_list == NULL) {
                return NSERROR_OK;
	}

	NSLOG(schedule, DEBUG, "removing %p, %p", callback, p);

        cur_nscb = schedule_list;
        prev_nscb = NULL;

        while (cur_nscb != NULL) {
                if ((cur_nscb->callback ==  callback) &&
                    (cur_nscb->p ==  p)) {
                        /* item to remove */

                        NSLOG(schedule, DEBUG,
			      "callback entry %p removing  %p(%p)",
                             cur_nscb, cur_nscb->callback, cur_nscb->p);

                        /* remove callback */
                        unlnk_nscb = cur_nscb;
                        cur_nscb = unlnk_nscb->next;

                        if (prev_nscb == NULL) {
                                schedule_list = cur_nscb;
                        } else {
                                prev_nscb->next = cur_nscb;
                        }
                        free (unlnk_nscb);
                } else {
                        /* move to next element */
                        prev_nscb = cur_nscb;
                        cur_nscb = prev_nscb->next;
                }
        }

	return NSERROR_OK;
}

/* exported function documented in framebuffer/schedule.h */
nserror framebuffer_schedule(int tival, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;
	struct timeval tv;
	nserror ret;

	/* ensure uniqueness of the callback and context */
	ret = schedule_remove(callback, p);
	if ((tival < 0) || (ret != NSERROR_OK)) {
		return ret;
	}

	NSLOG(schedule, DEBUG, "Adding %p(%p) in %d", callback, p, tival);

        tv.tv_sec = tival / 1000; /* miliseconds to seconds */
        tv.tv_usec = (tival % 1000) * 1000; /* remainder to microseconds */

	nscb = calloc(1, sizeof(struct nscallback));

	gettimeofday(&nscb->tv, NULL);
	timeradd(&nscb->tv, &tv, &nscb->tv);

	nscb->callback = callback;
	nscb->p = p;

	/*
	 * Add to the back of the list.  The runner walks it from the
	 * front, so adding at the front ran callbacks that came due
	 * together in the reverse of the order they were asked for.
	 * Schedulers depend on that order: posting three messages through
	 * a MessageChannel delivered them third, second, first.
	 * (VitaSurf; taken ungated -- the bug is not Vita-specific.)
	 */
	{
		struct nscallback *tail = schedule_list;

		nscb->next = NULL;
		if (tail == NULL) {
			schedule_list = nscb;
		} else {
			while (tail->next != NULL) {
				tail = tail->next;
			}
			tail->next = nscb;
		}
	}

	return NSERROR_OK;
}

/* exported function documented in framebuffer/schedule.h */
int schedule_run(void)
{
	struct timeval tv;
	struct timeval nexttime;
	struct timeval rettime;
        struct nscallback *cur_nscb;
        struct nscallback *prev_nscb;
        struct nscallback *unlnk_nscb;

        if (schedule_list == NULL)
                return -1;

	/* reset enumeration to the start of the list */
        cur_nscb = schedule_list;
        prev_nscb = NULL;
	nexttime = cur_nscb->tv;

	gettimeofday(&tv, NULL);

        while (cur_nscb != NULL) {
                if (timercmp(&tv, &cur_nscb->tv, >)) {
                        /* scheduled time */

                        /* remove callback */
                        unlnk_nscb = cur_nscb;

                        if (prev_nscb == NULL) {
                                schedule_list = unlnk_nscb->next;
                        } else {
                                prev_nscb->next = unlnk_nscb->next;
                        }

                        unlnk_nscb->callback(unlnk_nscb->p);

                        free(unlnk_nscb);

                        /* need to deal with callback modifying the list. */
			if (schedule_list == NULL)
				return -1; /* no more callbacks scheduled */
			
                        /* reset enumeration to the start of the list */
                        cur_nscb = schedule_list;
                        prev_nscb = NULL;
			nexttime = cur_nscb->tv;
                } else {
			/* if the time to the event is sooner than the
			 * currently recorded soonest event record it 
			 */
			if (timercmp(&nexttime, &cur_nscb->tv, >)) {
				nexttime = cur_nscb->tv;
			}
                        /* move to next element */
                        prev_nscb = cur_nscb;
                        cur_nscb = prev_nscb->next;
                }
        }

	/* make rettime relative to now */
	timersub(&nexttime, &tv, &rettime);

	NSLOG(schedule, DEBUG,
	      "returning time to next event as %ldms",
	      (rettime.tv_sec * 1000) + (rettime.tv_usec / 1000)); 

	/* return next event time in milliseconds (24days max wait) */
        return (rettime.tv_sec * 1000) + (rettime.tv_usec / 1000);
}

void list_schedule(void)
{
	struct timeval tv;
        struct nscallback *cur_nscb;

	gettimeofday(&tv, NULL);

        NSLOG(netsurf, INFO, "schedule list at %ld:%ld", tv.tv_sec,
              tv.tv_usec);

        cur_nscb = schedule_list;

        while (cur_nscb != NULL) {
                NSLOG(netsurf, INFO, "Schedule %p at %ld:%ld", cur_nscb,
                      cur_nscb->tv.tv_sec, cur_nscb->tv.tv_usec);
                cur_nscb = cur_nscb->next;
        }
}


/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
