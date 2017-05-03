# NOTE: Last ones are only required for Pango, maybe move to HarfBuzz?
DEP_FLAGS = -lcairo \
			-lX11-xcb \
			-lX11 \
			-lxcb \
			-lxcb-sync \
			-lm \
			\
			-lpango-1.0 \
			-lpangocairo-1.0 \
			-I/usr/include/pango-1.0 \
			-I/usr/include/cairo \
			-I/usr/include/glib-2.0 \
			-I/usr/lib/x86_64-linux-gnu/glib-2.0/include

DEFAULT_PROG = database_expl
#DEFAULT_PROG = search
#DEFAULT_PROG = save_file_to_pdf

FLAGS = -g #Debug
#FLAGS = -O2 -Wall #Release
#FLAGS = -O2 -pg -Wall #Profile release

all: $(DEFAULT_PROG)

database_expl:
	gcc $(FLAGS) database_expl.c -o bin/database_expl $(DEP_FLAGS)

search:
	gcc $(FLAGS) -o bin/search search.c -lm

save_file_to_pdf:
	gcc $(FLAGS) -o bin/save_file_to_pdf save_file_to_pdf.c -lcairo -lm

