# Enhanced-XV6

Enhanced-XV6 is an extended version of the XV6 operating system, designed for educational purposes. It builds upon the original XV6 by adding new features and functionalities to explore advanced operating system concepts. The project also includes networking capabilities and additional tools for experimentation.

## Features Implemented

### Core Kernel Enhancements
- **Improved File System**: Added support for advanced file system operations.
- **Process Management**: Enhanced process scheduling and inter-process communication.
- **Memory Management**: Implemented advanced memory allocation techniques.
- **Device Drivers**: Added support for additional devices.

### Networking
- **TCP and UDP Protocols**: Basic implementation of TCP and UDP protocols in the `networks/part-A` directory.
- **Client-Server Communication**: Example client-server programs in `networks/partB` for testing networking capabilities.

### User Programs
- **Custom User Programs**: Additional user programs in the `src/user` directory to demonstrate system calls and kernel features.

## How to Run

### Prerequisites
- A Linux-based system.
- GCC or Clang compiler.
- QEMU installed for running the XV6 operating system.

### Steps to Build and Run

1. Clone the repository:
    ```bash
    git clone https://github.com/your-username/Enhanced-XV6.git
    cd Enhanced-XV6/initial-xv6
    ```

2. Build the kernel:
    ```bash
    make
    ```

3. Run the operating system in QEMU:
    ```bash
    make qemu
    ```

4. To run tests:
    ```bash
    make test
    ```

5. For networking examples:
    - Navigate to the `networks/partB` directory:
        ```bash
        cd ../networks/partB
        ```
    - Compile and run the client and server programs:
        ```bash
        gcc server.c -o server
        gcc client.c -o client
        ./server &
        ./client
        ```

## Repository Structure

```
Enhanced-XV6/
├── initial-xv6/
│   ├── src/
│   │   ├── kernel/       # Kernel source code
│   │   ├── user/         # User programs
│   │   ├── mkfs/         # File system tools
│   │   ├── Makefile      # Build script
│   │   ├── README        # Original XV6 documentation
│   └── LICENSE           # Licensing information
├── networks/
│   ├── part-A/           # TCP and UDP implementation
│   ├── partB/            # Client-server programs
│   └── README.md         # Networking documentation
├── README.md             # Project documentation
└── REPORT1.pdf           # Detailed report on enhancements
```
