/* See LICENSE for licence details. */
/* yaft.c: include main function */
#include "yaft.h"
#include "conf.h"
#include "util.h"
#include "fb/common.h"
#include "terminal.h"
#include "ctrlseq/esc.h"
#include "ctrlseq/csi.h"
#include "ctrlseq/osc.h"
#include "ctrlseq/dcs.h"
#include "parse.h"

ssize_t process_hangul(struct terminal_t *term, const uint8_t *buf, size_t size)
{
	ssize_t ret = 0;
	int bufpos = 0;
	const ucschar *commit_string;
	char u8buf[BUFSIZE];
	bool used;

	for (int i = 0; i < size; i++) {
		if (term->hangul_mode) { //한글모드
			// 입력받은 키를 처리
			// ESC 누를 시 영문으로 전환 (vim 사용시 한영 전환 편의를 위함)
			if (buf[i] == ESC || buf[i] == HANGUL_KEY) {
				term->hangul_mode = false;
				commit_string = hangul_ic_flush(term->hic);
			} else {
				if (buf[i] == DEL || buf[i] == BS) { // 백스페이스 처리
					used = hangul_ic_backspace(term->hic);
				} else
					used = hangul_ic_process(term->hic, buf[i]);
				commit_string = hangul_ic_get_commit_string(term->hic);
			}

			term->preedit_str = hangul_ic_get_preedit_string(term->hic);

			// UTF-8로 출력
			bufpos = 0;
			for (int j = 0; commit_string[j] != 0; j++) {
				if (commit_string[j] < 0x80)
					u8buf[bufpos++] = commit_string[j];
				else if (commit_string[j] < 0x800) {
					u8buf[bufpos++] = 0xC0 | (commit_string[j] >> 6);
					u8buf[bufpos++] = 0x80 | (commit_string[j] & 0x3F);
				} else if (commit_string[j] < 0x10000) {
					u8buf[bufpos++] = 0xE0 | (commit_string[j] >> 12);
					u8buf[bufpos++] = 0x80 | (commit_string[j] >> 6 & 0x3F);
					u8buf[bufpos++] = 0x80 | (commit_string[j] & 0x3F);
				}
			}
			if (bufpos)
				ret += ewrite(term->fd, u8buf, bufpos);

			if ((buf[i] == ESC || !used) && buf[i] != HANGUL_KEY) {
				ret += ewrite(term->fd, buf+i, 1);
				used = true;
			}
		} else { //영어모드
			if (buf[i] == HANGUL_KEY)
				term->hangul_mode = true;
			else
				ret += ewrite(term->fd, buf+i, 1);
		}
	}

	return ret;
}

void sig_handler(int signo)
{
	sigset_t sigset;
	/* global */
	extern volatile sig_atomic_t vt_active;
	extern volatile sig_atomic_t child_alive;
	extern volatile sig_atomic_t need_redraw;

	logging(DEBUG, "caught signal! no:%d\n", signo);

	if (signo == SIGCHLD) {
		child_alive = false;
		wait(NULL);
	} else if (signo == SIGUSR1) { /* vt activate */
		vt_active   = true;
		need_redraw = true;
		ioctl(STDIN_FILENO, VT_RELDISP, VT_ACKACQ);
	} else if (signo == SIGUSR2) { /* vt deactivate */
		vt_active = false;
		ioctl(STDIN_FILENO, VT_RELDISP, 1);

		if (BACKGROUND_DRAW) { /* update passive cursor */
			need_redraw = true;
		} else {               /* sleep until next vt switching */
			sigfillset(&sigset);
			sigdelset(&sigset, SIGUSR1);
			sigsuspend(&sigset);
		}
	}
}

void set_rawmode(int fd, struct termios *save_tm)
{
	struct termios tm;

	tm = *save_tm;
	tm.c_iflag     = tm.c_oflag = 0;
	tm.c_cflag    &= ~CSIZE;
	tm.c_cflag    |= CS8;
	tm.c_lflag    &= ~(ECHO | ISIG | ICANON);
	tm.c_cc[VMIN]  = 1; /* min data size (byte) */
	tm.c_cc[VTIME] = 0; /* time out */
	etcsetattr(fd, TCSAFLUSH, &tm);
}

bool tty_init(struct termios *termios_orig)
{
	struct sigaction sigact;

	memset(&sigact, 0, sizeof(struct sigaction));
	sigact.sa_handler = sig_handler;
	sigact.sa_flags   = SA_RESTART;
	esigaction(SIGCHLD, &sigact, NULL);

	if (VT_CONTROL) {
		esigaction(SIGUSR1, &sigact, NULL);
		esigaction(SIGUSR2, &sigact, NULL);

		struct vt_mode vtm;
		vtm.mode   = VT_PROCESS;
		vtm.waitv  = 0;
		vtm.acqsig = SIGUSR1;
		vtm.relsig = SIGUSR2;
		vtm.frsig  = 0;

		if (ioctl(STDIN_FILENO, VT_SETMODE, &vtm))
			logging(WARN, "ioctl: VT_SETMODE failed (maybe here is not console)\n");

		if (FORCE_TEXT_MODE == false) {
			if (ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS))
				logging(WARN, "ioctl: KDSETMODE failed (maybe here is not console)\n");
		}
	}

	etcgetattr(STDIN_FILENO, termios_orig);
	set_rawmode(STDIN_FILENO, termios_orig);
	ewrite(STDIN_FILENO, "\033[?25l", 6); /* make cusor invisible */

	return true;
}

