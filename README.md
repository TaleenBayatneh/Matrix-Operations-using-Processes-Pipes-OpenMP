# Matrix Operations using Processes, Pipes & OpenMP

##  Project Overview
This project is developed as part of the **Real-Time Applications & Embedded Systems** course.

The main goal of the project is to perform different **matrix operations** using:
- Multi-processing (fork)
- Pipes for inter-process communication
- Signals
- OpenMP for parallel execution inside workers

Instead of executing all operations in a single process, the program distributes the work across multiple worker processes.

---

##  Main Idea (Simple Explanation)
- The program starts with a **parent process**.
- The parent creates a **pool of worker processes**.
- Each worker is an independent process.
- The parent sends small tasks to workers using pipes.
- Workers perform computations and send results back.
- The parent collects results and builds the final output.

This design improves performance and demonstrates real-time and parallel programming concepts.

---

##  Supported Matrix Operations
The program supports:
- Matrix creation and modification
- Reading matrices from files or folders
- Saving matrices to files or folders
- Matrix addition
- Matrix subtraction
- Matrix multiplication
- Determinant calculation
- Eigenvalues and eigenvectors calculation
- Displaying and managing multiple matrices

---

##  Parallelism Concept
- Each worker handles a **small part of the computation**.
- For matrix operations, work is divided by:
  - Elements
  - Rows
  - Columns
- Workers run in parallel as separate processes.
- OpenMP is optionally used inside workers to speed up loops.

---

##  Process Pool Concept
- Workers are created **once** at program start.
- Workers stay alive and wait for jobs.
- Pipes are used for communication:
  - Parent → Worker (send jobs)
  - Worker → Parent (send results)
- This avoids creating and destroying processes repeatedly.

---

##  Eigenvalues & Eigenvectors
- The project uses an **iterative numerical method**.
- Workers compute row–vector dot products.
- The method repeats until convergence or a fixed number of iterations.
- This approach is efficient for large matrices.

---

##  Performance Measurement
The execution time is measured for:
- Single-process execution
- Multi-process execution
- Multi-process execution with OpenMP

This allows comparing performance improvements.

---

##  Technologies Used
- C Programming Language
- Linux system calls
- fork(), pipe(), poll(), signals
- OpenMP
- GCC Compiler

---

##  Notes
- Workers communicate only through pipes.
- The parent controls scheduling and synchronization.
- The design focuses on clarity, performance, and correctness.
