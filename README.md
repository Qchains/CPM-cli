# CPM - A Foundational Library for Promises in C

CPM is a C library designed to bring promise-based asynchronous programming patterns, inspired by Q Promises, to C development. It helps manage complex asynchronous operations and callback sequences in a more structured and maintainable way.

## Requirements

To use and compile CPM, you will generally need:

* A C compiler supporting at least the C11 standard (e.g., GCC, Clang, MSVC).
* Standard C libraries.
* The Pthreads library (for full functionality, especially if using concurrency-related features or PMLL-hardened components that might rely on `pthread_mutex_t`).
* Build tools like `make` or `CMake` (depending on the project structure).

## Installation

1.  **Clone the Repository:**
    It's recommended to get the source code by cloning the official repository:
    ```bash
    git clone [CPM Repository URL]
    cd cpm
    ```

2.  **Compile the Library:**
    CPM can typically be compiled using standard build tools.
    *(Example using Make)*
    ```bash
    make
    ```
    *(Example using CMake)*
    ```bash
    mkdir build && cd build
    cmake ..
    make
    ```

3.  **Install (Optional):**
    You can install the library system-wide or use it directly from the build directory.
    *(Example for system-wide installation - may require appropriate permissions)*
    ```bash
    sudo make install
    ```
    Alternatively, link against the compiled library files (`.a` or `.so`/`.dylib`) directly in your project.

## Usage

To use CPM in your C project:

1.  **Include the Header:**
    ```c
    #include <cpm.h> // Assuming cpm.h is the main header
    ```

2.  **Compile and Link:**
    When compiling your project, ensure you link against the CPM library and any dependencies like pthreads:
    ```bash
    gcc your_app.c -lcpm -lpthread -o your_app
    ```

**Example Snippet (Conceptual):**

```c
#include <stdio.h>
#include <cpm.h>    // Main CPM header
#include <string.h> // For strdup, if used for values
#include <stdlib.h> // For free, if used for values

// Forward declare your callback functions
PromiseValue my_success_handler(PromiseValue value, void* user_data) {
    printf("[%s] Fulfilled with: %s\n", (char*)user_data, (char*)value);
    // Remember to manage memory if value was dynamically allocated
    // For example, if this handler takes ownership and it's the end of its use:
    // free(value);
    return NULL; // Or return a new value/promise for further chaining
}

PromiseValue my_error_handler(PromiseValue reason, void* user_data) {
    printf("[%s] Rejected with: %s\n", (char*)user_data, (char*)reason);
    // free(reason);
    return NULL;
}

int main() {
    printf("CPM Conceptual Usage Example\n");
    Promise* p1 = promise_create();

    char* user_context = "HandlerContext";
    promise_then(p1, my_success_handler, my_error_handler, user_context);

    // In a real application, an asynchronous operation would resolve/reject 'p1'
    // For this example, we'll resolve it directly:
    char* success_msg = strdup("Hello from CPM!");
    if (success_msg) {
        promise_resolve(p1, (PromiseValue)success_msg);
    } else {
        promise_reject(p1, (PromiseValue)strdup("Failed to allocate message"));
    }
    
    // The promise 'p1' and its value would need to be managed and eventually freed
    // based on the ownership rules defined by your application and the library.
    // For instance, if the value is not freed by the handlers:
    if (p1->state == PROMISE_FULFILLED && p1->value == success_msg) {
        // Assuming 'success_msg' was dynamically allocated and not freed by handler
        // free(p1->value); 
    } else if (p1->state == PROMISE_REJECTED && p1->value != NULL) {
        // Assuming rejection reason was dynamically allocated and not freed by handler
        // free(p1->value);
    }
    // promise_free(p1); // Call when the promise and all its chained operations are truly complete.

    printf("Example finished.\n");
    return 0;
}
