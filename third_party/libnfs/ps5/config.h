/* libnfs configuration for the PS5 (FreeBSD headers from the payload SDK). */
#define HAVE_ARPA_INET_H 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_INTTYPES_H 1
#define HAVE_NETDB_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NETINET_TCP_H 1
#define HAVE_NET_IF_H 1
#define HAVE_POLL_H 1
#define HAVE_SOCKADDR_LEN 1
#define HAVE_SOCKADDR_STORAGE 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_FILIO_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_SOCKIO_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UIO_H 1
#define HAVE_UNISTD_H 1
/* lib/CMakeLists.txt passes this on the command line. */
#define _U_ __attribute__((unused))
/* FreeBSD has no CLOCK_MONOTONIC_COARSE; CLOCK_MONOTONIC (4) is it. */
#define CLOCK_MONOTONIC_COARSE 4
#define HAVE_SYS_STATVFS_H 1
#define HAVE_UTIME_H 1
/* No HAVE_SIGNAL_H: libnfs would ignore SIGPIPE for the whole app; its
 * sockets get SO_NOSIGPIPE instead (lib/socket.c). */
