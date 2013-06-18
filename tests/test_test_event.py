from test import lock_tests
from pthread_event import Event

class EventTest(lock_tests.EventTests):
    eventtype = staticmethod(Event)
