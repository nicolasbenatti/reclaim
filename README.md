# reclaim

*Reclaim* is a toy C implementation of a thread-safe memory allocator.

## Introduction

Virtual memory management is a fundamental primitive in commodity, server, and even some embedded operating systems. This is a proposed implementation of a scalable, thread-safe allocation system for managing virtual memory.

The exposed API is quite simple, and adheres to the basic UNIX memory management interface; just `malloc`and `free`.

## Design

## Build and Run

This project makes use of the Make build system. To build and link, just run
```bash
make
```

To cleanup build artifacts, a `clean` target is also available
```bash
make clean
```

## Evaluation

Reclaim is meant to be a performance-oriented allocator. Therefore, the project includes a small (yet significative) benchmark suite, which compares reclaim with `malloc()` and `free()` implementations from glibc.

Relevant metrics:
* Time overhead
* ...

## Results
