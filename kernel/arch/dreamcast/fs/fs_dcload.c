#if 0

/* KallistiOS 1.1.5

kernel/arch/dreamcast/fs/fs_dcload.c
(c)2001 Andrew Kieschnick
   
*/

/*

This is a rewrite of Dan Potter's fs_serconsole to use the dcload / dc-tool
fileserver and console. 

printf goes to the dc-tool console
/pc corresponds to / on the system running dc-tool

This is the KOS 1.1.x version - a few things that used to be in this file had
to be moved elsewhere, so using fs_dcload now requires patching a file - 
kernel/arch/dreamcast/kernel/main.c. You should have gotten the patch along with this file...

*/

/* $$$ Ben : Add buffering for READ ONLY open mode. It can be disable by
   setting the dcload_buffering to 0. 
*/

//#define BBA

/* Size of both read frag and file buffer. */
const int dcload_buffering = 1024;

/* Maximal size of chunk of file read in one shot */
#ifdef BBA
# define FRAG 512*100
#else
# define FRAG 512
#endif

#include <dc/fs_dcload.h>
#include <kos/thread.h>
#include <arch/spinlock.h>
#include <kos/dbgio.h>
#include <kos/fs.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <malloc.h>

int dcload_type = DCLOAD_TYPE_NONE;

#define thd_enabled thd_mode
static spinlock_t mutex = SPINLOCK_INITIALIZER;

#ifdef BBA
# define STOPIRQ
# define STARTIRQ
#else
# define STOPIRQ \
 	if (dcload_type == DCLOAD_TYPE_SER && \
            !irq_inside_int()) { oldirq = irq_disable(); } else

# define STARTIRQ \
	if (dcload_type == DCLOAD_TYPE_SER && \
            !irq_inside_int()) { irq_restore(oldirq); } else
#endif

extern int dma_transfering;

#define plain_dclsc(...) ({ \
	int old = 0, rv; \
	if (!irq_inside_int()) { \
		old = irq_disable(); \
/*while(dma_transfering) { irq_restore(old); thd_pass(); old = irq_disable(); }*/ \
	} \
	while ((*(vuint32 *)0xa05f688c) & 0x20) \
		; \
	rv = dcloadsyscall(__VA_ARGS__); \
	if (!irq_inside_int()) \
		irq_restore(old); \
	rv; \
})

// #define plain_dclsc(...) dcloadsyscall(__VA_ARGS__)

static void * lwip_dclsc = 0;

#define dclsc(...) ({ \
    int rv; \
    if (lwip_dclsc) \
	rv = (*(int (*)()) lwip_dclsc)(__VA_ARGS__); \
    else \
	rv = plain_dclsc(__VA_ARGS__); \
    rv; \
})



/* Added by ben for buffering ! */
typedef struct  {
  int max;        /* buffer max size. */
  int cur;        /* current read index into buffer */
  int cnt;        /* number of byte in buffer */
  int hdl;        /* kos file descriptor */
  int tell;       /* shadow tell() */
  int8 buffer[4]; /* read buffer */
} dcload_handler_t;

static dcload_handler_t * dcload_new_handler(int max)
{
  dcload_handler_t * h;
  int size;

  if (max < 1) {
    max = 1;
  }
  size = sizeof(*h) - sizeof(h->buffer) + max;
  h = malloc(size);
  if (h) {
    h->max = max;
    h->cur = 0;
    h->cnt = 0;
    h->hdl = 0;
    h->tell = 0;
    if ((int)h > 0) {
      free(h);
      h = 0;
    }
  }
  return h;
}

static dcload_handler_t * dcload_get_buffer_handler(uint32 hnd)
{
  return ((int)hnd < 0) ? (dcload_handler_t *) hnd : 0;
}

static int dcload_get_handler(uint32 hnd)
{
  dcload_handler_t * dh = dcload_get_buffer_handler(hnd);
  hnd = dh ? dh->hdl : hnd;
  return hnd;
}

/* $$$ ben : used sign for testing dcload direct handle or
   bufferised access. */
