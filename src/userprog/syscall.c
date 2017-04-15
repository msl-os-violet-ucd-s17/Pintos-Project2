#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "list.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
void* check_ptr(const void*);
struct process_file* list_search(struct list* files, int fd);

extern bool running;

static int get_user (const uint8_t *uaddr);

// STRUCT TO HELP WITH PROCESSING THE FILES
struct process_file {
  struct file* ptr;
  int fd;
  struct list_elem elem;
};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t *user_ptr = f->esp;

  check_ptr(user_ptr);

  uint32_t system_call = *user_ptr;

  switch (system_call)
  {
    case SYS_HALT:
      // SHUTDOWN THE SYSTEM
      shutdown_power_off();
      break;

    case SYS_EXIT:
      // EXIT THE PROCESS
      check_ptr(user_ptr + 1);
      exit_process(*(user_ptr + 1));
      break;

    case SYS_EXEC:
      // EXECUTE THE PROCESS
      check_ptr(user_ptr + 1);
      check_ptr(*(user_ptr + 1));

      f->eax = check_execute_process(*(user_ptr + 1));
      break;

    case SYS_WAIT:
      // MAKE THE SYSTEM WAIT
      check_ptr(user_ptr + 1);

      f->eax = process_wait(*(user_ptr + 1));
      break;
    
    case SYS_CREATE:
      // CREATE A FILE
      check_ptr(user_ptr + 5);
      check_ptr(*(user_ptr + 4));

      // ACQUIRE A LOCK ON THE FILE
      acquire_filesys_lock();
      f->eax = filesys_create(*(user_ptr + 4), *(user_ptr + 5));
      // RELEASE THE LOCK ON THE FILE
      release_filesys_lock();
      break;

    case SYS_REMOVE:
      // DELETE A FILE AFTER ACQUIRING A LOCK, THEN RELEASE THE LOCK AFTER FILE IS DELETED
      check_ptr(user_ptr + 1);
      check_ptr(*(user_ptr + 1));

      // ACQUIRE A LOCK ON THE FILE
      acquire_filesys_lock();

      // IF THE FILE IS NULL THEN MAKE FRAME PTR FALSE, ELSE TRUE
      if(filesys_remove(*(user_ptr + 1)) == NULL)
        f->eax = false;
      else
        f->eax = true;

      // RELEASE THE LOCK ON THE FILE
      release_filesys_lock();
      break;

    case SYS_OPEN:
      // OPEN A FILE
      check_ptr(user_ptr + 1);
      check_ptr(*(user_ptr + 1));

      // ACQUIRE A LOCK ON THE FILE
      acquire_filesys_lock();

      struct file *frame_ptr = filesys_open (*(user_ptr + 1));

      // RELEASE THE LOCK ON THE FILE
      release_filesys_lock();

      // IF FRAME_PTR IS NULL RETURN ERROR, ELSE THE FILE IS OPEN FOR THE USER ADDRESS SPACE
      if(frame_ptr == NULL)
        f->eax = -1;
      else
      {
        struct process_file *user_ptr_file = malloc(sizeof(*user_ptr_file));

        user_ptr_file->ptr = frame_ptr;
        user_ptr_file->fd = thread_current()->file_count;
        thread_current()->file_count++;
        list_push_back (&thread_current()->files, &user_ptr_file->elem);
        f->eax = user_ptr_file->fd;
      }
      break;

    case SYS_FILESIZE:
      // RETURN THE FILE SIZE OF THE FILE
      check_ptr(user_ptr + 1);

      acquire_filesys_lock();
      f->eax = file_length (list_search(&thread_current()->files, *(user_ptr + 1))->ptr);
      release_filesys_lock();
      break;

    case SYS_READ:
      // READ A FILE
      check_ptr(user_ptr + 7);
      check_ptr(*(user_ptr + 6));

      // IF THE (USER_PTR + 5) IS 0 TAKE INPUT FROM THE KEYBOARD
      if(*(user_ptr + 5) == 0)
      {
        int i;
        uint8_t* buffer = *(user_ptr + 6);

        for(i = 0 ; i < *(user_ptr + 7); i++)
          buffer[i] = input_getc();

        f->eax = *(user_ptr + 7);
      }
      else
      {
        // ELSE THE FILE WILL BE READ
        struct process_file* frame_ptr = list_search(&thread_current()->files, *(user_ptr + 5));

        // IF FRAME_PTR IS NULL RETURN ERROR
        if(frame_ptr == NULL)
          f->eax = -1;
        else
        {
          // ELSE READ THE FILE
          acquire_filesys_lock();
	  f->eax = file_read (frame_ptr->ptr, *(user_ptr + 6), *(user_ptr + 7));
	  release_filesys_lock();
	}
      }
      break;

    case SYS_WRITE:
      // WRITE A FILE
      check_ptr(user_ptr + 7);
      check_ptr(*(user_ptr + 6));

      if(*(user_ptr + 5) == 1)
      {
        // WRITE THE NUMBER OF CHARACTERS IN THE BUFFER TO THE CONSOL
        putbuf(*(user_ptr + 6), *(user_ptr + 7));
        f->eax = *(user_ptr + 7);
      }
      else
      {
        
        struct process_file* frame_ptr = list_search(&thread_current()->files, *(user_ptr + 5));
        // IF FRAME_PTR IS NULL RETURN ERROR
        if(frame_ptr == NULL)
          f->eax = -1;
        else
        {
          // ELSE WRITE THE FILE
          acquire_filesys_lock();
          f->eax = file_write (frame_ptr->ptr, *(user_ptr + 6), *(user_ptr + 7));
          release_filesys_lock();
        }
      }
      break;

    case SYS_SEEK:
      // MOVE THE PTR IN THE OPEN FILE TO A CERTAIN POSITION
      check_ptr(user_ptr + 5);

      acquire_filesys_lock();
      file_seek(list_search(&thread_current()->files, *(user_ptr + 4))->ptr, *(user_ptr + 5));
      release_filesys_lock();
      break;

    case SYS_TELL:
      // RETURN THE POSITION OF THE NEXT POSITION TO BE READ OR WRITTEN TO IN THE FILE
      check_ptr(user_ptr + 1);

      acquire_filesys_lock();
      f->eax = file_tell(list_search(&thread_current()->files, *(user_ptr + 1))->ptr);
      release_filesys_lock();
      break;

    case SYS_CLOSE:
      // CLOSE THE OPEN FILE
      check_ptr(user_ptr + 1);

      acquire_filesys_lock();
      close_file(&thread_current()->files,*(user_ptr + 1));
      release_filesys_lock();
      break;


    default:
      printf("Default %d\n", *user_ptr);
  }
}

