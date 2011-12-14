#include <inc/lib.h>
#include <inc/pincl.h>

int flag[256];

void
truncate_file(const char *path, off_t nsize)
{
	int fd, r;
	struct Stat st;

	//clog("wp1: %s, %d", path, nsize);
	fd = open(path, O_RDWR);
	if (fd < 0) {
		clog("open %s: %e", path, fd);
		exit();
	}
	r = fstat(fd, &st);
	if (r < 0) {
		clog("stat %s: %e", path, r);
		exit();
	}

	if (st.st_isdir) {
		clog("%s: is a directory: %e", path, -E_NOT_SUPP);
		exit();
	}

	r = ftruncate(fd, nsize);
	if (r < 0) {
		clog("truncate %s: %e", path, r);
		exit();
	}

	close(fd);
}

void
usage(void)
{
	printf("usage: truncate -s [new size] [file...]\n");
	exit();
}

void
umain(int argc, char **argv)
{
	int i;
	off_t nsize;

	ARGBEGIN{
	case 's':
		flag[(uint8_t)ARGC()]++;
		break;
	default:
		usage();
	}ARGEND

	if (!flag['s']) {
		usage();
	}

	if (argc <= 1)
		usage();
	else {
		nsize = (off_t) strtol(argv[0], NULL, 0);
		for (i=1; i<argc; i++)
			truncate_file(argv[i], nsize);
	}
}