static void dcload_close_handler(uint32 hnd)
{
  int oldirq = 0;
  dcload_handler_t *dh;

  if (!hnd) {
    return;
  }
  dh = dcload_get_buffer_handler(hnd);
  if (dh) {
    hnd = dh->hdl;
    free(dh);
  }

  if (hnd > 100) { /* hack */
    STOPIRQ;
    dcloadsyscall(DCLOAD_CLOSEDIR, hnd);
    STARTIRQ;
  } else {
    hnd--; /* KOS uses 0 for error, not -1 */
    STOPIRQ;
    dcloadsyscall(DCLOAD_CLOSE, hnd);
    STARTIRQ;
  }
}

/* Printk replacement */

int dcload_write_buffer(const char *str, int len, int xlat) {
  int oldirq = 0;

  spinlock_lock(&mutex);
  STOPIRQ;
  len = dcloadsyscall(DCLOAD_WRITE, 1, str, len);
  STARTIRQ;
  spinlock_unlock(&mutex);
  return len;
}

static char *dcload_path = NULL;

uint32 dcload_open(vfs_handler_t*vfs, const char *fn, int mode)
{
  dcload_handler_t * hdl = 0;
  int hnd = 0;
  int dcload_mode = 0;
  int oldirq = 0;
  int max_buffer = 0;

/*   dbglog(DBG_DEBUG, */
/* 	 "fs_dcload : open [%s,%d]\n",fn,mode); */
    
  spinlock_lock(&mutex);
    
  if (mode & O_DIR) {
    if (fn[0] == '\0') {
      fn = "/";
    }
    STOPIRQ;
    hnd = dcloadsyscall(DCLOAD_OPENDIR, fn);
    STARTIRQ;
    if (hnd) {
      if (dcload_path)
	free(dcload_path);
      if (fn[strlen(fn)] == '/') {
	dcload_path = malloc(strlen(fn)+1);
	strcpy(dcload_path, fn);
      } else {
	dcload_path = malloc(strlen(fn)+2);
	strcpy(dcload_path, fn);
	strcat(dcload_path, "/");
      }
    }
  } else { /* hack */
    if (mode == O_RDONLY) {
      max_buffer = dcload_buffering;
      dcload_mode = 0;
    } else if (mode == O_RDWR) {
      dcload_mode = 2 | 0x0200;
    } else if (mode == O_WRONLY) {
      dcload_mode = 1 | 0x0200;
    } else if (mode == O_APPEND) {
      dcload_mode =  2 | 8 | 0x0200;
    }
    STOPIRQ;
    hnd = dcloadsyscall(DCLOAD_OPEN, fn, dcload_mode, 0644);
    STARTIRQ;
    hnd++; /* KOS uses 0 for error, not -1 */
  }

  hdl = 0;
  if (hnd > 0 && max_buffer) {
    hdl = dcload_new_handler(max_buffer);
    if (hdl) {
      hdl->hdl = hnd;
      hnd = (int) hdl;
    }
    /* if alloc failed, fallback to nornal handle. */
  }

  spinlock_unlock(&mutex);

/*   dbglog(DBG_DEBUG, */
/* 	 "fs_dcload : open handler = [%p]\n", hdl); */

  return hnd;
}

void dcload_close(uint32 hnd)
{
  spinlock_lock(&mutex);
  dcload_close_handler(hnd);
  spinlock_unlock(&mutex);
}

static ssize_t dcload_read_buffer(uint32 hnd, int8 *buf, size_t cnt)
{
  ssize_t frag = FRAG;
  int oldirq = 0;
  ssize_t ret = 0;
  
  if (dcload_type != DCLOAD_TYPE_SER)
    frag *= 100;

  while (cnt) {
    ssize_t err, n;

    n = cnt;
    if (n > frag) {
      n = frag;
    }
    cnt -= n;

    STOPIRQ;
/*    vid_border_color(0xff,0,0); */
    err = dcloadsyscall(DCLOAD_READ, hnd, buf, n);
/*    vid_border_color(0,0,0); */
    STARTIRQ;

    if (err < 0) {
/*      vid_border_color(0,0xff,0);*/
      if (!ret) {
	ret = -1;
      }
      break;
    } else if (!err) {
      break;
    } else {
      buf += err;
      ret += err;
/*       if (err != n) { */
/* 	vid_border_color(0,0,0xff); */
/* 	break; */
/*       } */
    }
    if (cnt && thd_enabled) {
      thd_pass();
    }
  }

  return ret;
}

