from setuptools import setup
from setuptools import Extension

mod = Extension(
        'pthread_event._pthread_event',
        sources=['pthread_event/_pthread_event.c'],
        extra_compile_args=["-O2"]
        )

setup(
    name='pthread_event',
    version='0.2',
    description="Event using pthread lock API",
    classifiers=[
        'Programming Language :: C',
        'Intended Audience :: Developers',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Operating System :: POSIX',
    ],
    keywords='event pthread',
    author='Stephan Hofmockel',
    author_email="Use the github issues",
    url='https://github.com/stephan-hof/pthread_event',
    license='boost',
    packages=['pthread_event', 'pthread_event.tests'],
    ext_modules=[mod]
)
