
/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/file.h>
#include <cups/dir.h>
#include <cups/ppd.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "kc.h"

/*
 * For Debuging...
 */

#if 0
#define DEBUG
#endif

#ifdef DEBUG
	#define debug(format, args...) printf(format, ##args)
#else
	#define debug(format, args...)
#endif


void debugFunc(int argc, char *argv[]);
void debugFunc(int argc, char *argv[])
{
#ifdef DEBUG
  
  int i;
  cups_dest_t *dests;
  int num_dests = cupsGetDests(&dests);
  cups_dest_t *dest;

  /*
   * Print main() args...
   */
  debug("driver: rastertokc.\n");
  for(i = 0; i < argc; i++)
    debug("argv[%d]: %s\n", i, argv[i]);


  /*
   * Print printer options
   */
  if ((dest = cupsGetDest(argv[0], NULL, num_dests, dests)) == NULL)
    debug("Unable to get dest %s.\n", argv[0]);
  else
    debug("Printer: %s.\n", argv[0]);

  for(i = 0; i < dest->num_options; i++)
    debug("%s: %s\n", dest->options[i].name,dest->options[i].value);

  cupsFreeDests(num_dests, dests);

#endif
}


/*
 * Macros...
 */
#define pwrite(s,n) fwrite((s), 1, (n), stdout)


/*
 * Globals...
 */
int		Canceled;		/* Has the current job been canceled? */


/*
 * Prototypes...
 */

void Setup(int argc, char *argv[]);
void StartPage(cups_file_t *fp);
void EndPage(int argc, char *argv[]);

void CancelJob(int sig);

void Setup(int argc, char *argv[])
{

  /* for correct status */
  signal(SIGPIPE, SIG_IGN);
  Canceled = 0;

#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, CancelJob);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = CancelJob;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, CancelJob);
#endif /* HAVE_SIGSET */

  /* the reset command */
  const char	cmd_reset[] = { 0x1b, 0x40 };
  pwrite(cmd_reset, 2);

  fflush(stdout);
}

void StartPage(cups_file_t *fp)
{
  int c;
  while( (!Canceled) && ((c = cupsFileGetChar(fp)) != -1)){
    putchar(c);
  }
  fflush(stdout);
}


/*
 * 'EndPage()' - Finish all page of graphics
 */
void
EndPage(int argc, char *argv[])
{
  char cmd_cutmedia[] = { 0x1d, 0x56, 0x42, 0x00};
  const char *noCutMedia = "noCutMedia";
  const char *CutMedia = "CutMedia";

  if ( (strstr(argv[5], CutMedia) != NULL) && 
       (strstr(argv[5], noCutMedia) == NULL) )
  {
    debug("CutMedia Support!\n");
    pwrite(cmd_cutmedia, 4);
  }

  if (Canceled)
    fprintf(stderr, "INFO: Printing Canceled!\n");
  else
    fprintf(stderr, "INFO: Printing Completed!\n");
}


/*
 * 'CancelJob()' - Cancel the current job...
 */

void
CancelJob(int sig)			/* I - Signal */
{
  (void)sig;

  Canceled = 1;
}

/*
 * 'main()' - Main entry and processing of driver.
 */
int
main(int argc,
     char *argv[])
{
  debugFunc(argc, argv);

  cups_file_t *fp = NULL;
  if (argc == 7){
    if( (fp = cupsFileOpen(argv[6], "r")) == NULL){
      fprintf (stderr, "ERROR: Unable to open file %s!\n", argv[6]);
      return (1);
    }
  }else{
    fprintf (stderr, "ERROR: Unknow Command in driver texttokc!\n");
    return (1);
  }

  Setup(argc, argv);
  StartPage(fp);

  if (fp != NULL)
    cupsFileClose(fp);

  EndPage(argc, argv);
  return (0);
}