static ssize_t retell(dcload_handler_t *h, int inc)
{
  int oldirq = 0;
  ssize_t ret;

  ret = h->tell;
  if (ret == -1) {
    STOPIRQ;
    ret = dcloadsyscall(DCLOAD_LSEEK, h->hdl-1, 0, SEEK_CUR);
    STARTIRQ;
  } else {
    ret += inc;
  }
  return h->tell = ret;
}

ssize_t dcload_read(uint32 hnd, void *buf, size_t cnt)
{
  int oldirq = 0;
  ssize_t ret = -1;
    
  spinlock_lock(&mutex);
    
  if (hnd) {
    dcload_handler_t * dh = dcload_get_buffer_handler(hnd);
    if (!dh) {
      ret = dcload_read_buffer(hnd-1, buf, cnt);
    } else {
      int eof = 0;
      hnd = dh->hdl-1;
      ret = 0;

      while (cnt) {
	ssize_t n;

	n = dh->cnt - dh->cur;
	if (n <= 0) {
	  n = dh->cnt = dh->cur = 0;
	  if (!eof) {
	    n = dcload_read_buffer(hnd, dh->buffer, dh->max);
	    eof = n != dh->max;
	    if (n == -1) {
	      /* $$$ Try */
	      if (!ret)
		ret = n;
	      break;
	    }
	  }
	  dh->cnt = n;
	}

	if (!n) {
	  break;
	}
	retell(dh,n);

	if (n > cnt) {
	  n = cnt;
	}

	/* Fast copy */
	memcpy(buf, dh->buffer+dh->cur, n);
	dh->cur += n;
	cnt -= n;
	ret += n;
	buf = (void *)((int8 *)buf + n);
      }
    }
  }

  spinlock_unlock(&mutex);
  return ret;
}

ssize_t dcload_write(uint32 hnd, const void *buf, size_t cnt)
{
  int oldirq = 0;
  ssize_t ret = -1;
    	
  spinlock_lock(&mutex);

  hnd = dcload_get_handler(hnd);
    
  if (hnd) {
    hnd--; /* KOS uses 0 for error, not -1 */
    STOPIRQ;
    ret = dcloadsyscall(DCLOAD_WRITE, hnd, buf, cnt);
    STARTIRQ;
  }

  spinlock_unlock(&mutex);
  return ret;
}

off_t dcload_seek(uint32 hnd, off_t offset, int whence)
{
  int oldirq = 0;
  off_t ret = -1;

  spinlock_lock(&mutex);

  if (hnd) {
    dcload_handler_t * dh = dcload_get_buffer_handler(hnd);
    if (!dh) {
      hnd--; /* KOS uses 0 for error, not -1 */
      STOPIRQ;
      ret = dcloadsyscall(DCLOAD_LSEEK, hnd, offset, whence);
      STARTIRQ;
    } else {
      const int skip = 0x666;
      off_t cur, start, end;

      hnd = dh->hdl-1;

      switch (whence) {
      case SEEK_END:
	break;

      case SEEK_SET:
      case SEEK_CUR:
	cur = retell(dh,0);
	if (cur == -1) {
	  whence = skip;
	  break;
	}
	/* end is the buffer end file position */
	end = cur;
	/* start is the buffer start file position */
	start = end - dh->cnt;
	/* cur is the buffer current file position */
	cur = start + dh->cur;

	/* offset is the ABSOLUTE seeking position */
	if (whence == SEEK_CUR) {
	  offset += cur;
	  whence = SEEK_SET;
	}

	if (offset >= start && offset <= end) {
	  /* Seeking in the buffer :) */
	  dh->cur = offset - start;
	  whence = skip;
	  ret = offset;
	}
	break;

      default:
	whence = skip;
	break;
      }

      if (whence != skip) {
	STOPIRQ;
	ret = dcloadsyscall(DCLOAD_LSEEK, hnd, offset, whence);
	STARTIRQ;
	dh->cur = dh->cnt = 0;
	dh->tell = ret;
      }
    }
  }
  spinlock_unlock(&mutex);
  return ret;
}

