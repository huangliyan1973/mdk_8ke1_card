#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "lffifo.h"

/* Escape bytes used to mark unused space, for framing, etc. Chosen to be
   fairly infrequent in typical data, to save on escaping overhead. */
#define BYT_EMPTY	0xfe
#define BYT_ESCAPE	0xfd
/* These bytes are used after BYT_ESCAPE to denote special values.
   They MUST be different from BYT_EMPTY. */
#define BYT_ESCAPED_EMPTY 0x00
#define BYT_ESCAPED_ESCAPE 0x01
#define BYT_ESCAPED_FRAME_END 0x02

struct lffifo *lffifo_alloc(int size)
{
	struct lffifo *p;

	if(size <= 0)
		return NULL;

	p = pvPortMalloc(sizeof(*p) + size);
	if (p == NULL)
		return NULL;

	p->size = size;
	p->start = 0;
	p->end = 0;
	memset(p->buf, BYT_EMPTY, p->size);

	return p;
}

void lffifo_free(struct lffifo *fifo)
{
	vPortFree(fifo);
}

int lffifo_put(struct lffifo *fifo, u8_t *data, u16_t size)
{
	int i,j;
	int x;
	int iteration;

	if (size <= 0)
		return 1;

	/* Do this twice: first to check that there is room, and after to actually
	       put the data into the buffer. We don't want to worry about reader
	       issues with partial frames that do not fit in the fifo. */
	for (iteration = 0; iteration < 2; iteration++)
	{
		i = fifo->end;
		for (j = 0; j <= size; j++)
		{
			x = (j == size ? -1 : data[j]);

			if (iteration == 0 && fifo->buf[i] != BYT_EMPTY)
				return 1;  /* FIFO is full */

			if (x == BYT_EMPTY || x == BYT_ESCAPE || x == -1)
			{
				if (iteration == 1)
					fifo->buf[i] = BYT_ESCAPE;

				i++;
				if (i >= fifo->size)
					i = 0;

				if (iteration == 0 && fifo->buf[i] != BYT_EMPTY)
					return 1;  /* FIFO is full */

				if (x == BYT_EMPTY)
					x = BYT_ESCAPED_EMPTY;
				else if (x == BYT_ESCAPE)
					x = BYT_ESCAPED_ESCAPE;
				else
					x = BYT_ESCAPED_FRAME_END;
			}

			if (iteration == 1)
				fifo->buf[i] = x;

			i++;
			if (i >= fifo->size)
				i = 0;
		}
	}

	fifo->end = i;
	return 0;
}

int lffifo_get(struct lffifo *fifo, u8_t *buf, u16_t bufsize)
{
	int i,j;
	int x;
	int iteration;

	for (iteration = 0; iteration < 2; iteration++)
	{
		i = fifo->start;
		j = 0;
		for(;;)
		{
			x = fifo->buf[i];
			if (iteration == 0)
			{
				if (x == BYT_EMPTY)
					return 0;  /*FIFO is empty */
			}
			else
			{
				fifo->buf[i] = BYT_EMPTY;
			}

			if (x == BYT_ESCAPE)
			{
				i++;
				if (i >= fifo->size)
					i = 0;
				x = fifo->buf[i];
				if (iteration == 0)
				{
					if (x == BYT_EMPTY)
						return 0;  /*FIFO is empty */
				}
				else
				{
					fifo->buf[i] = BYT_EMPTY;
				}

				if (x == BYT_ESCAPED_EMPTY)
					x = BYT_EMPTY;
				else if (x == BYT_ESCAPED_ESCAPE)
					x = BYT_ESCAPE;
				else
					x = -1;  /* Assume frame end */
			}
			i++;
			//2020.8.14 hly change from > to >=
			if (i >= fifo->size)
				i = 0;

			if (x == -1)
			{
				if (j > bufsize)
				{
					if (iteration == 1)
						fifo->start = i;
					return bufsize - j;  /* Passed buffer is too small */
				}
				break;
			}
			else
			{
				if (iteration == 1)
				{
					if (j < bufsize)
						buf[j] = x;
				}
				j++;
			}

			if (j > fifo->size)
			{
				fifo->start = fifo->end;
				if (iteration == 0)
					break;
				else
					return 0;
			}
		}
	}

	fifo->start = i;
	return j;
}
