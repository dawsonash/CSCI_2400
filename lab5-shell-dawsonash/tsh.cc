// 
// tsh - A tiny shell program with job control
// 
// <Put your name and login ID here>
//

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>

#include "globals.h"
#include "jobs.h"
#include "helper-routines.h"

//
// Needed global variable definitions
//

static char prompt[] = "tsh> ";
int verbose = 0;

//
// You need to implement the functions eval, builtin_cmd, do_bgfg,
// waitfg, sigchld_handler, sigstp_handler, sigint_handler
//
// The code below provides the "prototypes" for those functions
// so that earlier code can refer to them. You need to fill in the
// function bodies below.
// 

void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

//
// main - The shell's main routine 
//
int main(int argc, char **argv) 
{
  int emit_prompt = 1; // emit prompt (default)

  //
  // Redirect stderr to stdout (so that driver will get all output
  // on the pipe connected to stdout)
  //
  dup2(1, 2);

  /* Parse the command line */
  char c;
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h':             // print help message
      usage();
      break;
    case 'v':             // emit additional diagnostic info
      verbose = 1;
      break;
    case 'p':             // don't print a prompt
      emit_prompt = 0;  // handy for automatic testing
      break;
    default:
      usage();
    }
  }

  //
  // Install the signal handlers
  //

  //
  // These are the ones you will need to implement
  //
  Signal(SIGINT,  sigint_handler);   // ctrl-c
  Signal(SIGTSTP, sigtstp_handler);  // ctrl-z
  Signal(SIGCHLD, sigchld_handler);  // Terminated or stopped child

  //
  // This one provides a clean way to kill the shell
  //
  Signal(SIGQUIT, sigquit_handler); 

  //
  // Initialize the job list
  //
  initjobs(jobs);

  //
  // Execute the shell's read/eval loop
  //
  for(;;) {
    //
    // Read command line
    //
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }

    char cmdline[MAXLINE];

    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
      app_error("fgets error");
    }
    //
    // End of file? (did user type ctrl-d?)
    //
    if (feof(stdin)) {
      fflush(stdout);
      exit(0);
    }

    //
    // Evaluate command line
    //
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  } 

  exit(0); //control never reaches here
}
  
/////////////////////////////////////////////////////////////////////////////
//
// eval - Evaluate the command line that the user has just typed in
// 
// If the user has requested a built-in command (quit, jobs, bg or fg)
// then execute it immediately. Otherwise, fork a child process and
// run the job in the context of the child. If the job is running in
// the foreground, wait for it to terminate and then return.  Note:
// each child process must have a unique process group ID so that our
// background children don't receive SIGINT (SIGTSTP) from the kernel
// when we type ctrl-c (ctrl-z) at the keyboard.
//
void eval(char *cmdline) 
{
  /* Parse command line */
  //
  // The 'argv' vector is filled in by the parseline
  // routine below. It provides the arguments needed
  // for the execve() routine, which you'll need to
  // use below to launch a process.
  //
  char *argv[MAXARGS];
  char buf[MAXLINE];
  pid_t pid;
  sigset_t set, prev_set;
    
  strcpy(buf, cmdline);  
  int bg = parseline(buf, argv); //returns 1 for bg job, 0 for fg
  
  if (argv[0] == NULL) 
  {
    return;   /* ignore empty lines */
  }

  if (!builtin_cmd(argv))
  {
    
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);

    sigprocmask(SIG_BLOCK, &set, &prev_set);  
      
      if ((pid = fork()) == 0 ) 
      {
          sigprocmask(SIG_SETMASK, &prev_set, NULL);
          setpgid(0,0);//By making itself the leader of its own group, it cleanly separates itself from any parent shell (or other processes) and establishes its identity for terminal control.
            
            if (execve(argv[0], argv, environ) < 0) 
            {
                printf("%s: Command not found\n", argv[0]);
                exit(0);
            }
      }
      //___________________________________ Parent process {
      addjob(jobs, pid, bg ? BG : FG, cmdline);

      sigprocmask(SIG_SETMASK, &prev_set, NULL);

      
      
      
      //___________________________________ }

      if (!bg)
      {
       waitfg(pid);
      }
      else 
      {
          printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
      }
  }
  return;
}



/////////////////////////////////////////////////////////////////////////////
//
// builtin_cmd - If the user has typed a built-in command then execute
// it immediately. The command name would be in argv[0] and
// is a C string. We've cast this to a C++ string type to simplify
// string comparisons; however, the do_bgfg routine will need 
// to use the argv array as well to look for a job number.
//


//External Command: A command (like /bin/ls) that requires the shell to fork() a child process to run it.

//Built-in Command A command (like quit or jobs) that is executed directly by the shell process without fork().
int builtin_cmd(char **argv) 
{
  string cmd(argv[0]);

  if (cmd == "quit")
  {
  exit(0);
  } 
  else if (cmd == "jobs")
  {
    listjobs(jobs);
    return 1;
  } 
  else if (cmd == "bg")
  {
    do_bgfg(argv);
    return 1; 
  } 
  else if (cmd == "fg")
  {
    do_bgfg(argv);
    return 1;
  }
  return 0;     /* not a builtin command */
}