off_t dcload_tell(uint32 hnd)
{
  return dcload_seek(hnd, 0, SEEK_CUR);
}

size_t dcload_total(uint32 hnd) {
  int oldirq = 0;
  size_t ret = -1;
  size_t cur;
	
  spinlock_lock(&mutex);

  hnd = dcload_get_handler(hnd);
	
  if (hnd) {
    hnd--; /* KOS uses 0 for error, not -1 */
    STOPIRQ;
    cur = dcloadsyscall(DCLOAD_LSEEK, hnd, 0, SEEK_CUR);
    ret = dcloadsyscall(DCLOAD_LSEEK, hnd, 0, SEEK_END);
    dcloadsyscall(DCLOAD_LSEEK, hnd, cur, SEEK_SET);
    STARTIRQ;
  }
	
  spinlock_unlock(&mutex);
  return ret;
}

/* Not thread-safe, but that's ok because neither is the FS */
static dirent_t dirent;
dirent_t *dcload_readdir(uint32 hnd)
{
  int oldirq = 0;
  dirent_t *rv = NULL;
  dcload_dirent_t *dcld;
  dcload_stat_t filestat;
  char *fn;

  hnd = dcload_get_handler(hnd);

  if (hnd < 100) return NULL; /* hack */

  spinlock_lock(&mutex);

  STOPIRQ;
  dcld = (dcload_dirent_t *)dcloadsyscall(DCLOAD_READDIR, hnd);
  STARTIRQ;
    
  if (dcld) {
    rv = &dirent;
    strcpy(rv->name, dcld->d_name);
    rv->size = 0;
    rv->time = 0;
    rv->attr = 0; /* what the hell is attr supposed to be anyways? */

    fn = malloc(strlen(dcload_path)+strlen(dcld->d_name)+1);
    strcpy(fn, dcload_path);
    strcat(fn, dcld->d_name);

    STOPIRQ;
    if (!dcloadsyscall(DCLOAD_STAT, fn, &filestat)) {
      if (filestat.st_mode & S_IFDIR)
	rv->size = -1;
      else
	rv->size = filestat.st_size;
      rv->time = filestat.st_mtime;
	    
    }
    STARTIRQ;
	
    free(fn);
  }
    
  spinlock_unlock(&mutex);
  return rv;
}

int dcload_rename(vfs_handler_t*vfs, const char *fn1, const char *fn2) {
  int oldirq = 0;
  int ret;

  spinlock_lock(&mutex);

  /* really stupid hack, since I didn't put rename() in dcload */

  STOPIRQ;
  ret = dcloadsyscall(DCLOAD_LINK, fn1, fn2);

  if (!ret)
    ret = dcloadsyscall(DCLOAD_UNLINK, fn1);
  STARTIRQ;

  spinlock_unlock(&mutex);
  return ret;
}

int dcload_unlink(vfs_handler_t*vfs, const char *fn) {
  int oldirq = 0;
  int ret;

  spinlock_lock(&mutex);

  STOPIRQ;
  ret = dcloadsyscall(DCLOAD_UNLINK, fn);
  STARTIRQ;

  spinlock_unlock(&mutex);
  return ret;
}

/* Pull all that together */
static vfs_handler_t vh = {
  /* Name handler */
  {
    "/pc",		/* name */
    0,		/* tbfi */
    0x00010000,	/* Version 1.0 */
    0,		/* flags */
    NMMGR_TYPE_VFS,
    NMMGR_LIST_INIT
  },
  0,		/* In-kernel, no cacheing */
  NULL,               /* linked list pointer */
  dcload_open, 
  dcload_close,
  dcload_read,
  dcload_write,
  dcload_seek,
  dcload_tell,
  dcload_total,
  dcload_readdir,
  NULL,               /* ioctl */
  dcload_rename,
  dcload_unlink,
  NULL                /* mmap */
};

