#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
void wait_before_exec(struct thread*);
int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;

  wait_before_exec(thread); // 1.1
  //cprintf("path: %s\n", path);
  //cprintf("argv[0]: %s\n", argv[0]);
  begin_op();
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) < sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;


  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }

  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(proc->name, last, sizeof(proc->name));

  // Commit to the user image.
  oldpgdir = proc->pgdir;
  proc->pgdir = pgdir;
  proc->sz = sz;
  thread->tf->eip = elf.entry;  //@ // main
  thread->tf->esp = sp;         //@ 
  switchuvm(proc);
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}


void wait_before_exec(struct thread* t){
  struct proc *cp;    // containing process
  struct thread *ot;  // other thread
  cp = t->tproc;
  int threadsAlive=0;
  
  acquire(&cp->lock); // TODO: is "cp->lock" ok?
  cp->executed=1;
  while(1){
    for(ot=cp->pthreads; ot < &cp->pthreads[NTHREAD]; ot++){
      if(ot == t || ot->state == UNUSED)
        continue;
      if(ot->state==ZOMBIE){
        kfree(ot->kstack);
        ot->kstack = 0;
        ot->state = UNUSED;
        continue; 
      }
      // ot is still allive: EMBRYO / RUNNABLE / RUNNNIG / SLEEPING
      threadsAlive=1;
      // wake up sleeping thread to rush him
      if(ot->state == SLEEPING)
       ot->state = RUNNABLE;
    }

    if(!threadsAlive){
      cp->executed = 0;
      release(&cp->lock);
      return;
    }

    sleep(cp, &cp->lock); // TODO: is "cp" ok?
  }
}


