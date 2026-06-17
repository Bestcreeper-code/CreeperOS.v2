#pragma once 



#define	E_PERM		 1	/* Operation not permitted */
#define	E_NOENT		 2	/* No such file or directory */
#define	E_SRCH		 3	/* No such process */
#define	E_INTR		 4	/* Interrupted system call */
#define	E_IO		 5	/* I/O error */
#define	E_NXIO		 6	/* No such device or address */
#define	E_2BIG		 7	/* Argument list too long */
#define	E_NOEXEC     8	/* Exec format error */
#define	E_BADF		 9	/* Bad file number */
#define	E_CHILD		10	/* No child processes */
#define	E_AGAIN		11	/* Try again */
#define	E_NOMEM		12	/* Out of memory */
#define	E_ACCES		13	/* Permission denied */
#define	E_FAULT		14	/* Bad address */
#define	E_NOTBLK	15	/* Block device required */
#define	E_BUSY		16	/* Device or resource busy */
#define	E_EXIST		17	/* File exists */
#define	E_XDEV		18	/* Cross-device link */
#define	E_NODEV		19	/* No such device */
#define	E_NOTDIR	20	/* Not a directory */
#define	E_ISDIR		21	/* Is a directory */
#define	E_INVAL		22	/* Invalid argument */
#define	E_NFILE		23	/* File table overflow */
#define	E_MFILE		24	/* Too many open files */
#define	E_NOTTY		25	/* Not a typewriter */
#define	E_TXTBSY	26	/* Text file busy */
#define	E_FBIG		27	/* File too large */
#define	E_NOSPC		28	/* No space left on device */
#define	E_SPIPE		29	/* Illegal seek */
#define	E_ROFS		30	/* Read-only file system */
#define	E_MLINK		31	/* Too many links */
#define	E_PIPE		32	/* Broken pipe */
#define	E_DOM		33	/* Math argument out of domain of func */
#define	E_RANGE		34	/* Math result not representable */

#define E_WOULDBLOCK 	35	/* Operation would block */
#define E_INPROGRESS 	36	/* Operation now in progress */
#define E_ALREADY    	37	/* Operation already in progress */
#define E_NOTSOCK    	38	/* Socket operation on non-socket */
#define E_DESTADDRREQ    39	/* Destination address required */
#define E_MSGSIZE    	40	/* Message too long */
#define E_PROTOTYPE  	41	/* Protocol wrong type for socket */
#define E_NOPROTOOPT 	42	/* Protocol not available */
#define E_PROTONOSUPPORT 43	/* Protocol not supported */
#define E_SOCKTNOSUPPORT 44	/* Socket type not supported */
#define E_OPNOTSUPP  	45	/* Op not supported on transport endpoint */
#define E_PFNOSUPPORT    46	/* Protocol family not supported */
#define E_AFNOSUPPORT    47	/* Address family not supported by protocol */
#define E_ADDRINUSE  	48	/* Address already in use */
#define E_ADDRNOTAVAIL  	49	/* Cannot assign requested address */
#define E_NETDOWN    	50	/* Network is down */
#define E_NETUNREACH 	51	/* Network is unreachable */
#define E_NETRESET   	52	/* Net dropped connection because of reset */
#define E_CONNABORTED   	53	/* Software caused connection abort */
#define E_CONNRESET  	54	/* Connection reset by peer */
#define E_NOBUFS 		55	/* No buffer space available */
#define E_ISCONN 		56	/* Transport endpoint is already connected */
#define E_NOTCONN    	57	/* Transport endpoint is not connected */
#define E_SHUTDOWN   	58	/* No send after transport endpoint shutdown */
#define E_TOOMANYREFS   	59	/* Too many references: cannot splice */
#define E_TIMEDOUT   	60	/* Connection timed out */
#define E_CONNREFUSED   	61	/* Connection refused */
#define E_LOOP   		62	/* Too many symbolic links encountered */
#define E_NAMETOOLONG    63	/* File name too long */
#define E_HOSTDOWN   	64	/* Host is down */
#define E_HOSTUNREACH   	65	/* No route to host */
#define E_NOTEMPTY   	66	/* Directory not empty */
#define E_PROCLIM        67      /* SUNOS: Too many processes */
#define E_USERS  		68	/* Too many users */
#define E_DQUOT  		69	/* Quota exceeded */
#define E_STALE  		70	/* Stale file handle */
#define E_REMOTE 		71	/* Object is remote */
#define E_NOSTR  		72	/* Device not a stream */
#define E_TIME   		73	/* Timer expired */
#define E_NOSR   		74	/* Out of streams resources */
#define E_NOMSG  		75	/* No message of desired type */
#define E_BADMSG 		76	/* Not a data message */
#define E_FSBADCRC    E_BADMSG	/* Bad CRC detected */
#define E_IDRM   		77	/* Identifier removed */
#define E_DEADLK 		78	/* Resource deadlock would occur */
#define E_NOLCK  		79	/* No record locks available */
#define E_NONET  		80	/* Machine is not on the network */
#define E_RREMOTE        81  /* SunOS: Too many lvls of remote in path */
#define E_NOLINK 		82	/* Link has been severed */
#define E_ADV    		83	/* Advertise error */
#define E_SRMNT  		84	/* Srmount error */
#define E_COMM   		85  /* Communication error on send */
#define E_PROTO  		86	/* Protocol error */
#define E_MULTIHOP   	87	/* Multihop attempted */
#define E_DOTDOT 		88	/* RFS specific error */
#define E_REMCHG 		89	/* Remote address changed */
#define E_NOSYS  		90	/* Function not implemented */

#define RET_ERR(err) return -err
#define RET_IF( equation, retval) if(equation) return retval