dbgio_handler_t dbgio_dcload = { 0 };

int fs_dcload_detected() {
    /* Check for dcload */
    if (*DCLOADMAGICADDR == DCLOADMAGICVALUE)
	return 1;
    else
	return 0;
}

/* Call this before arch_init_all (or any call to dbgio_*) to use dcload's
   console output functions. */
void fs_dcload_init_console() {
    /* Setup our dbgio handler */
    memcpy(&dbgio_dcload, &dbgio_null, sizeof(dbgio_dcload));
    dbgio_dcload.detected = fs_dcload_detected;
    dbgio_dcload.write_buffer = dcload_write_buffer;
}

static int *dcload_wrkmem = NULL;

int fs_dcload_init() {
    
  /* Check for dcload */
  if (*DCLOADMAGICADDR != DCLOADMAGICVALUE)
    return -1;

  /* Give dcload the 64k it needs to compress data (if on serial) */
/*   dcload_wrkmem = malloc(65536); */
/*   if (dcload_wrkmem) */
/*     if (dcloadsyscall(DCLOAD_ASSIGNWRKMEM, dcload_wrkmem) == -1) */
/*       free(dcload_wrkmem); */
    
    /* Give dcload the 64k it needs to compress data (if on serial) */
    dcload_wrkmem = malloc(65536);
    if (dcload_wrkmem) {
    	if (dclsc(DCLOAD_ASSIGNWRKMEM, dcload_wrkmem) == -1) {
    	    free(dcload_wrkmem);
    	    dcload_type = DCLOAD_TYPE_IP;
    	    dcload_wrkmem = NULL;
    	} else {
    	    dcload_type = DCLOAD_TYPE_SER;
    	}
    }

    /* If we're not on serial, and we're not on lwIP, we can
       continue using the debug channel */
    if (dcload_type != DCLOAD_TYPE_IP && !lwip_dclsc) {
        dcload_type = DCLOAD_TYPE_NONE;
        dbgio_dev_select("scif");
    }

  /* Register with VFS */
  return nmmgr_handler_add(&vh);
}

int fs_dcload_shutdown() {
  /* Check for dcload */
  if (*DCLOADMAGICADDR != DCLOADMAGICVALUE)
    return -1;

  /* Free dcload wrkram */
  if (dcload_wrkmem) {
    dcloadsyscall(DCLOAD_ASSIGNWRKMEM, 0);
    free(dcload_wrkmem);
  }
    
  return nmmgr_handler_remove(&vh);
}

/* used for dcload-ip + lwIP
 * assumes fs_dcload_init() was previously called
 */
int fs_dcload_init_lwip(void *p)
{
    /* Check for combination of KOS networking and dcload-ip */
    if ((dcload_type == DCLOAD_TYPE_IP) && (__kos_init_flags & INIT_NET)) {
	lwip_dclsc = p;

	dbglog(DBG_INFO, "dc-load console support enabled (lwIP)\n");
    } else
	return -1;

    /* Register with VFS */
    return nmmgr_handler_add(&vh.nmmgr);
}



#else

/* KallistiOS ##version##

   kernel/arch/dreamcast/fs/fs_dcload.c
   Copyright (C)2002 Andrew Kieschnick
   Copyright (C)2004 Dan Potter
   
*/

/*

This is a rewrite of Dan Potter's fs_serconsole to use the dcload / dc-tool
fileserver and console. 

printf goes to the dc-tool console
/pc corresponds to / on the system running dc-tool

*/

#include <dc/fs_dcload.h>
#include <kos/thread.h>
#include <arch/spinlock.h>
#include <kos/dbgio.h>
#include <kos/fs.h>

#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <malloc.h>

CVSID("$Id: fs_dcload.c,v 1.14 2003/04/24 03:05:49 bardtx Exp $");

static spinlock_t mutex = SPINLOCK_INITIALIZER;

