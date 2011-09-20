CC = gcc
CFLAGS = -std=c99 -lzmq -ljson -D_GNU_SOURCE
BINS = get_roots \
       watch_dir \
       unwatch_dir \
       get_queue_size \
       get_events

all : $(BINS)

% : %.c
	$(CC) $(CFLAGS) -o $@ $<

clean :
	rm -f $(BINS)
