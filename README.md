NonblockingTcpServer
====================

A simple non-blocking TCP server. This is a single threaded TCP server that does not block on I/O.

I used the select() system call over non-blocking sockets for polling for detecting socket readiness 
for read and write.

Long reads and writes will be done in round robin fashion: instead of finishing one long read or write 
in one go it will be interspered with other reads and writes. Thus no clinet connections will be starved 
off server attention. And there will always be some progress in terms of data reads/writes.