#define vid_border_color(r, g, b) (void)0 /* nothing */

#define plain_dclsc(...) ({ \
	int old = 0, rv; \
	if (!irq_inside_int()) { \
		old = irq_disable(); \
	} \
        vid_border_color(0, 255, 255); \
	while ((*(vuint32 *)0xa05f688c) & 0x20) \
		; \
	rv = dcloadsyscall(__VA_ARGS__); \
        vid_border_color(0, 0, 0); \
	if (!irq_inside_int()) \
		irq_restore(old); \
	rv; \
})

// #define plain_dclsc(...) dcloadsyscall(__VA_ARGS__)

static void * lwip_dclsc = 0;

#define dclsc(...) ({ \
    int rv; \
    if (lwip_dclsc) \
	rv = (*(int (*)()) lwip_dclsc)(__VA_ARGS__); \
    else \
	rv = plain_dclsc(__VA_ARGS__); \
    rv; \
})

/* Printk replacement */

int dcload_write_buffer(const uint8 *data, int len, int xlat) {
    if (lwip_dclsc && irq_inside_int()) {
	errno = EAGAIN;
	return -1;
    }
    spinlock_lock(&mutex);
    dclsc(DCLOAD_WRITE, 1, data, len);
    spinlock_unlock(&mutex);

    return len;
}

static char *dcload_path = NULL;
uint32 dcload_open(vfs_handler_t * vfs, const char *fn, int mode) {
    int hnd = 0;
    uint32 h;
    int dcload_mode = 0;
    
    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);
    
    if (mode & O_DIR) {
        if (fn[0] == '\0') {
            fn = "/";
        }
	hnd = dclsc(DCLOAD_OPENDIR, fn);
	if (hnd) {
	    if (dcload_path)
		free(dcload_path);
	    if (fn[strlen(fn) - 1] == '/') {
		dcload_path = malloc(strlen(fn)+1);
		strcpy(dcload_path, fn);
	    } else {
		dcload_path = malloc(strlen(fn)+2);
		strcpy(dcload_path, fn);
		strcat(dcload_path, "/");
	    }
	}
    } else { /* hack */
	if ((mode & O_MODE_MASK) == O_RDONLY)
	    dcload_mode = 0;
	if ((mode & O_MODE_MASK) == O_RDWR)
	    dcload_mode = 2 | 0x0200;
	if ((mode & O_MODE_MASK) == O_WRONLY)
	    dcload_mode = 1 | 0x0200;
	if ((mode & O_MODE_MASK) == O_APPEND)
	    dcload_mode =  2 | 8 | 0x0200;
	if (mode & O_TRUNC)
	    dcload_mode |= 0x0400;
	hnd = dclsc(DCLOAD_OPEN, fn, dcload_mode, 0644);
	hnd++; /* KOS uses 0 for error, not -1 */
    }
    
    h = hnd;

    spinlock_unlock(&mutex);
    return h;
}

void dcload_close(uint32 hnd) {
    if (lwip_dclsc && irq_inside_int())
	return;

    spinlock_lock(&mutex);
    
    if (hnd) {
	if (hnd > 100) /* hack */
	    dclsc(DCLOAD_CLOSEDIR, hnd);
	else {
	    hnd--; /* KOS uses 0 for error, not -1 */
	    dclsc(DCLOAD_CLOSE, hnd);
	}
    }
    spinlock_unlock(&mutex);
}

static ssize_t dcload_read_buffer(uint32 hnd, int8 *buf, size_t cnt)
{
  ssize_t frag = 512;
  int oldirq = 0;
  ssize_t ret = 0;
  
  if (dcload_type != DCLOAD_TYPE_SER)
    frag *= 10;

  while (cnt) {
    ssize_t err, n;

    n = cnt;
    if (n > frag) {
      n = frag;
    }
    cnt -= n;

    err = dclsc(DCLOAD_READ, hnd, buf, n);

    if (err < 0) {
/*      vid_border_color(0,0xff,0);*/
      if (!ret) {
	ret = -1;
      }
      break;
    } else if (!err) {
      break;
    } else {
      buf += err;
      ret += err;
    }
    if (cnt && thd_mode && !irq_inside_int()) {
      thd_pass();
    }
  }

  return ret;
}

