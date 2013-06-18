Python C-Extension using pthread to reimplement threading.Event

threading.Event in python 2.X uses a sleep-poll loop to wait for an event.
This module uses the native pthread calls 'pthread_cond_timedwait' and
'pthread_cond_wait' to accomplish the same task.
