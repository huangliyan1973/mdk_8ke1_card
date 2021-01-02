#ifndef LFFIFO_H
#define LFFIFO_H

#include "lwip/sys.h"

struct lffifo{
	int size;
	int start;
	int end;
	u8_t buf[1];
};

struct lffifo *lffifo_alloc(int size);
void lffifo_free(struct lffifo *fifo);

/* Put and get data to/from the fifo.

   The idea is that one thread may put and one other thread may get without
   synchronisation, and still get correct behaviour.

   Put may fail if fifo is full; likewise get may fail if fifo is empty.
*/

/* Puts a frame, returns zero if ok, non-zero if full. */
int lffifo_put(struct lffifo *fifo, u8_t *data, u16_t size);

/* Gets a single frame, returns frame size or zero if fifo is empty.
   Returns negative number if caller buffer is too small. */
int lffifo_get(struct lffifo *fifo, u8_t *buf, u16_t bufsize);

#endif