ssize_t dcload_read(uint32 hnd, void *buf, size_t cnt) {
    ssize_t ret = -1;
    
    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);
    
    if (hnd) {
	hnd--; /* KOS uses 0 for error, not -1 */
	//ret = dclsc(DCLOAD_READ, hnd, buf, cnt);
	ret = dcload_read_buffer(hnd, buf, cnt);
    }
    
    spinlock_unlock(&mutex);
    return ret;
}

ssize_t dcload_write(uint32 hnd, const void *buf, size_t cnt) {
    ssize_t ret = -1;
    	
    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);
    
    if (hnd) {
	hnd--; /* KOS uses 0 for error, not -1 */
	ret = dclsc(DCLOAD_WRITE, hnd, buf, cnt);
    }

    spinlock_unlock(&mutex);
    return ret;
}

off_t dcload_seek(uint32 hnd, off_t offset, int whence) {
    off_t ret = -1;

    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);

    if (hnd) {
	hnd--; /* KOS uses 0 for error, not -1 */
	ret = dclsc(DCLOAD_LSEEK, hnd, offset, whence);
    }

    spinlock_unlock(&mutex);
    return ret;
}

off_t dcload_tell(uint32 hnd) {
    off_t ret = -1;
    
    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);

    if (hnd) {
	hnd--; /* KOS uses 0 for error, not -1 */
	ret = dclsc(DCLOAD_LSEEK, hnd, 0, SEEK_CUR);
    }

    spinlock_unlock(&mutex);
    return ret;
}

size_t dcload_total(uint32 hnd) {
    size_t ret = -1;
    size_t cur;
	
    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);
	
    if (hnd) {
	hnd--; /* KOS uses 0 for error, not -1 */
	cur = dclsc(DCLOAD_LSEEK, hnd, 0, SEEK_CUR);
	ret = dclsc(DCLOAD_LSEEK, hnd, 0, SEEK_END);
	dclsc(DCLOAD_LSEEK, hnd, cur, SEEK_SET);
    }
	
    spinlock_unlock(&mutex);
    return ret;
}

/* Not thread-safe, but that's ok because neither is the FS */
static dirent_t dirent;
dirent_t *dcload_readdir(uint32 hnd) {
    dirent_t *rv = NULL;
    dcload_dirent_t *dcld;
    dcload_stat_t filestat;
    char *fn;

    if (lwip_dclsc && irq_inside_int())
	return 0;

    if (hnd < 100) return NULL; /* hack */

    spinlock_lock(&mutex);

    dcld = (dcload_dirent_t *)dclsc(DCLOAD_READDIR, hnd);
    
    if (dcld) {
	rv = &dirent;
	strcpy(rv->name, dcld->d_name);
	rv->size = 0;
	rv->time = 0;
	rv->attr = 0; /* what the hell is attr supposed to be anyways? */

	fn = malloc(strlen(dcload_path)+strlen(dcld->d_name)+1);
	strcpy(fn, dcload_path);
	strcat(fn, dcld->d_name);

	if (!dclsc(DCLOAD_STAT, fn, &filestat)) {
	    if (filestat.st_mode & S_IFDIR)
		rv->size = -1;
	    else
		rv->size = filestat.st_size;
	    rv->time = filestat.st_mtime;
	    
	}
	
	free(fn);
    }
    
    spinlock_unlock(&mutex);
    return rv;
}

int dcload_rename(vfs_handler_t * vfs, const char *fn1, const char *fn2) {
    int ret;

    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);

    /* really stupid hack, since I didn't put rename() in dcload */

    ret = dclsc(DCLOAD_LINK, fn1, fn2);

    if (!ret)
	ret = dclsc(DCLOAD_UNLINK, fn1);

    spinlock_unlock(&mutex);
    return ret;
}

