
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
  debug("driver: commandtokc.\n");
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
  const char  cmd_reset[] = { 0x1b, 0x40 };
  pwrite(cmd_reset, 2);
  
  char *optstr;
  
  char cmd_darkness[] = { 0x12, 0x23, 0x00};
  const char *cupsDarkness = "cupsDarkness=";
  if ( (optstr = strstr(argv[5], cupsDarkness)) != NULL)
  {
    switch(*(optstr + strlen(cupsDarkness)))
    {
      case 'V':
        cmd_darkness[2] = 0x00;
        break;
      case 'L':
        cmd_darkness[2] = 0x04;
        break;
      case 'S':
        cmd_darkness[2] = 0x08;
        break;
      case 'D':
        cmd_darkness[2] = 0x10;
        break;
      case 'M':
        cmd_darkness[2] = 0x17;
        break;
      case 'H':
        cmd_darkness[2] = 0x1f;
        break;
      default:
        cmd_darkness[2] = 0x00;
        break;
    }
    debug("Darkness Support!\n");
    pwrite(cmd_darkness, 3);
  }

  fflush(stdout);
}

void StartPage(cups_file_t *fp)
{

  /*
   * Hear we process the command
   */
  const char *AutoConfigure = "AutoConfigure";
  const char *Clean = "Clean";
  const char *PrintSelfTestPage = "PrintSelfTestPage";
  const char *ReportLevels = "ReportLevels";
  const char *ReportStatus = "ReportStatus";

  char buf[512] = {0};
  while( (!Canceled) && (cupsFileGets(fp, buf, sizeof(buf)) != NULL) )
  {
    if ( strstr(buf, AutoConfigure) != NULL)
    {
      const char  cmd_reset[] = { 0x1b, 0x40 };
      pwrite(cmd_reset, 2);
      fprintf(stderr, "INFO: AutoConfigure Printer!\n");
    }

    if ( strstr(buf, Clean) != NULL)
    {
      const char  cmd_reset[] = { 0x1b, 0x40 };
      pwrite(cmd_reset, 2);
      fprintf(stderr, "INFO: Clean Printer!\n");
    }

    if ( strstr(buf, PrintSelfTestPage) != NULL)
    {
      const char cmd_selftest[] = { 0x12, 0x54};
      pwrite(cmd_selftest, 2);
      fprintf(stderr, "INFO: Print SelfTestPage!\n");
    }

    if ( strstr(buf, ReportLevels) != NULL)
    {
      while ( cupsBackChannelRead(buf, sizeof(buf), 0.0) > 0) 
      {
        debug("Unuseful Data received from device!\n");
      }

      const char cmd_reportlevels[] = { 0x10, 0x04, 0x04 };
      pwrite(cmd_reportlevels, 3);
      buf[0] = 0x00;
      if ( cupsBackChannelRead(buf, 10, 1.0) == 1 )
      {
        debug("Useful Data received from device!\n");
      }
      fprintf(stderr, "INFO: Don't Support Reportlevels!\n");
    }

    if ( strstr(buf, ReportStatus) != NULL)
    {
      while ( cupsBackChannelRead(buf, sizeof(buf), 0.0) > 0) 
      {
        debug("Unuseful Data received from device!\n");
      }

      const char cmd_reportlevels[] = { 0x10, 0x04, 0x01, 0x10, 
                 0x04, 0x02, 0x10, 0x04, 0x03, 0x10, 0x04, 0x04 };
      pwrite(cmd_reportlevels, 12);
      buf[0] = 0x00;buf[1] = 0x00;buf[2] = 0x00;buf[3] = 0x00;
      if ( cupsBackChannelRead(buf, 4, 1.0) == 1 )
      {
        debug("Useful Data received from device!\n");
      }
      fprintf(stderr, "INFO: Don't Support ReportStatus!\n");
    }
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
    fprintf (stderr, "ERROR: Unknow Command in driver commandtokc!\n");
    return (1);
  }

  Setup(argc, argv);
  StartPage(fp);

  if (fp != NULL)
    cupsFileClose(fp);

  EndPage(argc, argv);
  return (0);
}
