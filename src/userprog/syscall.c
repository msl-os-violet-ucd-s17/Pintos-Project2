#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"


static void syscall_handler (struct intr_frame *);
void check_ptr(const void *vaddr);
void get_arguments(struct intr_frame *f, int *arg, int n);
int convert_user_kernel(const void *vaddr);
void check_buffer(void* buffer, unsigned size);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int arg[3];
  check_ptr((const void *) f->esp);
 // printf ("system call!\n");
  switch(* (int *) f->esp)
   {
     case SYS_HALT:
     {
       halt();
       break;
     }
     case SYS_EXIT:
     {
       get_arguments(f, &arg[0], 1);
       exit(arg[0]);
       break;
     }
     case SYS_EXEC:
     {
       exec();
       break;
     }
     case SYS_WAIT:
     {
       wait();
       break;
     }
     case SYS_CREATE:
     {
       create();
       break;
     }
     case SYS_REMOVE:
     {
       remove();
       break;
     }
     case SYS_OPEN:
     {
       open();
       break;
     }
     case SYS_FILESIZE:
     {
       filesize();
       break;
     }
     case SYS_READ:
     {
       read();
       break;
     }
     case SYS_WRITE:
     {
       get_arguments(f, &arg[0], 3);
       check_buffer((void *) arg[1], (unsigned) arg[2]);
       arg[1] = convert_user_kernel((const void *) arg[1]);
       f->eax = write(arg[0],(const void *) arg[1],(unsigned) arg[2]);
       break;
     }
     case SYS_SEEK:
     {
       seek();
       break;
     }
     case SYS_TELL:
     {
       tell();
       break;
     }
     case SYS_CLOSE:
     {
       close();
       break;
     }
   }
}

void halt (void)
{ 
  printf("halt called!\n");
  shutdown_power_off();
}

void exit(int status)
{
  struct thread *ethread = thread_current();

  printf("%s: exit(%d)\n", ethread->name, status);
  thread_exit();
}

void exec (void)
{
  printf("exec called!\n");
  return;
}

int wait (void)
{
  printf("wait called!\n");
}

void create (void)
{
  printf("create called!\n");
  return false;
}

void remove (void)
{
  printf("remove called!\n");
  return false;
}

int open (void)
{
  printf("open called!\n");
}

int filesize (void)
{
  printf("filesize called!\n");
}

int read (void)
{
  printf("read called!\n");
}


int write (int fd, const void *buffer, unsigned size)
{
 // printf("write called!\n");
  
  if (fd == STDOUT_FILENO)
  {
    putbuf(buffer, size);
    return size;
  }
}

void seek (void)
{
  printf("seek called!\n");
  return;
}

void tell (void)
{
  printf("tell called!\n");
  return;
}

void close (void)
{
  printf("close called!\n");
  return;
}

void check_ptr(const void *vaddr)
{
  if(!is_user_vaddr(vaddr))
   {
      exit(-1);
   }

}

void check_buffer(void* buffer, unsigned size)
{
  char* checkbuffer = (char *) buffer;
  for(unsigned i = 0; i < size; i++)
  {
    check_ptr((const void*) checkbuffer);
    checkbuffer++;
  }
}

void get_arguments(struct intr_frame *f, int *arg, int n)
{
   int *ptr;
   for(unsigned i = 0; i < n; i++)
   {
     ptr = (int *) f->esp + i + 1;
     check_ptr((const void *) ptr);
     arg[i] = *ptr;
   }
}

int convert_user_kernel(const void *vaddr)
{
   check_ptr(vaddr);
   void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
   if(!ptr)
   {
     exit(-1);
   }
  return (int) ptr;
}