void* check_ptr(const void *vaddr)
{
  // IF IT IS NOT VALID USER ADDRESS RETURN ERROR
  if (!is_user_vaddr(vaddr))
  {
    exit_process(-1);
    return 0;
  }

  void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);

  // IF THE PTR DOES NOT POINT TO A VALID ADDRESS RETURN ERROR
  if (!ptr)
  {
    exit_process(-1);
    return 0;
  }
  //CHECK THE INDIVIDUAL BYTES OF OUR POINTER
  uint8_t* check_byteptr = (uint8_t *)vaddr;

  //IF ANY OF THE INDIVIDUAL BYTES OF OUR POINTER IS IN INVALID MEMORY, RETURN ERROR
  for (uint8_t i = 0; i < 4; i++) 
  {
    if(get_user(check_byteptr + i) == -1)
    {
      exit_process(-1);
      return 0;
    }
  }

  return ptr;
}


int check_execute_process(char *file_name)
{
  acquire_filesys_lock();
  char *file_name_copy = malloc (strlen(file_name) + 1);
  strlcpy(file_name_copy, file_name, strlen(file_name) + 1);
	  
  char *place_holder_ptr;
  // COPY THE NAME OF THE FILE IN A CHAR STAR
  file_name_copy = strtok_r(file_name_copy, " ", &place_holder_ptr);

  struct file *f = filesys_open (file_name_copy);

  // IF THE FILE IS NULL RETURN ERROR
  if(f == NULL)
  {
    release_filesys_lock();
    return -1;
  }
  else
  {
    // ELSE CLOSE THE FILE AND RELEASE THE LOCK, RETURN THE FILE NAME
    file_close(f);
    release_filesys_lock();
    return process_execute(file_name);
  }
}

void close_file(struct list* files, int fd)
{
  struct list_elem *element;

  struct process_file *f;

  // FOR EACH ELEMENT IN THE LIST...
  for (element = list_begin (files); element != list_end (files); element = list_next (element))
  {
    // CLOSE THE FILE DESCRIPTOR AND REMOVE IT FROM THE LIST
    f = list_entry (element, struct process_file, elem);
    if(f->fd == fd)
    {
      file_close(f->ptr);
      list_remove(element);
    }
  }
  free(f);
}

void close_files(struct list* files)
{
  struct list_elem *element;

  // WHILE THE LIST OF FILES IS NOT EMPTY...
  while(!list_empty(files))
  {
    // CLOSE THE FILE DESCRIPTOR AND REMOVE IT FROM THE LIST FOR ALL FILES
    element = list_pop_front(files);

    struct process_file *f = list_entry (element, struct process_file, elem);
    file_close(f->ptr);
    list_remove(element);
    free(f);
  }     
}

struct process_file* list_search(struct list* files, int fd)
{
  struct list_elem *element;

  // FOR EACH ELEMENT IN THE LIST...
  for (element = list_begin (files); element != list_end (files); element = list_next (element))
  {
    // IF THE FILE DESCRIPTOR MATCHES THE ARGUMENT, RETURN THE MATCHING FILE
    struct process_file *f = list_entry (element, struct process_file, elem);
    if(f->fd == fd)
      return f;
  }
  return NULL;
}

void exit_process(int status)
{
  struct list_elem *element;

  // FOR EACH ELEMENT IN THE LIST...
  for (element = list_begin (&thread_current()->parent->child_proc); element != list_end (&thread_current()->parent->child_proc); element = list_next (element))
  {
    struct child *f = list_entry (element, struct child, elem);
  
    // IF VIRTUAL MEMORY IS READING THE SAME AS THGE KERNEL THEN ASSIGN THE STATUS OF THE FRAME
    if(f->tid == thread_current()->tid)
    {
      f->used = true;
      f->exit_error = status;
    }
  }


  thread_current()->exit_error = status;

  if(thread_current()->parent->wait_on_thread == thread_current()->tid)
    sema_up(&thread_current()->parent->child_lock);

  // EXIT THE PROGRAM
  thread_exit();
}

//ATTEMPTS TO PULL A BYTE FROM THE GIVEN USER ADDRESS, IF A PAGE FAULT OCCURS THEN RETURN ERROR
static int get_user (const uint8_t *uaddr)
{
 int result;
 asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
 return result;
}
