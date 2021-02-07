#include "policyd.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <mach/mach.h>

#define PT_DETACH    11
#define PT_ATTACHEXC 14

typedef struct _FridaPolicydRequest FridaPolicydRequest;

struct _FridaPolicydRequest
{
  union __RequestUnion__frida_policyd_subsystem body;
  mach_msg_trailer_t trailer;
};

extern kern_return_t bootstrap_register (mach_port_t bp, const char * service_name, mach_port_t sp);
extern int ptrace (int request, pid_t pid, void * addr, int data);

#define frida_policyd_soften frida_policyd_do_soften
#include "policyd-server.c"

int
main (int argc, char * argv[])
{
  kern_return_t kr;
  mach_port_t listening_port;

  signal (SIGCHLD, SIG_IGN);

  kr = mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_RECEIVE, &listening_port);
  assert (kr == KERN_SUCCESS);

  kr = bootstrap_register (bootstrap_port, FRIDA_POLICYD_SERVICE_NAME, listening_port);
  if (kr != KERN_SUCCESS)
    goto checkin_error;

  while (TRUE)
  {
    FridaPolicydRequest request;
    union __ReplyUnion__frida_policyd_subsystem reply;
    mach_msg_header_t * header_in, * header_out;
    boolean_t handled;

    bzero (&request, sizeof (request));

    header_in = (mach_msg_header_t *) &request;
    header_in->msgh_size = sizeof (request);
    header_in->msgh_local_port = listening_port;

    kr = mach_msg_receive (header_in);
    if (kr != KERN_SUCCESS)
      break;

    header_out = (mach_msg_header_t *) &reply;

    handled = frida_policyd_server (header_in, header_out);
    if (handled)
      mach_msg_send (header_out);

    mach_msg_destroy (header_in);
  }

  return 0;

checkin_error:
  {
    fputs ("Unable to check in with launchd: are we running standalone?\n", stderr);
    return 1;
  }
}

kern_return_t
frida_policyd_do_soften (mach_port_t server, int pid, int * error_code)
{
  boolean_t should_retry;

  if (ptrace (PT_ATTACHEXC, pid, NULL, 0) == -1)
    goto attach_failed;

  do
  {
    int res = ptrace (PT_DETACH, pid, NULL, 0);

    should_retry = res == -1 && errno == EBUSY;
    if (should_retry)
      usleep (10000);
  }
  while (should_retry);

  *error_code = 0;

  return KERN_SUCCESS;

attach_failed:
  {
    *error_code = (errno == EBUSY) ? 0 : errno;

    return KERN_SUCCESS;
  }
}

