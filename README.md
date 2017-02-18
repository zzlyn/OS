# OS - Some projects related to Operating Systems


Multi Thread APIs:   
  - thread.c / thread.h
  - Built with basic linux system calls like interrupt handling & CPU/Reg operations
  - Contains thread and concurrency control APIs
  - thread_yield, thread_create, thread_kill etc.
  - Conditional variables (cv) methods
    
    
Web Server:
  - server.c / client.c / request.c / request.h are methods to establish a simple client-server model
  - server_thread.c / server_thread.h allow the server to multi process requests
  - Has a cache that stores requested files to speed up request processing
  - LRU (least recently used) cache eviction rule implmentation with Linked List
  
Hash Table
  - wc.h / wc.c
  - Implementation of a hash table that uses strings as key & value
  - Simple word-counting application using hash table
  - Stores & counts # of occurence of every word in a .txt file, support really large .txt files
