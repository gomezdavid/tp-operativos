# TP SisOp - 1C - 2025

University group assignment where we developed a simulation of an operating system. This assignment was created by [Tomas Peña](https://github.com/tomasp0603), [David Gómez](https://github.com/alenodav), and [Pedro Baccaro](https://github.com/pedro-baccaro).

A special mention to [David Lozada](https://www.linkedin.com/in/davidlozadaezequiel/) who helped us during the first couple of weeks with the CPU module. Due to personal issues, he decided to step out of the project.

## Project Specification

[Assignment specification](https://docs.google.com/document/d/1HC9Zi-kpn8jI_egJGEZe77wUCbSkwSw9Ygqqs7m_-is/edit?tab=t.0#heading=h.45hwzxoodksd)

The following image shows how the modules interact:

- **Kernel**: Coordinates process scheduling and communication between modules.
- **Memoria**: Simulates memory management using multi-level paging and frame allocation.
- **CPU**: Executes instructions, manages process execution cycles, and interacts with the Kernel for context switching.
- **IO**: Handles input/output operations asynchronously using simulated devices.

![Module Interaction](https://github.com/user-attachments/assets/022f4814-148f-41f1-8d51-cdb3f1c35988)

## ⚙️ Technologies & Tools

- Linux VM
- Programming Language: **C**
- Inter-process communication: **Sockets**
- Concurrency: **Threads**
- Project management: **Makefiles**, `valgrind`
[so-deploy]: https://github.com/sisoputnfrba/so-deploy
