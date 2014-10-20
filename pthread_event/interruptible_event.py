from ._pthread_event import SemLock
import threading
import multiprocessing


class Condition(object):
    def __init__(self):
        self._lock = threading.Lock()
        self._waiters = set()

    def _is_owned(self):
        if self._lock.acquire(0):
            self._lock.release()
            return False
        else:
            return True

    def wait(self, timeout=None):
        if not self._is_owned():
            raise RuntimeError("cannot wait on un-acquired lock")

        waiter = SemLock()
        if not (waiter.acquire() is True):
            raise RuntimeError("cannot acquire on SemLock")
        self._waiters.add(waiter)

        gotit = False
        self._lock.release()
        try:
            if timeout is None:
                gotit = waiter.acquire()
            else:
                gotit = waiter.acquire(timeout)
        finally:
            self._lock.acquire()
            if not gotit:
                self._waiters.discard(waiter)

        return gotit

    def notify_all(self):
        if not self._is_owned():
            raise RuntimeError("cannot wait on un-acquired lock")

        for waiter in list(self._waiters):
            waiter.release()
            self._waiters.discard(waiter)


def _reinit_event(ev):
    ev._cond = Condition()


class InterruptibleEvent(object):
    def __init__(self):
        self._cond = Condition()
        self._flag = False
        multiprocessing.util.register_after_fork(self, _reinit_event)

    def is_set(self):
        return self._flag

    def set(self):
        with self._cond._lock:
            self._flag = True
            self._cond.notify_all()

    def clear(self):
        with self._cond._lock:
            self._flag = False

    def wait(self, timeout=None):
        with self._cond._lock:
            if self._flag is True:
                return True
            return self._cond.wait(timeout)