int dcload_unlink(vfs_handler_t * vfs, const char *fn) {
    int ret;

    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);

    ret = dclsc(DCLOAD_UNLINK, fn);

    spinlock_unlock(&mutex);
    return ret;
}

/* Pull all that together */
static vfs_handler_t vh = {
	/* Name handler */
	{
		"/pc",		/* name */
		0,		/* tbfi */
		0x00010000,	/* Version 1.0 */
		0,		/* flags */
		NMMGR_TYPE_VFS,
		NMMGR_LIST_INIT
	},

	0, NULL,	/* no cache, privdata */

	dcload_open, 
	dcload_close,
	dcload_read,
	dcload_write,
	dcload_seek,
	dcload_tell,
	dcload_total,
	dcload_readdir,
	NULL,               /* ioctl */
	dcload_rename,
	dcload_unlink,
	NULL                /* mmap */
};

dbgio_handler_t dbgio_dcload = { 0 };

int fs_dcload_detected() {
    /* Check for dcload */
    if (*DCLOADMAGICADDR == DCLOADMAGICVALUE)
	return 1;
    else
	return 0;
}

/* Call this before arch_init_all (or any call to dbgio_*) to use dcload's
   console output functions. */
void fs_dcload_init_console() {
    /* Setup our dbgio handler */
    memcpy(&dbgio_dcload, &dbgio_null, sizeof(dbgio_dcload));
    dbgio_dcload.detected = fs_dcload_detected;
    dbgio_dcload.write_buffer = dcload_write_buffer;
}

static int *dcload_wrkmem = NULL;
int dcload_type = DCLOAD_TYPE_NONE;

/* Call fs_dcload_init_console() before calling fs_dcload_init() */
int fs_dcload_init() {
    
    /* Check for dcload */
    if (*DCLOADMAGICADDR != DCLOADMAGICVALUE)
	return -1;

    /* Give dcload the 64k it needs to compress data (if on serial) */
    dcload_wrkmem = malloc(65536);
    if (dcload_wrkmem) {
    	if (dclsc(DCLOAD_ASSIGNWRKMEM, dcload_wrkmem) == -1) {
    	    free(dcload_wrkmem);
    	    dcload_type = DCLOAD_TYPE_IP;
    	    dcload_wrkmem = NULL;
    	} else {
    	    dcload_type = DCLOAD_TYPE_SER;
    	}
    }

    /* Check for combination of KOS networking and dcload-ip */
    if ((dcload_type == DCLOAD_TYPE_IP) && (__kos_init_flags & INIT_NET)) {
	dbglog(DBG_INFO, "dc-load console+kosnet, will switch to internal ethernet\n");
	return -1;
	/* if (old_printk) {
	    dbgio_set_printk(old_printk);
	    old_printk = 0;
	}
	return -1; */
    }

    /* Register with VFS */
    return nmmgr_handler_add(&vh.nmmgr);
}

int fs_dcload_shutdown() {
    /* Check for dcload */
    if (*DCLOADMAGICADDR != DCLOADMAGICVALUE)
	return -1;

    /* Free dcload wrkram */
    if (dcload_wrkmem) {
        dclsc(DCLOAD_ASSIGNWRKMEM, 0);
        free(dcload_wrkmem);
    }

    /* If we're not on serial, and we're not on lwIP, we can
       continue using the debug channel */
    if (dcload_type != DCLOAD_TYPE_IP && !lwip_dclsc) {
        dcload_type = DCLOAD_TYPE_NONE;
        dbgio_dev_select("scif");
    }

    return nmmgr_handler_remove(&vh.nmmgr);
}

/* used for dcload-ip + lwIP
 * assumes fs_dcload_init() was previously called
 */
int fs_dcload_init_lwip(void *p)
{
    /* Check for combination of KOS networking and dcload-ip */
    if ((dcload_type == DCLOAD_TYPE_IP) && (__kos_init_flags & INIT_NET)) {
	lwip_dclsc = p;

	dbglog(DBG_INFO, "dc-load console support enabled (lwIP)\n");
    } else
	return -1;

    /* Register with VFS */
    return nmmgr_handler_add(&vh.nmmgr);
}


#endif
