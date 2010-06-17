ifeq ($(USE_CLANG),)
  CC = gcc
  LD = $(CC)
else
  CC = clang
  LD = $(CC)
endif


#SH=--coverage -O2 -DHELGRIND
#SH=--coverage -O2
#SH=--profile-generate
SH=-O2

COPTS = -Wall -fpic -ftls-model=initial-exec -g \
	-march=native -mtune=native		\
	$(SH)

LDFLAGS = -g $(SH)

LDOPTS  = -lpthread -fpic -lrt

ECHO = echo

OBJS = 	\
	msock_base.o		\
	msock_domain.o		\
	msock_engine.o		\
	msock_process.o		\
	msock_reg.o		\
	msock_utils.o		\
	msock_worker.o		\
	memalloc.o		\
	umap.o			\
	timer.o			\
	msock_engine_io.o	\
	io.o			\
	msock_engine_fd.o	\
	msock_engine_user.o	\
	msock_engine_signal.o	\
	msock_engine_epoll.o
#	msock_engine_select.o


example01: src/rel/example01.o libmsock.so
	$(LD) $(LDFLAGS) -Wl,-rpath=. -o $@ $^ -lmsock -L.
clean::
	rm -f example01

example02: src/rel/example02.o libmsock.so
	$(LD) $(LDFLAGS) -Wl,-rpath=. -o $@ $^ -lmsock -L.
clean::
	rm -f example02

example03: src/rel/example03.o libmsock.so
	$(LD) $(LDFLAGS) -Wl,-rpath=. -o $@ $^ -lmsock -L.
clean::
	rm -f example03

example04: src/rel/example04.o libmsock.so
	$(LD) $(LDFLAGS) -Wl,-rpath=. -o $@ $^ -lmsock -L.
clean::
	rm -f example04

example05: src/rel/example05.o libmsock.so
	$(LD) $(LDFLAGS) -Wl,-rpath=. -o $@ $^ -lmsock -L. -lrt
clean::
	rm -f example05

example06: src/rel/example06.o libmsock.so
	$(LD) $(LDFLAGS) -Wl,-rpath=. -o $@ $^ -lmsock -L. -lrt
clean::
	rm -f example06

example07: src/rel/example07.o libmsock.so
	$(LD) $(LDFLAGS) -Wl,-rpath=. -o $@ $^ -lmsock -L. -lrt
clean::
	rm -f example07

libmsock.so:: $(patsubst %, src/rel/%, $(OBJS))
	$(LD) $(LDFLAGS) -shared -o $@ $^ $(LDOPTS)
clean::
	rm -f libmsock.so

src/rel/%.o: src/%.c
	@mkdir src/rel 2>/dev/null || true
	$(CC) $(COPTS) -c -o $@ $<
clean::
	rm -f src/rel/*.o


