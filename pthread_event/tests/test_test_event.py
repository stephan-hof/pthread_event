import unittest
import time
from test import lock_tests
from pthread_event import Event
from pthread_event import SemLock
import threading


class EventTest(lock_tests.EventTests):
    eventtype = staticmethod(Event)


class TestSemLock(unittest.TestCase):
    def test_wrong_release(self):
        lock = SemLock()
        with self.assertRaisesRegexp(Exception, "release unlocked lock"):
            lock.release()

        lock.acquire()
        lock.release()

        with self.assertRaisesRegexp(Exception, "release unlocked lock"):
            lock.release()

    def test_acquire(self):
        lock = SemLock()
        lock.acquire()
        t1 = threading.Thread(target=lock.acquire)
        t1.start()

        t2 = threading.Thread(target=lock.acquire)
        t2.start()

        time.sleep(1)
        lock.release()
        time.sleep(1)
        lock.release()

        t1.join()
        t2.join()