/////////////////////////////////////////////////////////////////////////////
//
// do_bgfg - Execute the builtin bg and fg commands
//
void do_bgfg(char **argv) 
{
  struct job_t *jobp=NULL;
    
  /* Ignore command if no argument */
  if (argv[1] == NULL) {
    printf("%s command requires PID or %%jobid argument\n", argv[0]);
    return;
  }
    
  /* Parse the required PID or %JID arg */
  if (isdigit(argv[1][0])) {
    pid_t pid = atoi(argv[1]);
    if (!(jobp = getjobpid(jobs, pid))) {
      printf("(%d): No such process\n", pid);
      return;
    }
  }
  else if (argv[1][0] == '%') {
    int jid = atoi(&argv[1][1]);
    if (!(jobp = getjobjid(jobs, jid))) {
      printf("%s: No such job\n", argv[1]);
      return;
    }
  }	    
  else {
    printf("%s: argument must be a PID or %%jobid\n", argv[0]);
    return;
  }

  //
  // You need to complete rest. At this point,
  // the variable 'jobp' is the job pointer
  // for the job ID specified as an argument.
  //
  // Your actions will depend on the specified command
  // so we've converted argv[0] to a string (cmd) for
  // your benefit.
  //
    kill(-jobp->pid, SIGCONT);
    
  string cmd(argv[0]);
    if (cmd == "bg") 
    {
        jobp->state = BG;
    }
    else
    {
        jobp->state = FG;
    }
    
    if (cmd == "bg") 
    {
        printf("[%d] (%d) %s", jobp->jid, jobp->pid, jobp->cmdline);
    }
    if (cmd == "fg")
    {
        waitfg(jobp->pid);
    }
        

  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// waitfg - Block until process pid is no longer the foreground process
//
void waitfg(pid_t pid)
{
    struct job_t *j = getjobpid(jobs,pid);
    if (!j) {
    return; 
    }
    else
    
        //could use sigsuspend(), temporarily changes the signal mask, blocks the shell, and ensures that the blocked SIGCHLD is handled before the shell returns from suspension, solving the race condition elegantly.
        while (j->pid == pid && j->state == FG){
            sleep(1);
        }
    }
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// Signal handlers
//


/////////////////////////////////////////////////////////////////////////////
//
// sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
//     a child job terminates (becomes a zombie), or stops because it
//     received a SIGSTOP or SIGTSTP signal. The handler reaps all
//     available zombie children, but doesn't wait for any other
//     currently running children to terminate.  


//The sigchld_handler calls waitpid() to reap a child. The status variable it gets back is a bitfield. You must use macros to understand how the child's state changed.
void sigchld_handler(int sig) 
{
    pid_t child_pid;
    int status;
    //WUNTRACED reports status changes from stopped children, its how the shell knows a program was stopped by Ctrl-Z
    //WNOHANG allows the while loop to reap all pending children and then exit.
        while((child_pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) 
        {
            
            // Use this first. It's true if the child was stopped (e.g., by SIGTSTP). You should update the job's state to ST
            if (WIFSTOPPED(status))
            {
                getjobpid(jobs,child_pid)->state = ST;
              
                printf("Job [%d] (%d) stopped by signal 20\n",
                    pid2jid(child_pid), child_pid);
                
            }
           //Use this next. It's true if the child was terminated by a signal (e.g., SIGINT). You should delete the job from the list. (This is what you confused with WIFEXITED).
            else if (WIFSIGNALED(status))
            {
                int sig_num = WTERMSIG(status);
                printf("Job [%d] (%d) terminated by signal %d\n",
                    pid2jid(child_pid), child_pid, sig_num);
                deletejob(jobs, child_pid);
            }
             // Use this last. It's true if the child terminated normally (e.g., called exit()). You should delete the job from the list.   
            else if (WIFEXITED(status))
            {
            deletejob(jobs, child_pid);
            }
    }
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigint_handler - The kernel sends a SIGINT to the shell whenver the
//    user types ctrl-c at the keyboard.  Catch it and send it along
//    to the foreground job.  
//

//sigint (Ctrl-C) is received by the shell and forwarded (kill) to the job
void sigint_handler(int sig) 
{
    pid_t pid = fgpid(jobs);
     if (pid != 0) 
     {
        kill(-pid, SIGINT); //Sending a signal to a negative PID sends it to every process in the process group with that ID.
     }
    
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
//     the user types ctrl-z at the keyboard. Catch it and suspend the
//     foreground job by sending it a SIGTSTP.  
//


//The function that catches the SIGTSTP signal (from Ctrl-Z) and forwards it to the foreground process group.
void sigtstp_handler(int sig) 
{
    pid_t pid = fgpid(jobs);
     if (pid != 0) 
    {
        kill(-pid, SIGSTOP);
    }
    
  return;
}

/*********************
 * End signal handlers
 *********************/




