/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Development: a sampling profiler for the MiSTer, which has no perf.
// MISTER_PROF=<file> samples the program counter and the return address of
// the thread that runs, 1000 times a second of CPU time, and writes them to
// <file> (pairs of 32-bit words, thread id in the first of each sample) at
// the end and whenever the program gets SIGUSR2. tools/prof.py makes a
// table of them with the unstripped binary.

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "backends/platform/sdl/mister/mister_port.h"

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#define PROF_MAX (1 << 20)

static uint32_t *profBuf;
static volatile uint32_t profCount;
static int profFd = -1;

static void profSample(int sig, siginfo_t *info, void *context) {
	uint32_t n = __sync_fetch_and_add(&profCount, 1);
	if (n >= PROF_MAX)
		return;
#if defined(__arm__)
	const mcontext_t *mc = &((const ucontext_t *)context)->uc_mcontext;
	profBuf[n * 3] = (uint32_t)syscall(SYS_gettid);
	profBuf[n * 3 + 1] = mc->arm_pc;
	profBuf[n * 3 + 2] = mc->arm_lr;
#elif defined(__x86_64__)
	const mcontext_t *mc = &((const ucontext_t *)context)->uc_mcontext;
	profBuf[n * 3] = (uint32_t)syscall(SYS_gettid);
	profBuf[n * 3 + 1] = (uint32_t)mc->gregs[REG_RIP];
	profBuf[n * 3 + 2] = 0;
#endif
}

static void profWrite(int sig) {
	uint32_t n = profCount < PROF_MAX ? profCount : PROF_MAX;
	if (lseek(profFd, 0, SEEK_SET) == 0 && ftruncate(profFd, 0) == 0) {
		ssize_t r = write(profFd, profBuf, n * 12);
		(void)r;
	}
}

static void profEnd() {
	profWrite(0);
}

void MiSTer_ProfStart() {
	const char *path = getenv("MISTER_PROF");
	if (!path || !*path)
		return;
	profFd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	profBuf = (uint32_t *)malloc(PROF_MAX * 12);
	if (profFd < 0 || !profBuf)
		return;

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = profSample;
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sigaction(SIGPROF, &sa, nullptr);
	signal(SIGUSR2, profWrite);
	atexit(profEnd);

	struct itimerval t;
	t.it_interval.tv_sec = 0;
	t.it_interval.tv_usec = 1000;
	t.it_value = t.it_interval;
	setitimer(ITIMER_PROF, &t, nullptr);
}

// The frames shown a second, and the game thread's own time a frame (its
// CPU time, without the waits for the field): the frames shown go by
// whole fields (in 480i 29.8 or 14.9 a second), so a frame made faster
// shows in them only once it takes a field less
void MiSTer_ProfFrame() {
	static int frames;
	static struct timespec last, lastCpu;
	struct timespec now, cpu;

	if (profFd < 0)
		return;
	clock_gettime(CLOCK_MONOTONIC, &now);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu);
	if (!last.tv_sec) {
		last = now;
		lastCpu = cpu;
	}
	frames++;
	double s = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) / 1e9;
	if (s >= 5) {
		double work = (cpu.tv_sec - lastCpu.tv_sec) + (cpu.tv_nsec - lastCpu.tv_nsec) / 1e9;
		fprintf(stderr, "MiSTer prof: %.1f frames/s, %.1f ms of work a frame\n", frames / s, 1000 * work / frames);
		frames = 0;
		last = now;
		lastCpu = cpu;
	}
}
