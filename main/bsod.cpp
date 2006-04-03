#include <string.h>
#include <signal.h>
#include <asm/ptrace.h>

#include <lib/base/eerror.h>
#include <lib/base/smartptr.h>
#include <lib/gdi/grc.h>
#include <lib/gdi/gfbdc.h>

/************************************************/

#define CRASH_EMAILADDR "crashlog@dream-multimedia-tv.de"

#define RINGBUFFER_SIZE 16384
static char ringbuffer[RINGBUFFER_SIZE];
static int ringbuffer_head;

static void addToLogbuffer(const char *data, int len)
{
	while (len)
	{
		int remaining = RINGBUFFER_SIZE - ringbuffer_head;
	
		if (remaining > len)
			remaining = len;
	
		memcpy(ringbuffer + ringbuffer_head, data, remaining);
		len -= remaining;
		data += remaining;
		ringbuffer_head += remaining;
		if (ringbuffer_head >= RINGBUFFER_SIZE)
			ringbuffer_head = 0;
	}
}

static std::string getLogBuffer()
{
	int begin = ringbuffer_head;
	while (ringbuffer[begin] == 0)
	{
		++begin;
		if (begin == RINGBUFFER_SIZE)
			begin = 0;
		if (begin == ringbuffer_head)
			return "";
	}
	if (begin < ringbuffer_head)
		return std::string(ringbuffer + begin, ringbuffer_head - begin);
	else
	{
		return std::string(ringbuffer + begin, RINGBUFFER_SIZE - begin) + std::string(ringbuffer, ringbuffer_head);
	}
}

static void addToLogbuffer(int level, const std::string &log)
{
	addToLogbuffer(log.c_str(), log.size());
}


extern std::string getLogBuffer();

void bsodFatal()
{
	char logfile[128];
	sprintf(logfile, "/media/hdd/enigma2_crash_%u.log", (unsigned int)time(0));
	FILE *f = fopen(logfile, "wb");
	
	std::string lines = getLogBuffer();

	if (f)
	{
		time_t t = time(0);
		fprintf(f, "enigma2 crashed on %s", ctime(&t));
		fprintf(f, "please email this file to " CRASH_EMAILADDR "\n");
		std::string buffer = getLogBuffer();
		fwrite(buffer.c_str(), buffer.size(), 1, f);
		fclose(f);
	}
	
	ePtr<gFBDC> my_dc;
	gFBDC::getInstance(my_dc);
	
	{
		gPainter p(my_dc);
		p.resetClip(eRect(ePoint(0, 0), my_dc->size()));
		p.setBackgroundColor(gRGB(0x0000C0));
		p.setForegroundColor(gRGB(0xFFFFFF));
	
		ePtr<gFont> font = new gFont("Regular", 20);
		p.setFont(font);
		p.clear();
	
		eRect usable_area = eRect(100, 70, my_dc->size().width() - 200, 100);
	
		p.renderText(usable_area, 
			"We are really sorry. Something happened "
			"which should not have happened, and "
			"resulted in a crash. If you want to help "
			"us in improving this situation, please send "
			"the logfile created in /hdd/ to " CRASH_EMAILADDR ".", gPainter::RT_WRAP|gPainter::RT_HALIGN_LEFT);
	
		usable_area = eRect(100, 170, my_dc->size().width() - 200, my_dc->size().height() - 20);
	
		int i;
	
		size_t start = std::string::npos + 1;
		for (i=0; i<20; ++i)
		{
			start = lines.rfind('\n', start - 1);
			if (start == std::string::npos)
			{
				start = 0;
				break;
			}
		}
	
		font = new gFont("Regular", 14);
		p.setFont(font);
	
		p.renderText(usable_area, 
			lines.substr(start), gPainter::RT_HALIGN_LEFT);
	}

	raise(SIGKILL);
}

#if defined(__MIPSEL__)
void oops(const mcontext_t &context, int dumpcode)
{
	eDebug("PC: %08lx, vaddr: %08lx", (unsigned long)context.pc, (unsigned long)context.badvaddr);
}
#else
#warning "no oops support!"
#error bla
#define NO_OOPS_SUPPORT
#endif

void handleFatalSignal(int signum, siginfo_t *si, void *ctx)
{
	ucontext_t *uc = (ucontext_t*)ctx;
	eDebug("KILLED BY signal %d", signum);
#ifndef NO_OOPS_SUPPORT
	oops(uc->uc_mcontext, signum == SIGSEGV);
#endif
	eDebug("-------");
	bsodFatal();
}

void bsodCatchSignals()
{
	struct sigaction act;
	act.sa_handler = SIG_DFL;
	act.sa_sigaction = handleFatalSignal;
	act.sa_flags = 0;
	if (sigemptyset(&act.sa_mask) == -1)
		perror("sigemptyset");
	
		/* start handling segfaults etc. */
	sigaction(SIGSEGV, &act, 0);
	sigaction(SIGILL, &act, 0);
	sigaction(SIGBUS, &act, 0);
}

void bsodLogInit()
{
	logOutput.connect(addToLogbuffer);
}