void tty_die(struct termios *termios_orig)
{
 	/* no error handling */
	struct sigaction sigact;
	struct vt_mode vtm;

	memset(&sigact, 0, sizeof(struct sigaction));
	sigact.sa_handler = SIG_DFL;
	sigaction(SIGCHLD, &sigact, NULL);

	if (VT_CONTROL) {
		sigaction(SIGUSR1, &sigact, NULL);
		sigaction(SIGUSR2, &sigact, NULL);

		vtm.mode   = VT_AUTO;
		vtm.waitv  = 0;
		vtm.relsig = vtm.acqsig = vtm.frsig = 0;

		ioctl(STDIN_FILENO, VT_SETMODE, &vtm);

		if (FORCE_TEXT_MODE == false)
			ioctl(STDIN_FILENO, KDSETMODE, KD_TEXT);
	}

	tcsetattr(STDIN_FILENO, TCSAFLUSH, termios_orig);
	fflush(stdout);
	ewrite(STDIN_FILENO, "\033[?25h", 6); /* make cursor visible */
}

bool fork_and_exec(int *master, const char *cmd, char *const argv[], int lines, int cols)
{
	pid_t pid;
	struct winsize ws = {.ws_row = lines, .ws_col = cols,
		/* XXX: this variables are UNUSED (man tty_ioctl),
			but useful for calculating terminal cell size */
		.ws_ypixel = CELL_HEIGHT * lines, .ws_xpixel = CELL_WIDTH * cols};

	pid = eforkpty(master, NULL, NULL, &ws);
	if (pid < 0)
		return false;
	else if (pid == 0) { /* child */
		esetenv("TERM", term_name, 1);
		eexecvp(cmd, argv);
		/* never reach here */
		exit(EXIT_FAILURE);
	}
	return true;
}

int check_fds(fd_set *fds, struct timeval *tv, int input, int master)
{
	FD_ZERO(fds);
	FD_SET(input, fds);
	FD_SET(master, fds);
	tv->tv_sec  = 0;
	tv->tv_usec = SELECT_TIMEOUT;
	return eselect(master + 1, fds, NULL, NULL, tv);
}

int main(int argc, char *const argv[])
{
	extern const char *shell_cmd; /* defined in conf.h */
	const char *cmd;
	uint8_t buf[BUFSIZE];
	ssize_t size;
	fd_set fds;
	struct timeval tv;
	struct framebuffer_t fb;
	struct terminal_t term;
	/* global */
	extern volatile sig_atomic_t need_redraw;
	extern volatile sig_atomic_t child_alive;
	extern struct termios termios_orig;

	/* init */
	if (setlocale(LC_ALL, "") == NULL) /* for wcwidth() */
		logging(WARN, "setlocale falied\n");

	if (!fb_init(&fb)) {
		logging(FATAL, "framebuffer initialize failed\n");
		goto fb_init_failed;
	}

	if (!term_init(&term, fb.info.width, fb.info.height)) {
		logging(FATAL, "terminal initialize failed\n");
		goto term_init_failed;
	}

	if (!tty_init(&termios_orig)) {
		logging(FATAL, "tty initialize failed\n");
		goto tty_init_failed;
	}

	/* fork and exec shell */
	cmd = (argc < 2) ? shell_cmd: argv[1];
	if (!fork_and_exec(&term.fd, cmd, argv + 1, term.lines, term.cols)) {
		logging(FATAL, "forkpty failed\n");
		goto tty_init_failed;
	}
	child_alive = true;

	/* main loop */
	while (child_alive) {
		if (need_redraw) {
			need_redraw = false;
			cmap_update(fb.fd, fb.cmap); /* after VT switching, need to restore cmap (in 8bpp mode) */
			redraw(&term);
			refresh(&fb, &term);
		}

		if (check_fds(&fds, &tv, STDIN_FILENO, term.fd) == -1)
			continue;

		if (FD_ISSET(STDIN_FILENO, &fds)) {
			if ((size = read(STDIN_FILENO, buf, BUFSIZE)) > 0) {
				process_hangul(&term, buf, size);
				term.line_dirty[term.cursor.y] = true;
				refresh(&fb, &term);
			}
		}
		if (FD_ISSET(term.fd, &fds)) {
			if ((size = read(term.fd, buf, BUFSIZE)) > 0) {
				if (VERBOSE)
					ewrite(STDOUT_FILENO, buf, size);
				parse(&term, buf, size);
				if (LAZY_DRAW && size == BUFSIZE)
					continue; /* maybe more data arrives soon */
				refresh(&fb, &term);
			}
		}
	}

	/* normal exit */
	tty_die(&termios_orig);
	term_die(&term);
	fb_die(&fb);
	return EXIT_SUCCESS;

	/* error exit */
tty_init_failed:
	term_die(&term);
term_init_failed:
	fb_die(&fb);
fb_init_failed:
	return EXIT_FAILURE;
}
